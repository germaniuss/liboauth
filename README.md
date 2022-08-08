# anisync-oauth
### Minimal C oauth 2.0 authentication ilbrary using libcurl and libmicrohttpd

This library is insipred by https://github.com/slugonamission/OAuth2
but aims to add extra missing functionality like handle browser opening/code retrieval,
access token refresh, state saving, etc.

### Working functionality

- [x] Syncronous API requests with request caching.
- [x] Simple "All In One" Oauth 2.0 authentication.
- [x] Supports all major API request types (PUT, PATCH, POST, GET, DELETE...)
- [x] State saving of "tokens" and other variables (simple .yaml file)
- [x] Auto refresh "access token" with zero overhead for the user.

### Missing functionality

- [ ] Handling on auth callback different than
application/json (AKA XML)

- [ ] Better handling on browser opening and
code retrieval with URI schemes (may keep the old
method after implementation)

- [ ] Add async request support for the same oauth
module with the same client_id. This may not work on some
APIs since they might limit incoming trafic but will be
usefull for many others.

### Next steps

Add error codes to be returned when something fails.

Load file from .ini and add header section.

Simplification of the codebase for tighter
integration of the libraries used as well as general
bugfixing. 

Standarization of used libraries and compliance with C99

### Contributing

Any contribution is welcome and should be done through a pull request.

### Cretit

Credit goes to:<br>
Rafa Garcia from https://github.com/rafagafe/tiny-json<br>
Ozan Tezcan from https://github.com/tezc/sc<br>
Rodion Efremov from https://github.com/coderodde/coderodde.c.utils<br>
for their amazing libraries.
