use Test::Nginx::Socket 'no_plan';

no_shuffle();
run_tests();

__DATA__

=== TEST 1: footer twice in one location is refused
--- config
    location /t {
        footer "a";
        footer "b";
        return 200 "body";
    }
--- must_die
--- error_log
is duplicate



=== TEST 2: footer without an argument is refused
--- config
    location /t {
        footer;
        return 200 "body";
    }
--- must_die
--- error_log
invalid number of arguments



=== TEST 3: footer_types without an argument is refused
--- config
    location /t {
        footer_types;
        return 200 "body";
    }
--- must_die
--- error_log
invalid number of arguments
