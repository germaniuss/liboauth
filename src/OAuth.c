#include <curl/curl.h>

#include "OAuth.h"
#include "sha-256.h"
#include "str.h"
#include "timex.h"
#include "json.h"
#include "ini.h"
#include "map.h"
#include "mutex.h"
#include "thread.h"
#include "unordered_map.h"
#include "timer.h"
#include "path.h"

#define MAX_BUFFER 2048 //4KB Buffers

typedef struct OAuth {
    bool request_run;
    unordered_map* cache;
    unordered_map* request_queue;
    struct thread request_thread;
    struct mutex request_mutex;
    struct timer refresh_timer;
    struct curl_slist* header_slist;
    map* data;
    map* params;
    char* code_verifier;
    char* code_challenge;
    char* config_file;
    bool authed;
} OAuth;

typedef struct request_data {
    char* data;
    struct curl_slist *header;
    REQUEST method;
    char* endpoint;
    char* id;
} request_data;

typedef struct data_t {
    char d[MAX_BUFFER];
    struct data_t* next;
    int idx;
} data_t;

// THIS IS ALL RELATED TO GET AND HTTPS RESPONSE AND PROCESS THE STRING DATA

data_t* data_create() {
    data_t* data = malloc(sizeof(data_t));
    data->idx = 0;
    data->next = 0;
    return data;
}

void data_clean(data_t* d) {
    data_t* pd;
    while(d) {
        pd = d->next;
        free(d);
        d = pd;
    }
}

char* process_response_data(data_t* storage) {
    data_t* curr_storage;
    int data_len;
    char* retVal;

    data_len = 0;
    curr_storage = storage;
    while(curr_storage) {
        data_len += curr_storage->idx;
        curr_storage = curr_storage->next;
    }
    
    //Allocate storage
    retVal = (char*) malloc(sizeof(char)*data_len + 1);
    
    //Now copy in the data
    data_len = 0;
    curr_storage = storage;
    while(curr_storage) {
        memcpy(retVal+data_len, curr_storage->d, curr_storage->idx);
        data_len += curr_storage->idx;
        curr_storage = curr_storage->next;
    } retVal[data_len] = '\0';

    return retVal;
}

size_t process_response(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t idx;
    size_t max;
    data_t* d;
    data_t* nd = NULL;
    
    d = (data_t*)userdata;

    idx = 0;
    max = nmemb * size;

    //Scan to the correct buffer
    while(d->next != NULL)
        d = d->next;

    //Store the data
    while(idx < max) {
        d->d[d->idx++] = ((char*)ptr)[idx++];
        if(d->idx == MAX_BUFFER) {
            nd = (data_t*) malloc(sizeof(data_t));
            nd->next = NULL;
            nd->idx = 0;
            d->next = nd;
            d = nd;
        }
    }

    return max;
}

// THIS IS ALL RELATED TO HEADER AND DATA

char* parse_data(map* data, const char* data_join) {
    if (!data) return NULL;
    char* key; char* value;  
    char* data_str = NULL;
    map_iterator* data_iter = map_iterator_alloc(data);
    while (map_iterator_has_next(data_iter)) {
        map_iterator_next(data_iter, &key, &value);
        if (data_str != NULL) str_append(&data_str, data_join);
        str_append_fmt(&data_str, "%s=%s", key, value);
    } map_iterator_free(data_iter);
    return data_str;
}

response_data* request(REQUEST method, const char* endpoint, struct curl_slist* header, const char* data) {
    char* new_endpoint = str_create(endpoint);
    data_t* storage = data_create();
    data_t* header_data = data_create();
    response_data* resp_data = (response_data*) malloc(sizeof(response_data));

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (curl) {
        // Set the URL, header and callback function
        if (header != NULL) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, storage);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, header_data);
              
        /* Now specify the POST/DELETE/PUT/PATCH data */
        if (method != GET) { 
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, REQUEST_STRING[method]);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, data);
        } else str_append_fmt(&new_endpoint, "?%s", data);
        
        curl_easy_setopt(curl, CURLOPT_URL, new_endpoint);

        
        /* Perform the request, res will get the return code */
        CURLcode res;
        if((res = curl_easy_perform(curl)) != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            data_clean(storage);
            data_clean(header_data);
            return NULL;
        }

        resp_data->data = process_response_data(storage);
        resp_data->header = process_response_data(header_data);

        /* Check for errors */
        /* always cleanup */
        curl_easy_cleanup(curl);
        data_clean(storage);
        data_clean(header_data);
        str_destroy(&new_endpoint);
    } 
    curl_global_cleanup();
    return resp_data;
}

OAuth* oauth_create() {
    OAuth* oauth = (OAuth*) malloc(sizeof(OAuth));
    oauth->authed = false;
    oauth->request_run = false;
    oauth->request_queue = unordered_map_alloc(0, 0, unordered_map_hash_str, unordered_map_streq);
    mutex_init(&oauth->request_mutex);
    oauth->params = map_alloc(strcmp);
    oauth->data = NULL;
    oauth->header_slist = NULL;
    oauth->cache = unordered_map_alloc(0, 0, unordered_map_hash_str, unordered_map_streq);
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    unordered_map_free(oauth->request_queue);
    mutex_term(&oauth->request_mutex);
    map_free(oauth->params);
    if (oauth->data) map_free(oauth->data);
    if (oauth->code_challenge) str_destroy(&oauth->code_challenge);
    if (oauth->code_verifier) str_destroy(&oauth->code_verifier);
    unordered_map_free(oauth->cache);
    if (oauth->header_slist) curl_slist_free_all(oauth->header_slist);
}

void* oauth_refresh_task(void* in) {
    OAuth* oauth = (OAuth*) in;

    if (!map_get(oauth->params, "refresh_token") || !map_get(oauth->params, "client_id") || !map_get(oauth->params, "base_token_url"))
        return false;

    oauth_append_data(oauth, "client_id", map_get(oauth->params, "client_id"));
    oauth_append_data(oauth, "refresh_token", map_get(oauth->params, "refresh_token"));
    oauth_append_data(oauth, "grant_type", "refresh_token");
    if (map_contains_key(oauth->params, "client_secret"))
        oauth_append_data(oauth, "client_secret", map_get(oauth->params, "client_secret"));

    response_data* response = oauth_request(oauth, POST, map_get(oauth->params, "base_token_url"), false, false);

    enum { MAX_FIELDS = 512 };
    json_t pool[ MAX_FIELDS ];
    const json_t* json = json_create(response->data, pool, MAX_FIELDS);
    oauth_set_param(oauth, "token_type", json_value(json, "token_type").string);
    oauth_set_param(oauth, "access_token", json_value(json, "access_token").string);
    oauth_set_param(oauth, "refresh_token", json_value(json, "refresh_token").string);
    oauth->authed = true;

    if (*(char*) map_get(oauth->params, "save_on_refresh") == 't')
        oauth_save(oauth);

    if (*(char*) map_get(oauth->params, "refresh_on_refresh") == 't')
        oauth_start_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);
}

bool oauth_start_refresh(OAuth* oauth, uint64_t ms) {
    if (!map_get(oauth->params, "refresh_token") || !map_get(oauth->params, "client_id") || !map_get(oauth->params, "base_token_url"))
        return false;

    if (ms == 0) {
        oauth_refresh_task(oauth);
        return true;
    }

    if (!oauth->refresh_timer.init) timer_init(&oauth->refresh_timer);
    timer_start(&oauth->refresh_timer, ms, oauth_refresh_task, oauth);
    return true;
}

bool oauth_stop_refresh(OAuth* oauth) {
    timer_term(&oauth->refresh_timer);
}

bool oauth_gen_challenge(OAuth* oauth) {
    
    if (!map_get(oauth->params, "code_challenge_method"))
        return false;

    if (strcmp(map_get(oauth->params, "code_challenge_method"), "plain") == 0) {
        oauth->code_verifier = str_create_random(128);
        oauth->code_challenge = str_create(oauth->code_verifier);
        return true;
    } else if (strcmp(map_get(oauth->params, "code_challenge_method"), "S256") == 0) {
        uint8_t hash[32];
        char hash_string[65];
        oauth->code_verifier = str_create_random(32);
        calc_sha_256(hash, oauth->code_verifier, str_len(oauth->code_verifier));
        hash_to_string(hash_string, hash);
        oauth->code_challenge = str_encode_base64(hash_string);
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth) {

    if (!map_get(oauth->params, "base_auth_url") || !map_get(oauth->params, "client_id"))
        return NULL;

    oauth_gen_challenge(oauth);

    char* authURL = str_create_fmt("%s?client_id=%s\\&response_type=code", map_get(oauth->params, "base_auth_url"), map_get(oauth->params, "client_id"));

    if (map_contains_key(oauth->params, "redirect_uri"))
        str_append_fmt(&authURL, "\\&redirect_uri=", map_get(oauth->params, "redirect_uri"));
    if (map_contains_key(oauth->params, "code_challenge_method"))
        str_append_fmt(&authURL, "\\&code_challenge_method=%s\\&code_challenge=%s", map_get(oauth->params, "code_challenge_method"), oauth->code_challenge);

    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* code) {

    if (!map_get(oauth->params, "base_token_url") || !map_get(oauth->params, "client_id") || code == NULL)
        return NULL;

    if (map_contains_key(oauth->params, "client_secret")) 
        oauth_append_data(oauth, "client_secret", map_get(oauth->params, "client_secret"));
    if (map_contains_key(oauth->params, "redirect_uri")) 
        oauth_append_data(oauth, "redirect_uri", map_get(oauth->params, "redirect_uri"));
    if (oauth->code_verifier != 0)
        oauth_append_data(oauth, "code_verifier", oauth->code_verifier);

    oauth_append_data(oauth, "code", code);
    oauth_append_data(oauth, "client_id", map_get(oauth->params, "client_id"));
    oauth_append_data(oauth, "grant_type", "authorization_code");

    return oauth_request(oauth, POST, map_get(oauth->params, "base_token_url"), false, false);
}

void oauth_auth(OAuth* oauth, const char* code) {
    response_data* response = oauth_post_token(oauth, code);
    
    json_t buf[20];
    const json_t* obj = json_create(response->data, buf, 20);
    oauth_set_param(oauth, "access_token", json_value(obj, "access_token").string);
    oauth_set_param(oauth, "refresh_token", json_value(obj, "refresh_token").string);
    oauth_set_param(oauth, "token_type", json_value(obj, "token_type").string);

    if (*(char*) map_get(oauth->params, "refresh_on_auth") == 't')
        oauth_start_refresh(oauth, (json_value(obj, "expires_in").integer * 2000)/3);
}

void oauth_set_param(OAuth* oauth, const char* key, char* value) {
    map_put(oauth->params, (void*) key, (void*) value);
}

void oauth_append_header(OAuth* oauth, const char* key, const char* value) {
    char* val = str_create_fmt("%s:%s", key, value);
    oauth->header_slist = curl_slist_append(oauth->header_slist, val);
}

void oauth_append_data(OAuth* oauth, const char* key, const char* value) {
    if (!oauth->data) oauth->data = map_alloc(strcmp);
    map_put(oauth->data, key, value);
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
        timex_sleep(strtol(map_get(oauth->params, "request_timeout"), NULL, 10));
        mutex_unlock(&oauth->request_mutex);
    }
}

void oauth_start_request_thread(OAuth* oauth) {
    oauth->request_run = true;
    thread_init(&oauth->request_thread);
    thread_start(&oauth->request_thread, oauth_process_request, oauth);
}

void oauth_stop_request_thread(OAuth* oauth) {
    oauth->request_run = false;
    thread_term(&oauth->request_thread);
}

response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, bool cache, bool auth) {

    request_data* rq_data = (request_data*) malloc(sizeof(request_data));
    rq_data->id = NULL;
    rq_data->data = parse_data(oauth->data, "&");
    rq_data->endpoint = endpoint;
    rq_data->header = oauth->header_slist;
    rq_data->method = method;
    str_append_fmt(&rq_data->id, "%s?%s", rq_data->endpoint, rq_data->data);

    response_data* response;
    if (cache && (response = unordered_map_get(oauth->cache, rq_data->id))) { 
        if (!unordered_map_contains_key(oauth->request_queue, rq_data->id))
            unordered_map_put(oauth->request_queue, rq_data->id, rq_data);
    } else  {
        mutex_lock(&oauth->request_mutex);

        if (oauth->authed && auth) {
            const char* str = NULL;
            str_append_fmt(&str, "Authorization: %s %s", map_get(oauth->params, "token_type"), map_get(oauth->params, "access_token"));
            rq_data->header = curl_slist_append(rq_data->header, str);
        }

        response = request(method, endpoint, rq_data->header, rq_data->data);

        if (cache) unordered_map_put(oauth->cache, rq_data->id, response);
        mutex_unlock(&oauth->request_mutex);
    } 
    
    map_free(oauth->data);
    oauth->data = NULL;
    return response;
}

int oauth_process_ini(void *arg, int line, const char *section, const char *key, const char *value) {
    OAuth* oauth = (OAuth*) arg;
    if (strcmp(section, "Params") == 0) {
        oauth_set_param(oauth, str_create(key), str_create(value));
    } else if (strcmp(section, "Header") == 0) {
        oauth_append_header(oauth, str_create(key), str_create(value));
    } return 0;
}

void oauth_config_dir(OAuth* oauth, const char* dir, const char* name) {
    oauth->config_file = getexecdir();
    path_add(&oauth->config_file, dir);
    path_add(&oauth->config_file, name);
    str_append(&oauth->config_file, ".ini");
}

bool oauth_load(OAuth* oauth) {

    if (oauth->config_file == NULL) 
        return NULL;

    int rc = ini_parse_file(oauth, oauth_process_ini, oauth->config_file);
    if (*(char*) map_get(oauth->params, "refresh_on_load") == 't' && 
        *(char*) map_get(oauth->params, "refresh_token") != '\0')
        oauth_start_refresh(oauth, 0);
    return oauth->authed;
}

bool oauth_save(OAuth* oauth) {

    if (oauth->config_file == NULL) 
        return NULL;

    char* key; char* value;
    FILE *fp = fopen(oauth->config_file, "w");
    if (fp == NULL) return NULL;

    fprintf(fp, "[Params]\n");
    map_iterator* iter = map_iterator_alloc(oauth->params);
    while (map_iterator_has_next(iter)) {
        map_iterator_next(iter, &key, &value);
        fprintf(fp, "%s=%s\n", key, value);
    } map_iterator_free(iter);

    fprintf(fp, "\n[Header]\n");
    struct curl_slist* ptr = oauth->header_slist;
    while (ptr) {
        char* save = NULL;
        char* val = str_create(ptr->data);
        key = str_create(str_token_begin(val, &save, ":"));
        value = str_token_begin(val, &save, "");
        fprintf(fp, "%s=%s\n", key, value);
        str_destroy(&val);
        str_destroy(&key);
        ptr = ptr->next;
    }
        
    // close the file
    fclose(fp);
    return 1;
}