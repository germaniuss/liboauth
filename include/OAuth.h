#ifndef OAUTH_H
#define OAUTH_H

#include <utils/hash.h>
#include <utils/str.h>
#include <utils/time.h>
#include <utils/json.h>
#include <utils/ini.h>
#include <utils/thread.h>
#include <utils/sorted_map.h>
#include <utils/map.h>
#include <utils/timer.h>
#include <utils/path.h>

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define NUM_PARAMS 21

typedef enum PARAM {
    SAVE_ON_OAUTH,
    SAVE_ON_CLOSE,
    REFRESH_ON_OAUTH,
    REFRESH_ON_LOAD,
    REQUEST_ON_LOAD,
    CLIENT_ID,
    CLIENT_SECRET,
    CHALLENGE_METHOD,
    REDIRECT_URI,
    TOKEN_URL,
    AUTH_URL, 
    CONFIG_FILE,
    CACHE_FILE,
    REQUEST_TIMEOUT,
    ACCESS_TOKEN,
    TOKEN_BEARER,
    REFRESH_TOKEN,
    CODE_CHALLENGE,
    CODE_VERIFIER,
    REQUEST_QUEUE_SIZE,
    CACHE_SIZE
} PARAM;

static const char* PARAM_STRING[] = {
    "save_on_auth",
    "save_on_close",
    "refresh_on_auth",
    "refresh_on_load",
    "request_on_load",
    "client_id",
    "client_secret",
    "challenge_method",
    "redirect_uri",
    "token_url",
    "auth_url", 
    "config_file",
    "cache_file",
    "request_timeout",
    "access_token",
    "token_bearer",
    "refresh_token",
    "code_challenge",
    "code_verifier",
    "request_queue_size",
    "cache_size"
};

// THE OPTIONS MAY BE CHANGED TEMPORARILY FOR A REQUEST
typedef enum OPTION {
    REQUEST_CACHE = 1, 
    REQUEST_ASYNC = 2,
    REQUEST_AUTH = 4
} OPTION;

static const char* OPTION_STRING[] = {
    "request_cache",
    "request_async",
    "request_auth"
};

typedef enum REQUEST {
    POST, PUT, GET, PATCH, DEL
} REQUEST;

static const char *REQUEST_STRING[] = {
    "POST", "PUT", "GET", "PATCH", "DELETE"
};

typedef struct response_data {
    const char* data;
    const char* content_type;
    long response_code;
} response_data;

typedef struct request_data {
    const char* data;
    struct curl_slist *header;
    REQUEST method;
    const char* endpoint;
    const char* id;
} request_data;

typedef struct OAuth OAuth;

OAuth* oauth_create(const char* config_file);
void oauth_delete(OAuth* oauth);

char* oauth_auth_url(OAuth* oauth);
void oauth_auth(OAuth* oauth, const char* code);

bool oauth_start_refresh(OAuth* oauth, uint64_t ms);
bool oauth_stop_refresh(OAuth* oauth);

void oauth_append_header(OAuth* oauth, const char* key, const char* value);
void oauth_append_data(OAuth* oauth, const char* key, const char* value);
void oauth_set_param(OAuth* oauth, PARAM param, char* value);
bool oauth_set_options(OAuth* oauth, uint8_t options);

void oauth_start_request_thread(OAuth* oauth);
void oauth_stop_request_thread(OAuth* oauth);
response_data oauth_request(OAuth* oauth, REQUEST method, const char* endpoint);

bool oauth_load(OAuth* oauth);
bool oauth_save(OAuth* oauth);

#endif