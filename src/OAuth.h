#include "utils.h"
#include <pthread.h>

typedef struct OAuth {
    pthread_t refresh_thread;
    ht* params;
    ht* header;
    ht* data;
    char* code_verifier;
    char* code_challenge;
    bool authed;
} OAuth;

OAuth* oauth_create();
void oauth_delete(OAuth* oauth);

bool oauth_gen_challenge(OAuth* oauth);
enum MHD_Result oauth_get_code (
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method, const char *version,
    const char *upload_data,
    size_t *upload_data_size, void **con_cls);
char* oauth_auth_url(OAuth* oauth);
response_data* oauth_post_token(OAuth* oauth, const char* code);

bool oauth_start(OAuth* oauth);
bool oauth_refresh(OAuth* oauth, uint32_t sec);

void oauth_set_header(OAuth* oauth, ht* header);
void oauth_set_data(OAuth* oauth, ht* data);
void oauth_set_param(OAuth* oauth, const char* key, char* value);
response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, ht* header, ht* data);

bool oauth_load(OAuth* oauth, const char* dir, const char* name);
bool oauth_save(OAuth* oauth, const char* dir, const char* name);