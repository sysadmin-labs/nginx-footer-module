#!/bin/sh
#
# Drives the module against a running nginx and checks what the header says
# against what the body delivers.
#
# usage: ci/footer.sh <nginx binary>
#
# Set FOOTER_LOAD_MODULE to the path of ngx_http_footer_filter_module.so to
# test a loadable build.

set -eu

NGINX=${1:?path to the nginx binary missing}
WORK=${CI_WORK:-$(cd "$(dirname "$0")/.." && pwd)/ci-work}/footer
PORT=${FOOTER_PORT:-18910}

command -v curl > /dev/null || { echo "curl(1) is needed" >&2; exit 2; }

rm -rf "$WORK"
mkdir -p "$WORK/conf" "$WORK/logs" "$WORK/html"

fail=0
note() {
	echo "  $1" >&2
	fail=1
}

printf 'BODY' > "$WORK/html/a.html"
printf 'BODY' > "$WORK/html/a.txt"
# larger than one output buffer, so the body filter runs more than once and
# a footer appended per pass would show up twice
dd if=/dev/zero bs=1024 count=400 2>/dev/null | tr '\0' 'x' > "$WORK/html/big.html"

load=
for so in ${FOOTER_LOAD_MODULE:-}; do
	load="$load
load_module $so;"
done

cat > "$WORK/conf/nginx.conf" <<CONF
$load
daemon on;
master_process on;
worker_processes 1;
pid $WORK/logs/nginx.pid;
error_log $WORK/logs/error.log info;
events { worker_connections 64; }

http {
    access_log off;
    default_type text/html;
    output_buffers 2 4k;

    server {
        listen 127.0.0.1:$PORT;
        root $WORK/html;

        location /plain/   { alias $WORK/html/; footer "[F]"; }
        location /var/     { alias $WORK/html/; footer "[\$request_method]"; }
        location /json/    { default_type application/json; footer "[F]"; return 200 "body"; }
        location /jsontype/ { default_type application/json; footer_types application/json; footer "[F]"; return 200 "body"; }
        location /off/     { alias $WORK/html/; footer "[F]"; }
        location /off/sub/ { alias $WORK/html/; footer ""; }
        location /none/    { alias $WORK/html/; }
    }
}
CONF

: > "$WORK/logs/error.log"
"$NGINX" -c "$WORK/conf/nginx.conf"
trap '"$NGINX" -c "$WORK/conf/nginx.conf" -s stop 2>/dev/null || true' EXIT
sleep 1

body() { curl -s --max-time 10 "http://127.0.0.1:$PORT$1"; }
header() {
	curl -s -o /dev/null -D - --max-time 10 "http://127.0.0.1:$PORT$1" \
	  | tr -d '\r' | grep -i "^$2:" | cut -d' ' -f2-
}
head_header() {
	curl -s -I --max-time 10 "http://127.0.0.1:$PORT$1" \
	  | tr -d '\r' | grep -i "^$2:" | cut -d' ' -f2-
}

echo "== the footer is appended"
out=$(body /plain/a.html)
echo "  $out"
[ "$out" = "BODY[F]" ] || note "expected BODY[F], got $out"

echo "== a variable is expanded"
out=$(body /var/a.html)
echo "  $out"
[ "$out" = "BODY[GET]" ] || note "expected BODY[GET], got $out"

echo "== footer_types decides which content types are touched"
j=$(body /json/); jt=$(body /jsontype/)
echo "  application/json, not listed: $j"
echo "  application/json, listed:     $jt"
[ "$j" = "body" ] || note "a type outside footer_types has to stay untouched, got $j"
[ "$jt" = "body[F]" ] || note "expected body[F] for a listed type, got $jt"

echo "== an empty footer turns it off below a location that sets one"
a=$(body /off/a.html); b=$(body /off/sub/a.html)
echo "  above: $a"
echo "  below: $b"
[ "$a" = "BODY[F]" ] || note "expected BODY[F] above, got $a"
[ "$b" = "BODY" ] || note "the empty footer has to switch it off, got $b"

echo "== without the directive nothing is added"
out=$(body /none/a.html)
echo "  $out"
[ "$out" = "BODY" ] || note "expected BODY, got $out"

echo "== the length a HEAD announces is the one a GET delivers"
g=$(header /plain/a.html content-length)
hd=$(head_header /plain/a.html content-length)
n=$(body /plain/a.html | wc -c | tr -d ' ')
echo "  GET says $g, HEAD says $hd, the body is $n bytes"
[ "$g" = "$n" ] || note "GET announced $g for $n bytes"
[ "$hd" = "$n" ] || note "HEAD announced $hd for a body of $n bytes"

echo "== an entity tag is weak once the body is not the file any more"
with=$(header /plain/a.html etag)
without=$(header /none/a.html etag)
echo "  with a footer:    $with"
echo "  without a footer: $without"
case "$with" in
    W/*) ;;
    *) note "the entity tag has to be weak when a footer is appended, got $with" ;;
esac
case "$without" in
    W/*) note "nothing was appended here, the entity tag has to stay strong, got $without" ;;
esac

echo "== a body spanning several buffers gets one footer, not one per pass"
n=$(body /plain/big.html | grep -o '\[F\]' | wc -l | tr -d ' ')
echo "  $n"
[ "$n" = "1" ] || note "expected exactly one footer, counted $n"

echo "== error log"
if grep -qE '\[(alert|emerg)\]' "$WORK/logs/error.log"; then
	grep -E '\[(alert|emerg)\]' "$WORK/logs/error.log" | head -5
	note "alert or emerg in the error log"
else
	echo "  clean"
fi

[ "$fail" = 0 ] || { echo "$0: failed" >&2; exit 1; }
echo "$0: ok"
