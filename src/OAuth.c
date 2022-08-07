#include "OAuth.h"
#include "base64.h"
#include "sha-256.h"
#include "thread.h"
#include "str.h"
#include "time.h"
#include "json.h"

enum MHD_Result oauth_get_code (
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method, const char *version,
    const char *upload_data,
    size_t *upload_data_size, void **con_cls) 
{
    OAuth* oauth = (OAuth*) cls;
    response_data* token_response = oauth_post_token(oauth, 
        MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code")); 

    enum { MAX_FIELDS = 512 };
    json_t pool[ MAX_FIELDS ];
    const json_t* json = json_create(token_response->data, pool, MAX_FIELDS);
    map_put(oauth->params, "token_type", str_create(json_value(json, "token_type").string));
    map_put(oauth->params, "access_token", str_create(json_value(json, "access_token").string));
    map_put(oauth->params, "refresh_token", str_create(json_value(json, "refresh_token").string));

    oauth_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);

    char* str = NULL;
    str_append_fmt(&str, "%s %s", str_dup(map_get(oauth->params, "token_type")), str_dup(map_get(oauth->params, "access_token")));
    map_put(oauth->header, "Authorization", str);
    str_destroy(&str);

    oauth->authed = true;

    const char *page  = "<HTML><HEAD>Return to the app now.</BODY></HTML>";
    struct MHD_Response *response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

OAuth* oauth_create() {
    OAuth* oauth = (OAuth*) malloc(sizeof(OAuth));
    oauth->request_run = false;
    oauth->request_timeout = 250;
    oauth->port = 8000;
    oauth->listen_timeout = 30;
    timer_init(&oauth->refresh_timer);
    queue_init(&oauth->request_queue);
    mutex_init(&oauth->request_mutex);
    oauth->params = map_alloc(strcmp);
    oauth->data = NULL;
    oauth->header = NULL;
    unordered_map_init_sv(&oauth->cache, 0, 0);
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    timer_term(&oauth->refresh_timer);
    queue_free(&oauth->request_queue);
    mutex_term(&oauth->request_mutex);
    map_free(oauth->params);
    if (oauth->data) map_free(oauth->data);
    if (oauth->header) map_free(oauth->header);
    if (oauth->code_challenge) str_destroy(&oauth->code_challenge);
    if (oauth->code_verifier) str_destroy(&oauth->code_verifier);
    unordered_map_term_sv(&oauth->cache);
}

bool oauth_start(OAuth* oauth) {

    if (!map_get(oauth->params, "base_auth_url")  || 
        !map_get(oauth->params, "base_token_url") || 
        !map_get(oauth->params, "client_id")
    ) return false;

    oauth_gen_challenge(oauth);
    struct thread th;
    request_params params;
    params.port = oauth->port;
    params.sec = oauth->listen_timeout;
    params.data = oauth;
    params.request_callback = &oauth_get_code;
    char* url = oauth_auth_url(oauth);
    thread_init(&th);
    thread_start(&th, acceptSingleRequest, (void*) &params);
    openBrowser(url);
    str_destroy(&url);
    thread_join(&th, NULL);
    return true;
}

void* oauth_refresh_task(void* in) {
    OAuth* oauth = (OAuth*) in;

    if (!map_get(oauth->params, "refresh_token") || !map_get(oauth->params, "client_id") || !map_get(oauth->params, "base_token_url"))
        return false;

    map* data = map_alloc(strcmp);

    map_put(data, "client_id", str_dup(map_get(oauth->params, "client_id")));
    map_put(data, "refresh_token", str_dup(map_get(oauth->params, "refresh_token")));
    map_put(data, "grant_type", str_create("refresh_token"));
    if (map_contains_key(oauth->params, "client_secret"))
        map_put(data, "client_secret", str_create(map_get(oauth->params, "client_secret")));

    response_data* response = oauth_request(oauth, POST, map_get(oauth->params, "base_token_url"), false, data, NULL);

    enum { MAX_FIELDS = 512 };
    json_t pool[ MAX_FIELDS ];
    const json_t* json = json_create(response->data, pool, MAX_FIELDS);
    map_put(oauth->params, "token_type", str_create(json_value(json, "token_type").string));
    map_put(oauth->params, "access_token", str_create(json_value(json, "access_token").string));
    map_put(oauth->params, "refresh_token", str_create(json_value(json, "refresh_token").string));

    oauth_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);

    map_free(data);
}

bool oauth_refresh(OAuth* oauth, uint64_t ms) {
    timer_start(&oauth->refresh_timer, ms, oauth_refresh_task, oauth);
}

bool oauth_gen_challenge(OAuth* oauth) {
    
    if (!map_get(oauth->params, "code_challenge_method"))
        return false;

    if (strcmp(map_get(oauth->params, "code_challenge_method"), "plain") == 0) {
        oauth->code_verifier = str_create(base64_url_random(128));
        oauth->code_challenge = str_dup(oauth->code_verifier);
        return true;
    } else if (strcmp(map_get(oauth->params, "code_challenge_method"), "S256") == 0) {
        uint8_t hash[32];
        char hash_string[65];
        oauth->code_verifier = str_create(base64_url_random(32));
        calc_sha_256(hash, oauth->code_verifier, str_len(oauth->code_verifier));
        hash_to_string(hash_string, hash);
        oauth->code_challenge = str_create(base64_url_encode(hash_string));
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth) {

    if (!map_get(oauth->params, "base_auth_url") || !map_get(oauth->params, "client_id"))
        return NULL;

    map* data = map_alloc(strcmp);

    map_put(data, "client_id", str_dup(map_get(oauth->params, "client_id")));
    map_put(data, "response_type", str_create("code"));
    if (map_contains_key(oauth->params, "redirect_uri"))
        map_put(data, "redirect_uri", str_create(map_get(oauth->params, "redirect_uri")));
    if (map_contains_key(oauth->params, "code_challenge_method")) {
        map_put(data, "code_challenge_method", str_dup(map_get(oauth->params, "code_challenge_method")));
        map_put(data, "code_challenge", str_dup(oauth->code_challenge));
    }
    
    char* data_str = parse_data(data, "\\&");
    char* authURL = str_dup(map_get(oauth->params, "base_auth_url"));
    if (data_str != NULL) {
        str_append(&authURL, "?");
        str_append(&authURL, data_str);
    }; str_destroy(&data_str);

    map_free(data);
    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* code) {

    if (!map_get(oauth->params, "base_token_url") || code == NULL)
        return NULL;

    map* data = map_alloc(strcmp);

    if (map_contains_key(oauth->params, "client_secret")) 
        map_put(data, "client_secret", str_dup(map_get(oauth->params, "client_secret")));
    if (map_contains_key(oauth->params, "redirect_uri")) 
        map_put(data, "redirect_uri", str_dup(map_get(oauth->params, "redirect_uri")));
    if (oauth->code_verifier != 0)
        map_put(data, "code_verifier", str_dup(oauth->code_verifier));

    map_put(data, "code", str_create(code));
    map_put(data, "client_id", str_dup(map_get(oauth->params, "client_id")));
    map_put(data, "grant_type", str_create("authorization_code"));

    response_data* response = oauth_request(oauth, POST, map_get(oauth->params, "base_token_url"), false, data, NULL);
    map_free(data);
    return response;
}

void oauth_set_param(OAuth* oauth, const char* key, char* value) {
    map_put(oauth->params, (void*) key, (void*) value);
}

void oauth_set_header(OAuth* oauth, map* header) {
    map_free(oauth->header);
    oauth->header = header;
}

void oauth_set_data(OAuth* oauth, map* data) {
    map_free(oauth->data);
    oauth->data = data;
}

void* oauth_process_request(void* data) {
    OAuth* oauth = (OAuth*) data;
    while (oauth->request_run) {
        while (queue_empty(&oauth->request_queue) && oauth->request_run);
        if (!oauth->request_run) break;
        mutex_lock(&oauth->request_mutex);
        request_data* rq_data = queue_peek_first(&oauth->request_queue);
        queue_del_first(&oauth->request_queue);
        response_data* response = request(rq_data->method, rq_data->endpoint, rq_data->header, rq_data->data);
        unordered_map_put_sv(&oauth->cache, rq_data->id, response);
        time_sleep(oauth->request_timeout);
        mutex_unlock(&oauth->request_mutex);
    }
}

void oauth_start_requests(OAuth* oauth) {
    oauth->request_run = true;
    thread_init(&oauth->request_thread);
    thread_start(&oauth->request_thread, oauth_process_request, oauth);
}

void oauth_stop_requests(OAuth* oauth) {
    oauth->request_run = false;
    thread_term(&oauth->request_thread);
}

response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, bool cache, map* data, map* header) {
    if (!data && !header) {
        data = oauth->data;
        header = oauth->header;
    }

    request_data* rq_data = (request_data*) malloc(sizeof(request_data));
    rq_data->id = NULL;
    rq_data->data = parse_data(data, "&");
    rq_data->endpoint = endpoint;
    rq_data->header = parse_header(header);
    rq_data->method = method;
    str_append_fmt(&rq_data->id, "%s?%s", rq_data->endpoint, rq_data->data);

    response_data* response;
    if (cache && (response = unordered_map_get_sv(&oauth->cache, rq_data->id))) { 
        bool found = false;
        for (int i = 0; i < queue_size(&oauth->request_queue); i++) {
            request_data* in_queue = queue_at(&oauth->request_queue, i);
            if (str_cmp(in_queue->id, rq_data->id)) {
                found = true; 
                break;
            }
        } if (!found) queue_add_last(&oauth->request_queue, rq_data);
    } else  {
        mutex_lock(&oauth->request_mutex);
        response = request(method, endpoint, rq_data->header, rq_data->data);
        if (cache) unordered_map_put_sv(&oauth->cache, rq_data->id, response);
        mutex_unlock(&oauth->request_mutex);
    } return response;
}

bool oauth_load(OAuth* oauth, const char* dir, const char* name) {
    return false;
}

bool oauth_save(OAuth* oauth, const char* dir, const char* name) {
    return false;
}

int main() {
    srand(time_ms(NULL));
    OAuth* oauth = oauth_create();
    oauth_set_param(oauth, "client_id", str_create("ed7f347e239153101c9e6fc6b5bdfece"));
    oauth_set_param(oauth, "base_auth_url", str_create("https://myanimelist.net/v1/oauth2/authorize"));
    oauth_set_param(oauth, "base_token_url", str_create("https://myanimelist.net/v1/oauth2/token"));
    oauth_set_param(oauth, "code_challenge_method", str_create("plain"));
    oauth_start(oauth);
    // oauth_load(oauth, "", "TEST");
    // oauth_refresh(oauth, 15);
    while(true);
    // oauth_start(oauth);
    oauth_save(oauth, "", "TEST");
    oauth_delete(oauth);
}