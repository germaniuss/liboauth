#ifndef OAUTH_H
#define OAUTH_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define OAUTH_VERSION "1.1.0"

typedef enum REQUEST {
    POST, PUT, GET, PATCH, DEL
} REQUEST;

static const char *REQUEST_STRING[] = {
    "POST", "PUT", "GET", "PATCH", "DELETE"
};

typedef struct response_data {
    char* data;
    char* header;
    int error_code;
} response_data;

typedef struct OAuth OAuth;

OAuth* oauth_create();
void oauth_delete(OAuth* oauth);

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

void oauth_config_dir(OAuth* oauth, const char* dir, const char* name);
bool oauth_load(OAuth* oauth);
bool oauth_save(OAuth* oauth);

#endif