# liboauth
### Minimal C oauth 2.0 authentication ilbrary using libcurl

This library is insipred by https://github.com/slugonamission/OAuth2
but aims to add extra missing functionality like access token refresh, 
state saving, response caching, etc.
The project is now working but do expect bugs
and breaking API changes while the project matures.

Note that for the full functionality it is expected you use some other library
to extract the code from the redirect uri. It is recommended to use my scheme
registering and handling library however, other awesome options exist.

[libschemehandler](https://github.com/germaniuss/libschemehandler): Lightweight scheme registering and handling library with no extra dependencies (<b>LICENSE MIT</b>)
[Ultralight](https://ultralig.ht/): Lightweight and powerfull embeded browser library (formerly awesomium) (<b>TRIPLE LICENSE</b>)
[libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/): Lightweight and simple HTTP server library written in ansi C and perfect for embedded systems (<b>LICENSE GPL-3</b>)

### Working functionality

- [x] Syncronous API requests with request caching.
- [x] Compatibility with my URI scheme handler.
- [x] Supports all major API request types (PUT, PATCH, POST, GET, DELETE...)
- [x] State saving of "tokens" and other variables (simple .ini file)
- [x] Auto refresh "access token" with zero overhead for the user.
- [x] Cross-compatibility with Windows/Apple/Linux
- [x] The response code and content type (as well as the response data) are returned.

### Missing functionality

- [ ] Handling on auth callback different than
application/json (AKA XML)

- [ ] Add async request support for the same oauth
module with the same client_id. This may not work on some
APIs since they might limit incoming trafic but will be
usefull for many others.

- [ ] <b>IMPORTANT</b> Add cache saving between sessions. Add limit to
cache size to avoid overloading memory.

- [ ] Simplification of the codebase for tighter
integration of the libraries used as well as general
bugfixing (remove all unnecessary libraries). 

### Contributing

Any contribution is welcome and should be done through a pull request. Currently
help is mostly needed with request error handling and documentation.

### Cretit

Credit goes to:<br>
Rafa Garcia from https://github.com/rafagafe/tiny-json<br>
Ozan Tezcan from https://github.com/tezc/sc<br>
Rodion Efremov from https://github.com/coderodde/coderodde.c.utils<br>
for their amazing libraries.
