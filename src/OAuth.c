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
    oauth->request_queue = unordered_map_alloc(0, 0, unordered_map_hash_str, unordered_map_streq);
    mutex_init(&oauth->request_mutex);
    oauth->params = map_alloc(strcmp);
    oauth->data = NULL;
    oauth->header = NULL;
    oauth->cache = unordered_map_alloc(0, 0, unordered_map_hash_str, unordered_map_streq);
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    timer_term(&oauth->refresh_timer);
    unordered_map_free(oauth->request_queue);
    mutex_term(&oauth->request_mutex);
    map_free(oauth->params);
    if (oauth->data) map_free(oauth->data);
    if (oauth->header) map_free(oauth->header);
    if (oauth->code_challenge) str_destroy(&oauth->code_challenge);
    if (oauth->code_verifier) str_destroy(&oauth->code_verifier);
    unordered_map_free(oauth->cache);
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
    if (!map_get(oauth->params, "refresh_token") || !map_get(oauth->params, "client_id") || !map_get(oauth->params, "base_token_url"))
        return false;
    timer_start(&oauth->refresh_timer, ms, oauth_refresh_task, oauth);
    return true;
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
        while (unordered_map_empty(oauth->request_queue) && oauth->request_run);
        if (!oauth->request_run) break;
        mutex_lock(&oauth->request_mutex);
        request_data* rq_data = unordered_map_first(oauth->request_queue);
        unordered_map_remove(oauth->request_queue, rq_data->id);
        response_data* response = request(rq_data->method, rq_data->endpoint, rq_data->header, rq_data->data);
        unordered_map_put(oauth->cache, rq_data->id, response);
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
    if (cache && (response = unordered_map_get(oauth->cache, rq_data->id))) { 
        if (!unordered_map_contains_key(oauth->request_queue, rq_data->id))
            unordered_map_put(oauth->request_queue, rq_data->id, rq_data);
    } else  {
        mutex_lock(&oauth->request_mutex);
        response = request(method, endpoint, rq_data->header, rq_data->data);
        if (cache) unordered_map_put(oauth->cache, rq_data->id, response);
        mutex_unlock(&oauth->request_mutex);
    } return response;
}

bool oauth_load(OAuth* oauth, const char* dir, const char* name) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    const char *token;

    char* full_dir = NULL;
    if (dir[0] != '\0') str_append(&full_dir, "/");
    str_append_fmt(&full_dir, "%s.yaml", name);

    FILE * fp = fopen(full_dir, "r");
    str_destroy(&full_dir);
    if (fp == NULL) return NULL;

    while (getline(&line, &len, fp) != -1) {
        char* save = NULL;
        char* lenline = str_create(line);
        token = str_token_begin(lenline, &save, ":");
        char* key_token = str_create(token);
        str_trim(&key_token, " \n\t\r");
        token = str_token_begin(lenline, &save, "");
        char* value_token = str_create(token);
        str_trim(&value_token, " \n\t\r");
        map_put(oauth->params, key_token, value_token);
        str_destroy(&lenline);
    }

    fclose(fp);
    if (line) free(line);
    oauth_refresh(oauth, 0);
    return oauth;
}

bool oauth_save(OAuth* oauth, const char* dir, const char* name) {
    char* full_dir = NULL;
    if (dir[0] != '\0') str_append(full_dir, "/");
    str_append_fmt(&full_dir, "%s.yaml", name);

    FILE *fp = fopen(full_dir, "w");
    str_destroy(&full_dir);
    if (fp == NULL) {
        printf("Error opening the file %s", dir);
        return NULL;
    }

    // write to the text file
    map_iterator* iter = map_iterator_alloc(oauth->params);
    const char* key; const char* value;
    while (map_iterator_has_next(iter)) {
        map_iterator_next(iter, &key, &value);
        fprintf(fp, "%s: %s\n", key, value);
    }
        
    // close the file
    fclose(fp);
    return 1;
}

int main() {
    srand(time_ms());
    OAuth* oauth = oauth_create();
    oauth_load(oauth, "", "TEST");
    oauth_delete(oauth);
}