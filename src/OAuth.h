#ifndef OAUTH_H
#define OAUTH_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define OAUTH_VERSION "1.0.0"

#define FOREACH_REQUEST(REQUEST) \
        REQUEST(POST)       \
        REQUEST(PUT)        \
        REQUEST(GET)        \
        REQUEST(PATCH)      \
        REQUEST(DELETE)     \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum REQUEST {
    FOREACH_REQUEST(GENERATE_ENUM)
} REQUEST;

static const char *REQUEST_STRING[] = {
    FOREACH_REQUEST(GENERATE_STRING)
};

typedef struct response_data {
    char* data;
    char* header;
} response_data;

typedef struct OAuth OAuth;

OAuth* oauth_create();
void oauth_delete(OAuth* oauth);

bool oauth_gen_challenge(OAuth* oauth);
char* oauth_auth_url(OAuth* oauth);
void oauth_auth(OAuth* oauth, const char* code);

bool oauth_start_refresh(OAuth* oauth, uint64_t ms);
bool oauth_stop_refresh(OAuth* oauth);

void oauth_append_header(OAuth* oauth, const char* key, const char* value);
void oauth_append_data(OAuth* oauth, const char* key, const char* value);
void oauth_set_param(OAuth* oauth, const char* key, char* value);

void oauth_start_request_thread(OAuth* oauth);
void oauth_stop_request_thread(OAuth* oauth);
response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, bool cache, bool auth);

bool oauth_load(OAuth* oauth, const char* dir, const char* name);
bool oauth_save(OAuth* oauth, const char* dir, const char* name);

#endif