# nginx-footer-module

[![Build & Test][build-test-badge]][build-test-link]
[![Footer][footer-badge]][footer-link]
[![FreeBSD][freebsd-badge]][freebsd-link]
[![A/UBSan][sanitizers-badge]][sanitizers-link]
[![Valgrind][valgrind-badge]][valgrind-link]
[![Reload][reload-badge]][reload-link]
[![CodeQL][codeql-badge]][codeql-link]
[![Lint][lint-badge]][lint-link]

[build-test-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/build-test.yml/badge.svg
[build-test-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/build-test.yml
[footer-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/footer.yml/badge.svg
[footer-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/footer.yml
[freebsd-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/freebsd.yml/badge.svg
[freebsd-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/freebsd.yml
[sanitizers-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/sanitizers.yml/badge.svg
[sanitizers-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/sanitizers.yml
[valgrind-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/valgrind.yml/badge.svg
[valgrind-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/valgrind.yml
[reload-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/reload.yml/badge.svg
[reload-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/reload.yml
[codeql-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/codeql.yml/badge.svg
[codeql-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/codeql.yml
[lint-badge]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/lint.yml/badge.svg
[lint-link]: https://github.com/sysadmin-labs/nginx-footer-module/actions/workflows/lint.yml

Appends a piece of text to a response body, which may be built from
variables.

`add_after_body` in nginx's own addition module does something similar, but
it fetches the text with a subrequest, so it needs a location to serve it
from.  This module takes the text from the directive.

## Synopsis

```nginx
http {
    footer_types text/html application/xhtml+xml;

    server {
        location / {
            footer "<!-- served by $hostname at $time_iso8601 -->";
        }

        location /api/ {
            # nothing is appended here
            footer "";
        }
    }
}
```

## Directives

### footer

**syntax:** `footer <text>`
**default:** none
**context:** `http`, `server`, `location`

The text to append.  Variables are expanded.  An empty argument switches the
module off for that context, which is how a location below one that sets a
footer opts out; leaving the directive out inherits instead.

A value that expands to nothing appends nothing, and the response is passed
through untouched.

### footer_types

**syntax:** `footer_types <mime type> ...`
**default:** `text/html`
**context:** `http`, `server`, `location`

Which content types get a footer.  As everywhere else in nginx, `text/html`
is always included, so naming further types adds to it rather than replacing
it.

## What it does to the response header

A body that has grown is no longer the file it was read from, and the header
is corrected accordingly:

- `Content-Length` counts the footer, for a `HEAD` request as well, so the
  length announced there is the one a `GET` would deliver.
- `Accept-Ranges` is dropped, because a range of the original body no longer
  addresses what is sent.
- A strong entity tag is weakened to `W/"..."`, because the body differs from
  what that tag identifies.  A cache that revalidates on it would otherwise
  serve the variant without the footer.

Nothing is appended to a subrequest, to a `204` or a `304`, or to a response
whose content type is not in `footer_types`.

## Building

```sh
./configure --add-module=/path/to/nginx-footer-module
```

or as a loadable object:

```sh
./configure --add-dynamic-module=/path/to/nginx-footer-module
```

```nginx
load_module modules/ngx_http_footer_filter_module.so;
```

## Compatibility

The directives are the ones `alibaba/nginx-http-footer-filter` has carried
since 2010, so a configuration written for that module keeps working.  This
is a reimplementation, not a fork: that module's last change is from 2012,
its repository is archived, and its `config` predates loadable modules.

## License

BSD-2-Clause, see [LICENSE](LICENSE).
