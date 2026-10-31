use Test::Nginx::Socket 'no_plan';

no_shuffle();
run_tests();

__DATA__

=== TEST 1: the footer is appended
--- config
    location /t {
        default_type text/html;
        footer "[F]";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body[F]



=== TEST 2: a variable is expanded
--- config
    location /t {
        default_type text/html;
        footer "[$request_method]";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body[GET]



=== TEST 3: without the directive nothing is added
--- config
    location /t {
        default_type text/html;
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body



=== TEST 4: an empty footer turns it off
--- config
    footer "[outer]";
    location /t {
        default_type text/html;
        footer "";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body



=== TEST 5: a footer set above is inherited
--- config
    footer "[outer]";
    location /t {
        default_type text/html;
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body[outer]



=== TEST 6: footer_types keeps other types out
--- config
    location /t {
        default_type application/json;
        footer "[F]";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body



=== TEST 7: footer_types takes the type it is given
--- config
    location /t {
        default_type application/json;
        footer_types application/json;
        footer "[F]";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body[F]



=== TEST 8: an empty footer value adds nothing
--- config
    location /t {
        default_type text/html;
        footer "$http_x_absent";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body



=== TEST 9: 204 carries no body and gets no footer
--- config
    location /t {
        default_type text/html;
        footer "[F]";
        return 204;
    }
--- request
GET /t
--- error_code: 204
--- response_body chop



=== TEST 10: the footer counts towards the announced length
--- config
    location /t {
        default_type text/html;
        footer "[F]";
        return 200 "body";
    }
--- request
GET /t
--- response_body chop
body[F]
--- response_headers
Content-Length: 7



=== TEST 11: a 304 carries no body and no footer
--- config
    location = /a.html {
        default_type text/html;
        footer "[F]";
    }
--- user_files
>>> a.html
body
--- request
GET /a.html
--- more_headers
If-None-Match: *
--- error_code: 304
--- response_body chomp



=== TEST 12: a subrequest gets no footer
--- config
    location /outer {
        default_type text/html;
        ssi on;
        footer "[OUT]";
        return 200 '<!--# include virtual="/inner" -->';
    }
    location /inner {
        default_type text/html;
        footer "[IN]";
        return 200 "X";
    }
--- request
GET /outer
--- response_body chop
X[OUT]
