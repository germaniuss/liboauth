#pragma once
#include <curl/curl.h>
#include "utils.h"

typedef struct OAuth {
    char* client_id;
    char* client_secret;
    char* redirect_uri;
    char* code_challenge_method;
    char* code_challenge;
    char* code_verifier;
    char* code;
    char* expires_in;
    char* access_token;
    char* refresh_token;
    char* token_type;
    char* baseAuthURL;
    char* baseTokenURL;
    request_data* header;
    request_data* data;
    int authed;
} OAuth;

OAuth* oauth_create(const char* client_id, const char* client_secret, const char* redirect_uri);
void oauth_delete(OAuth* oauth);

bool oauth_gen_challenge(OAuth* oauth, const char* code_challenge_method);
enum MHD_Result oauth_get_code (
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method, const char *version,
    const char *upload_data,
    size_t *upload_data_size, void **con_cls);
char* oauth_auth_url(OAuth* oauth, const char* baseAuthURL);
response_data* oauth_post_token(OAuth* oauth, const char* baseTokenURL, const char* newcode);

void oauth_start(OAuth* oauth, const char* baseAuthURL, const char* baseTokenURL, const char* code_challenge_method);

void oauth_set_header(OAuth* oauth, request_data* header);
void oauth_set_data(OAuth* oauth, request_data* data);
response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, request_data* header, request_data* data);