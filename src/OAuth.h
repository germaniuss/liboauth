#include "utils.h"
#include "mutex.h"
#include "thread.h"
#include "unordered_map.h"
#include "timer.h"

typedef struct OAuth {
    bool request_run;
    unordered_map* request_queue;
    struct thread request_thread;
    struct mutex request_mutex;
    uint64_t request_timeout;
    unordered_map* cache;
    struct timer refresh_timer;
    map* params;
    map* header;
    map* data;
    char* code_verifier;
    char* code_challenge;
    bool authed;
    uint64_t port;
    uint64_t listen_timeout;
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
bool oauth_refresh(OAuth* oauth, uint64_t ms);

void oauth_set_header(OAuth* oauth, map* header);
void oauth_set_data(OAuth* oauth, map* data);
void oauth_set_param(OAuth* oauth, const char* key, char* value);

void oauth_start_requests(OAuth* oauth);
void oauth_stop_requests(OAuth* oauth);
response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, bool cache, map* data, map* header);

bool oauth_load(OAuth* oauth, const char* dir, const char* name);
bool oauth_save(OAuth* oauth, const char* dir, const char* name);