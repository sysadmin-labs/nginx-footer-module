# Changelog

## v0.1.0 (2026-10-31)

- First release.  `footer` appends text, which may be built from variables,
  to a response body, and `footer_types` says which content types it applies
  to.  The directives are the ones `alibaba/nginx-http-footer-filter` has
  carried since 2010, so a configuration written for it keeps working.
- `Content-Length` counts the footer for a `HEAD` request as well, so the
  length announced there is the one a `GET` delivers.
- A strong entity tag is weakened once a footer is appended, because the
  body is no longer the file the tag identifies.
- The footer is appended once, not once per pass of the body filter, which
  is called again with the same chain whenever the filter below it could not
  take all of it.
- Builds as a loadable object as well as into the binary.
