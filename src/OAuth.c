#define _UTILS_IMPL
#include <utils/sha_256.h>
#include <utils/str.h>
#include <utils/time.h>
#include <utils/json.h>
#include <utils/ini.h>
#include <utils/sorted_map.h>
#include <utils/mutex.h>
#include <utils/thread.h>
#include <utils/linked_map.h>
#include <utils/timer.h>
#include <utils/path.h>

#include <curl/curl.h>
#include "OAuth.h"

#define MAX_BUFFER 2048 //4KB Buffers

typedef struct OAuth {
    bool request_run;
    linked_map cache;
    linked_map request_queue;
    struct thread request_thread;
    struct mutex request_mutex;
    struct timer refresh_timer;
    struct curl_slist* header_slist;
    sorted_map* data;
    ini params;
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

response_data* copy_response(response_data* response) {
    response_data* response_copy = (response_data*) malloc(sizeof(response_data));
    response_copy->data = response->data;
    response_copy->content_type = response->content_type;
    response_copy->response_code = response->response_code;
    return response_copy;
}

// THIS IS ALL RELATED TO HEADER AND DATA

char* parse_data(sorted_map* data, const char* data_join) {
    if (!data) return NULL;
    char* key; char* value;  
    char* data_str = NULL;
    sorted_map_iterator* data_iter = sorted_map_iterator_alloc(data);
    while (sorted_map_iterator_has_next(data_iter)) {
        sorted_map_iterator_next(data_iter, &key, &value);
        if (data_str != NULL) str_append(&data_str, data_join);
        str_append_fmt(&data_str, "%s=%s", key, value);
    } sorted_map_iterator_free(data_iter);
    return data_str;
}

response_data* request(REQUEST method, const char* endpoint, struct curl_slist* header, const char* data) {
    char* new_endpoint = str_create(endpoint);
    data_t* storage = data_create();
    response_data* response = (response_data*) malloc(sizeof(response_data));

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (curl) {
        // Set the URL, header and callback function
        if (header != NULL) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, storage);
              
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
            return NULL;
        }

        response->data = process_response_data(storage);
        response->response_code == 0;

        // get response code and content type
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &response->response_code);
        curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &response->content_type);

        /* Check for errors */
        /* always cleanup */
        curl_easy_cleanup(curl);
        data_clean(storage);
        str_destroy(&new_endpoint);
    } 
    curl_global_cleanup();
    return response;
}

OAuth* oauth_create() {
    OAuth* oauth = (OAuth*) malloc(sizeof(OAuth));
    oauth->authed = false;
    oauth->request_run = false;
    ini_init(&oauth->params);
    linked_map_init(&oauth->request_queue, 200, 0, 0.0, true, true, &linked_map_config_str);
    linked_map_init(&oauth->cache, 200, 0, 0.0, true, true, &linked_map_config_str);
    mutex_init(&oauth->request_mutex);
    oauth->data = NULL;
    oauth->header_slist = NULL;    
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    linked_map_term(&oauth->request_queue);
    linked_map_term(&oauth->cache);
    mutex_term(&oauth->request_mutex);
    if (oauth->data) map_free(oauth->data);
    if (oauth->code_challenge) str_destroy(&oauth->code_challenge);
    if (oauth->code_verifier) str_destroy(&oauth->code_verifier);
    if (oauth->header_slist) curl_slist_free_all(oauth->header_slist);
}

void* oauth_parse_auth(OAuth* oauth, response_data* response) {
    
    enum { MAX_FIELDS = 512 };
    json_t pool[ MAX_FIELDS ];
    const json_t* json = json_create(response->data, pool, MAX_FIELDS);
    oauth_set_param(oauth, "token_type", json_value(json, "token_type").string);
    oauth_set_param(oauth, "access_token", json_value(json, "access_token").string);
    oauth_set_param(oauth, "refresh_token", json_value(json, "refresh_token").string);
    oauth->authed = true;

    if (*ini_get(&oauth->params, "Params", "save_on_refresh") == 't')
        oauth_save(oauth);

    if (*ini_get(&oauth->params, "Params", "refresh_on_auth") == 't')
        oauth_start_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);
}

void* oauth_refresh_task(void* in) {
    OAuth* oauth = (OAuth*) in;

    if (!ini_get(&oauth->params, "Params", "refresh_token") || !ini_get(&oauth->params, "Params", "client_id") || !ini_get(&oauth->params, "Params", "base_token_url"))
        return false;

    oauth_append_data(oauth, "client_id", ini_get(&oauth->params, "Params", "client_id"));
    oauth_append_data(oauth, "refresh_token", ini_get(&oauth->params, "Params", "refresh_token"));
    oauth_append_data(oauth, "grant_type", "refresh_token");
    if (ini_get(&oauth->params, "Params", "client_secret"))
        oauth_append_data(oauth, "client_secret", ini_get(&oauth->params, "Params", "client_secret"));

    response_data* response = oauth_request(oauth, POST, ini_get(&oauth->params, "Params", "base_token_url"), false, false);
    if (response && response->response_code == 200) {
        oauth_parse_auth(oauth, response);
        free(response);
    }
}

bool oauth_start_refresh(OAuth* oauth, uint64_t ms) {
    if (!ini_get(&oauth->params, "Params", "refresh_token") || !ini_get(&oauth->params, "Params", "client_id") || !ini_get(&oauth->params, "Params", "base_token_url"))
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
    
    if (!ini_get(&oauth->params, "Params", "code_challenge_method"))
        return false;

    if (strcmp(ini_get(&oauth->params, "Params", "code_challenge_method"), "plain") == 0) {
        oauth->code_verifier = str_create_random(128);
        oauth->code_challenge = str_create(oauth->code_verifier);
        return true;
    } else if (strcmp(ini_get(&oauth->params, "Params", "code_challenge_method"), "S256") == 0) {
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

    if (!ini_get(&oauth->params, "Params", "base_auth_url") || !ini_get(&oauth->params, "Params", "client_id"))
        return NULL;

    oauth_gen_challenge(oauth);

    char* authURL = str_create_fmt("%s?client_id=%s\\&response_type=code", ini_get(&oauth->params, "Params", "base_auth_url"), ini_get(&oauth->params, "Params", "client_id"));

    if (ini_get(&oauth->params, "Params", "redirect_uri"))
        str_append_fmt(&authURL, "\\&redirect_uri=", ini_get(&oauth->params, "Params", "redirect_uri"));
    if (ini_get(&oauth->params, "Params", "code_challenge_method"))
        str_append_fmt(&authURL, "\\&code_challenge_method=%s\\&code_challenge=%s", ini_get(&oauth->params, "Params", "code_challenge_method"), oauth->code_challenge);

    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* code) {

    if (!ini_get(&oauth->params, "Params", "base_token_url") || !ini_get(&oauth->params, "Params", "client_id") || code == NULL)
        return NULL;

    if (ini_get(&oauth->params, "Params", "client_secret")) 
        oauth_append_data(oauth, "client_secret", ini_get(&oauth->params, "Params", "client_secret"));
    if (ini_get(&oauth->params, "Params", "redirect_uri")) 
        oauth_append_data(oauth, "redirect_uri", ini_get(&oauth->params, "Params", "redirect_uri"));
    if (oauth->code_verifier != 0)
        oauth_append_data(oauth, "code_verifier", oauth->code_verifier);

    oauth_append_data(oauth, "code", code);
    oauth_append_data(oauth, "client_id", ini_get(&oauth->params, "Params", "client_id"));
    oauth_append_data(oauth, "grant_type", "authorization_code");

    return oauth_request(oauth, POST, ini_get(&oauth->params, "Params", "base_token_url"), false, false);
}

void oauth_auth(OAuth* oauth, const char* code) {
    response_data* response = oauth_post_token(oauth, code);
    if (response && response->response_code == 200) {
        oauth_parse_auth(oauth, response);
        free(response);
    }
}

void oauth_set_param(OAuth* oauth, const char* key, char* value) {
    ini_put(&oauth->params, "Params", (void*) key, (void*) value);
}

void oauth_append_header(OAuth* oauth, const char* key, const char* value) {
    char* val = str_create_fmt("%s:%s", key, value);
    oauth->header_slist = curl_slist_append(oauth->header_slist, val);
    ini_put(&oauth->params, "Header", (void*) key, (void*) value);
}

void oauth_append_data(OAuth* oauth, const char* key, const char* value) {
    if (!oauth->data) oauth->data = sorted_map_alloc(strcmp);
    sorted_map_put(&oauth->data, key, value);
}

void* oauth_process_request(void* data) {
    OAuth* oauth = (OAuth*) data;
    while (oauth->request_run) {
        while (oauth->request_queue.size == 0 && oauth->request_run);
        if (!oauth->request_run) break;
        mutex_lock(&oauth->request_mutex);
        request_data* rq_data = oauth->request_queue.head->value;
        linked_map_del(&oauth->request_queue, rq_data->id);
        response_data* response = request(rq_data->method, rq_data->endpoint, rq_data->header, rq_data->data);
        if (response && response->response_code == 200) {
            response_data* old_response;
            if (old_response = linked_map_get(&oauth->cache, rq_data->id))
                free(old_response);
            linked_map_put(&oauth->cache, rq_data->id, response);
        }  
        time_sleep(strtol(ini_get(&oauth->params, "Params", "request_timeout"), NULL, 10));
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
    str_append_fmt(&rq_data->id, "/%s %s?%s", REQUEST_STRING[method], endpoint, rq_data->data);

    response_data* response;
    if (cache && (response = linked_map_get(&oauth->cache, rq_data->id))) {
        response = copy_response(response);
        if (!linked_map_get(&oauth->request_queue, rq_data->id))
            linked_map_put(&oauth->request_queue, rq_data->id, rq_data);
    } else  {
        mutex_lock(&oauth->request_mutex);
        if (oauth->authed && auth) {
            const char* str = NULL;
            str_append_fmt(&str, "Authorization: %s %s", ini_get(&oauth->params, "Params", "token_type"), ini_get(&oauth->params, "Params", "access_token"));
            rq_data->header = curl_slist_append(rq_data->header, str);
        } response = request(method, endpoint, rq_data->header, rq_data->data);
        if (response && cache && response->response_code == 200) {
            linked_map_put(&oauth->cache, rq_data->id, response);
            response = copy_response(response);
        } 
        mutex_unlock(&oauth->request_mutex);
    }
    
    map_free(oauth->data);
    oauth->data = NULL;
    return response;
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

    int rc = ini_load(&oauth->params, oauth->config_file);

    char* key; char* value;
    uint32_t index = map_get_s64(&oauth->params.sections, "Header");
    map_foreach(&oauth->params.params[index], key, value) {
        char* val = str_create_fmt("%s:%s", key, value);
        oauth->header_slist = curl_slist_append(oauth->header_slist, val);
    }

    if (ini_get(&oauth->params, "Params", "refresh_on_load")[0] == 't' && 
        ini_get(&oauth->params, "Params", "refresh_token") != 0)
        oauth_start_refresh(oauth, 0);
    return oauth->authed;
}

bool oauth_save(OAuth* oauth) {

    if (oauth->config_file == NULL) 
        return NULL;

    return ini_save(&oauth->params, oauth->config_file);
}