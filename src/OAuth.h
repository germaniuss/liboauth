#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <curl/curl.h>
#include <microhttpd.h>

#include "map.h"
#include "mutex.h"
#include "thread.h"
#include "unordered_map.h"
#include "timer.h"

#define FOREACH_PLATFORM(PLATFORM) \
        PLATFORM(WINDOWS)   \
        PLATFORM(MACOS)     \
        PLATFORM(LINUX)     \
        PLATFORM(ANDROID)   \
        PLATFORM(IOS)       \

#define FOREACH_REQUEST(REQUEST) \
        REQUEST(POST)       \
        REQUEST(PUT)        \
        REQUEST(GET)        \
        REQUEST(PATCH)      \
        REQUEST(DELETE)     \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum PLATFORM {
    FOREACH_PLATFORM(GENERATE_ENUM)
} PLATFORM;

static const char *PLATFORM_STRING[] = {
    FOREACH_PLATFORM(GENERATE_STRING)
};

typedef enum REQUEST {
    FOREACH_REQUEST(GENERATE_ENUM)
} REQUEST;

static const char *REQUEST_STRING[] = {
    FOREACH_REQUEST(GENERATE_STRING)
};

#if defined(_WIN32) || defined(_WIN64)
static PLATFORM platform = WINDOWS;
#elif defined(TARGET_OS_IPHONE)
static PLATFORM platform = IOS;
#elif defined(TARGET_OS_MAX) || defined(__APPLE__) || defined(__MACH__)
static PLATFORM platform = MACOS;
#elif defined(__ANDROID__)
static PLATFORM platform = ANDROID;
#elif defined(__linux__)
static PLATFORM platform = LINUX;
#endif

typedef struct response_data {
    char* data;
    char* header;
} response_data;

typedef struct OAuth {
    bool request_run;
    unordered_map* request_queue;
    struct thread request_thread;
    struct mutex request_mutex;
    uint64_t request_timeout;
    unordered_map* cache;
    struct timer refresh_timer;
    map* params;
    map* data;
    struct curl_slist* header_slist;
    unordered_map* header_map;
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

void oauth_append_header(OAuth* oauth, const char* key, const char* value);
void oauth_append_data(OAuth* oauth, const char* key, const char* value);
void oauth_set_param(OAuth* oauth, const char* key, char* value);

void oauth_start_request_thread(OAuth* oauth);
void oauth_stop_request_thread(OAuth* oauth);
response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, bool cache, bool auth);

bool oauth_load(OAuth* oauth, const char* dir, const char* name);
bool oauth_save(OAuth* oauth, const char* dir, const char* name);