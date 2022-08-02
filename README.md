# anisync-oauth
Minimal C oauth 2.0 authentication ilbrary using libcurl and libmicrohttpd

This library is heavily insipred by https://github.com/slugonamission/OAuth2
but aims to add extra missing functionality like handle browser opening/code retrieval,
access token refresh, state saving, etc.

Missing functionality

- Correct PKCE code_challenge and code_verifier
generation

- Handling on auth callback different than
application/json

- Better handling on browser opening and
code retrieval (I like the way github
dektop does it)

- Serialization of OAuth and saving/opening
functionality.

- Access token refresh functionality
using the refresh token.

- Simplification of the codebase to
remove the use of tiny-json and
(if possible) libmicrohttpd

Contributing

Any contribution is welcome and should be done through a pull request.