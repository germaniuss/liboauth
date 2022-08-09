#include "OAuth.h"
#include "sha-256.h"
#include "str.h"
#include "time.h"
#include "json.h"
#include "ini.h"

#define MAX_BUFFER 2048 //4KB Buffers

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

typedef struct request_params {
    int port;
    int sec;
    MHD_AccessHandlerCallback request_callback;
    void* data;
} request_params;

bool openBrowser(const char* url) {
    char* command;
    switch(platform) {
        case WINDOWS:
            command = str_create("explorer ");
            break;
        case MACOS:
            command = str_create("open ");
            break;
        case LINUX:
            command = str_create("xdg-open ");
            break;
        case ANDROID:
            command = str_create("start --user 0 com.android.browser/com.android.browser.BrowserActivity -a android.intent.action.VIEW -d ");
            break;
        case IOS:
            command = str_create(" ");
            break;
        default:
            return false;
    }

    str_append(&command, url); /* add the extension */
    system(command);
    str_destroy(&command);
    return true;
}

// THIS IS RELATED TO REQUESTS

void request_completed (void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode code) {
  int *done = (int *)cls;
  *done = 1;
}

void* acceptSingleRequest(void* params) {
    request_params* prms = (request_params*) params;
    struct MHD_Daemon *d;
    fd_set rs;
    fd_set ws;
    fd_set es;
    MHD_socket max;
    time_t start;
    struct timeval tv;
    int done = 0;

    d = MHD_start_daemon (MHD_USE_ERROR_LOG, prms->port, NULL, NULL, prms->request_callback, prms->data, MHD_OPTION_NOTIFY_COMPLETED, &request_completed, &done, MHD_OPTION_END);
    if (d == NULL) {
        fprintf(stderr, "MHD_start_daemon() failed\n");
        return NULL;
    }

    start = time (NULL);
    while ((time (NULL) - start < prms->sec) && done == 0) {
        max = 0;
        FD_ZERO (&rs);
        FD_ZERO (&ws);
        FD_ZERO (&es);
        if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max)) {
            MHD_stop_daemon (d);
            fprintf(stderr, "MHD_get_fdset() failed\n");
            return NULL;
        }
        tv.tv_sec = 0;
        tv.tv_usec = 1000;
        select(max + 1, &rs, &ws, &es, &tv);
        MHD_run (d);
    } 
    
    MHD_stop_daemon (d);
    return (void*) 1;
}

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

    oauth_set_param(oauth, "token_type", json_value(json, "token_type").string);
    oauth_set_param(oauth, "access_token", json_value(json, "access_token").string);
    oauth_set_param(oauth, "refresh_token", json_value(json, "refresh_token").string);

    oauth_start_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);
    oauth->authed = true;

    const char *page  = "<HTML><HEAD>Return to the app now.</BODY></HTML>";
    struct MHD_Response *response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
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

bool oauth_start(OAuth* oauth) {

    if (!map_get(oauth->params, "base_auth_url")  || 
        !map_get(oauth->params, "base_token_url") || 
        !map_get(oauth->params, "client_id")
    ) return false;

    oauth_gen_challenge(oauth);
    struct thread th;
    request_params params;
    params.port = strtol(map_get(oauth->params, "port"), NULL, 10);
    params.sec = strtol(map_get(oauth->params, "listen_timeout"), NULL, 10);
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

    oauth_append_data(oauth, "client_id", map_get(oauth->params, "client_id"));
    oauth_append_data(oauth, "response_type", "code");
    if (map_contains_key(oauth->params, "redirect_uri"))
        oauth_append_data(oauth, "redirect_uri", map_get(oauth->params, "redirect_uri"));
    if (map_contains_key(oauth->params, "code_challenge_method")) {
        oauth_append_data(oauth, "code_challenge_method", map_get(oauth->params, "code_challenge_method"));
        oauth_append_data(oauth, "code_challenge", oauth->code_challenge);
    }
    
    char* data_str = parse_data(oauth->data, "\\&");
    char* authURL = str_create(map_get(oauth->params, "base_auth_url"));
    if (data_str != NULL) {
        str_append(&authURL, "?");
        str_append(&authURL, data_str);
    }; str_destroy(&data_str);

    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* code) {

    if (!map_get(oauth->params, "base_token_url") || code == NULL)
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

void oauth_set_param(OAuth* oauth, const char* key, char* value) {
    map_put(oauth->params, (void*) key, (void*) value);
}

void oauth_append_header(OAuth* oauth, const char* key, const char* value) {
    char* val = str_create_fmt("%s:%s", key, value);
    oauth->header_slist = curl_slist_append(oauth->header_slist, val);
}

void oauth_append_data(OAuth* oauth, const char* key, const char* value) {
    if (!oauth->data) oauth->data = map_alloc(strcmp);
    map_put(oauth->data, (void*) key, (void*) value);
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
        time_sleep(strtol(map_get(oauth->params, "request_timeout"), NULL, 10));
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

int process_ini(void *arg, int line, const char *section, const char *key, const char *value) {
    OAuth* oauth = (OAuth*) arg;
    if (strcmp(section, "Params") == 0) {
        oauth_set_param(oauth, str_create(key), str_create(value));
    } else if (strcmp(section, "Header") == 0) {
        oauth_append_header(oauth, str_create(key), str_create(value));
    } return 0;
}

bool oauth_load(OAuth* oauth, const char* dir, const char* name) {
    char* full_dir = NULL;
    if (dir[0] != '\0') str_append(&full_dir, "/");
    str_append_fmt(&full_dir, "%s.ini", name);
    int rc = ini_parse_file(oauth, process_ini, full_dir);
    str_destroy(&full_dir);
    return oauth;
}

bool oauth_save(OAuth* oauth, const char* dir, const char* name) {
    char* key; char* value;
    char* full_dir = NULL;
    if (dir[0] != '\0') str_append(&full_dir, "/");
    str_append_fmt(&full_dir, "%s.ini", name);

    FILE *fp = fopen(full_dir, "w");
    str_destroy(&full_dir);
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
        const char* key = str_create(str_token_begin(val, &save, ":"));
        const char* value = str_token_begin(val, &save, "");
        fprintf(fp, "%s=%s\n", key, value);
        str_destroy(&val);
        ptr = ptr->next;
    }
        
    // close the file
    fclose(fp);
    return 1;
}

int main() {
    srand(time_ms());
    OAuth* oauth = oauth_create();

    oauth_load(oauth, "", "TEST");
    oauth_start_refresh(oauth, 0);
    oauth_save(oauth, "", "TEST");

    oauth_start_request_thread(oauth);

    oauth_append_data(oauth, "fields", "id,title,main_picture,alternative_titles,start_date,end_date,synopsis,mean,rank,popularity,num_list_users,num_scoring_users,nsfw,created_at,updated_at,media_type,status,genres,my_list_status,num_episodes,start_season,broadcast,source,average_episode_duration,rating,pictures,background,related_anime,related_manga,recommendations,studios,statistics");

    response_data* response = oauth_request(oauth, GET, "https://api.myanimelist.net/v2/anime/30230", true, true);

    oauth_append_data(oauth, "fields", "id,title,main_picture,alternative_titles,start_date,end_date,synopsis,mean,rank,popularity,num_list_users,num_scoring_users,nsfw,created_at,updated_at,media_type,status,genres,my_list_status,num_episodes,start_season,broadcast,source,average_episode_duration,rating,pictures,background,related_anime,related_manga,recommendations,studios,statistics");

    response = oauth_request(oauth, GET, "https://api.myanimelist.net/v2/anime/30230", true, true);

    oauth_stop_request_thread(oauth);
    oauth_stop_refresh(oauth);

    oauth_delete(oauth);
}