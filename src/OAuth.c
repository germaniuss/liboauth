#include <curl/curl.h>
#include <OAuth.h>

typedef struct request_data {
    uint8_t options;
    const char* data;
    struct curl_slist *header;
    REQUEST method;
    const char* endpoint;
    const char* id;
} request_data;

#define MAX_BUFFER 2048 //4KB Buffers
#define BIT(NUM, N) ((NUM) & (N))

map_dec_strkey(request, const char*, request_data)
map_dec_strkey(response, const char*, response_data)
map_def_strkey(request, const char*, request_data, cmp_str, murmurhash, {.id = 0})
map_def_strkey(response, const char*, response_data, cmp_str, murmurhash, {.data = 0})

typedef struct OAuth {
    bool authed;
    // All the default options are loaded here
    char* args[NUM_PARAMS];
    uint8_t default_options;
    // There is all the data for a request
    uint8_t current_options;
    struct curl_slist* request_header;
    sorted_map* request_data;
    // Here is the response cache
    struct map_response cache;
    // Here is the request queue functionality
    int request_ms;
    thread_timer_t request_timer;
    struct map_request request_queue;
    // Here is the refresh functionality
    int refresh_ms;
    thread_timer_t refresh_timer;
} OAuth;

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

response_data request(REQUEST method, const char* endpoint, struct curl_slist* header, const char* data) {
    char* new_endpoint = str_create(endpoint);
    data_t* storage = data_create();
    response_data response;

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (curl) {
        // Set the URL, header and callback function
        if (header != NULL) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, storage);
              
        /* Now specify the POST/DELETE/PUT/ data */
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
            return (response_data) {.data = 0};
        }

        response.data = process_response_data(storage);
        response.response_code == 0;

        // get response code and content type
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &response.response_code);
        const char* content;
        curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &content);
        response.content_type = str_create(content);

        /* Check for errors */
        /* always cleanup */
        curl_easy_cleanup(curl);
        data_clean(storage);
        str_destroy(&new_endpoint);
    } 
    curl_global_cleanup();
    return response;
}

OAuth* oauth_create(const char* config_file) {
    OAuth* oauth = (OAuth*) calloc(1, sizeof(OAuth));       
    oauth->authed = false;
    thread_timer_init(&oauth->refresh_timer);
    thread_timer_init(&oauth->request_timer);
    map_init_request(&oauth->request_queue, 0, 0);
    map_init_response(&oauth->cache, 0, 0);
    map_set_circular(&oauth->cache, true);
    map_set_refresh(&oauth->cache, true);
    map_set_max_size(&oauth->request_queue, 200);
    map_set_max_size(&oauth->cache, 200);
    oauth->request_data = NULL;
    oauth->request_header = NULL;

    if (config_file) {
        oauth->args[CONFIG_FILE] = str_create(config_file);
        oauth_load(oauth);
    }
    
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    oauth_stop_request_timer(oauth);
    if (oauth->args[SAVE_ON_CLOSE])
        oauth_save(oauth);
    map_term_request(&oauth->request_queue);
    map_term_response(&oauth->cache);
    if (oauth->request_data) sorted_map_free(oauth->request_data);

    for (int i = 0; i < NUM_PARAMS; i++) {
        if (oauth->args[i]) str_destroy(&oauth->args[i]);
    }

    if (oauth->request_header) curl_slist_free_all(oauth->request_header);
    free(oauth);
}

void* oauth_parse_auth(OAuth* oauth, response_data response) {
    
    enum { MAX_FIELDS = 512 };
    json_t pool[ MAX_FIELDS ];
    const json_t* json = json_create(response.data, pool, MAX_FIELDS);
    oauth->args[TOKEN_BEARER] = json_value(json, "token_type").string;
    oauth->args[ACCESS_TOKEN] = json_value(json, "access_token").string;
    oauth->args[REFRESH_TOKEN] = json_value(json, "refresh_token").string;
    oauth->authed = true;

    if (oauth->args[SAVE_ON_OAUTH])
        oauth_save(oauth);

    if (oauth->args[REFRESH_ON_OAUTH])
        oauth_start_refresh(oauth, (json_value(json, "expires_in").integer * 2000)/3);
}

bool oauth_gen_challenge(OAuth* oauth) {
    
    if (!oauth->args[CHALLENGE_METHOD])
        return false;

    if (strcmp(oauth->args[CHALLENGE_METHOD], "plain") == 0) {
        oauth->args[CODE_VERIFIER] = str_create_random(128);
        oauth->args[CODE_CHALLENGE] = str_create(oauth->args[CODE_VERIFIER]);
        return true;
    } else if (strcmp(oauth->args[CHALLENGE_METHOD], "S256") == 0) {
        uint8_t hash[32];
        char hash_string[65];
        oauth->args[CODE_VERIFIER] = str_create_random(32);
        calc_sha_256(hash, oauth->args[CODE_VERIFIER], str_len(&oauth->args[CODE_VERIFIER]));
        hash_to_string(hash_string, hash);
        oauth->args[CODE_CHALLENGE] = str_encode_base64(hash_string);
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth) {

    if (!oauth->args[AUTH_URL] || !oauth->args[CLIENT_ID])
        return NULL;

    oauth_gen_challenge(oauth);

    char* authURL = str_create_fmt("%s?client_id=%s\\&response_type=code", oauth->args[AUTH_URL], oauth->args[CLIENT_ID]);

    if (oauth->args[REDIRECT_URI])
        str_append_fmt(&authURL, "\\&redirect_uri=", oauth->args[REDIRECT_URI]);
    if (oauth->args[CHALLENGE_METHOD])
        str_append_fmt(&authURL, "\\&code_challenge_method=%s\\&code_challenge=%s", oauth->args[CHALLENGE_METHOD], oauth->args[CODE_CHALLENGE]);

    return authURL;
}

response_data oauth_post_token(OAuth* oauth, const char* code) {

    if (!oauth->args[TOKEN_URL] || !oauth->args[CLIENT_ID] || code == NULL)
        return (response_data) {.data = 0};

    if (oauth->args[CLIENT_SECRET]) 
        oauth_append_data(oauth, "client_secret", oauth->args[CLIENT_SECRET]);
    if (oauth->args[REDIRECT_URI]) 
        oauth_append_data(oauth, "redirect_uri", oauth->args[REDIRECT_URI]);
    if (oauth->args[CODE_VERIFIER] != 0)
        oauth_append_data(oauth, "code_verifier", oauth->args[CODE_VERIFIER]);

    oauth_append_data(oauth, "code", code);
    oauth_append_data(oauth, "client_id", oauth->args[CLIENT_ID]);
    oauth_append_data(oauth, "grant_type", "authorization_code");

    oauth_set_options(oauth, 0);
    return oauth_request(oauth, POST, oauth->args[TOKEN_URL]);
}

void oauth_auth(OAuth* oauth, const char* code) {
    response_data response = oauth_post_token(oauth, code);
    if (response.data && response.response_code == 200) {
        oauth_parse_auth(oauth, response);
    }
}

void oauth_set_param(OAuth* oauth, PARAM param, char* value) {
    oauth->args[param] = value;
}

bool oauth_set_options(OAuth* oauth, uint8_t options) {
    if (options > 7) return false;
    oauth->current_options = options;
    return true;
}

void oauth_append_header(OAuth* oauth, const char* key, const char* value) {
    char* val = str_create_fmt("%s:%s", key, value);
    oauth->request_header = curl_slist_append(oauth->request_header, val);
}

void oauth_append_data(OAuth* oauth, const char* key, const char* value) {
    if (!oauth->request_data) oauth->request_data = sorted_map_alloc(strcmp);
    sorted_map_put(oauth->request_data, key, value);
}

void* oauth_refresh_task(OAuth* oauth) {
    if (!oauth->args[REFRESH_TOKEN] || !oauth->args[CLIENT_ID] || !oauth->args[TOKEN_URL])
        return false;

    oauth_append_data(oauth, "client_id", oauth->args[CLIENT_ID]);
    oauth_append_data(oauth, "refresh_token", oauth->args[REFRESH_TOKEN]);
    oauth_append_data(oauth, "grant_type", "refresh_token");
    if (oauth->args[CLIENT_SECRET])
        oauth_append_data(oauth, "client_secret", oauth->args[CLIENT_SECRET]);

    oauth_set_options(oauth, 0);
    response_data response = oauth_request(oauth, POST, oauth->args[TOKEN_URL]);

    if (response.data && response.response_code == 200) {
        oauth_parse_auth(oauth, response);
    }
}

bool oauth_start_refresh_timer(OAuth* oauth, uint64_t ms) {
    if (!oauth->args[REFRESH_TOKEN] || !oauth->args[CLIENT_ID] || !oauth->args[TOKEN_URL])
        return false;

    if (ms == 0) {
        oauth_refresh_task(oauth);
        return true;
    }

    oauth->refresh_ms = ms;
    thread_timer_start(&oauth->refresh_timer, &oauth->refresh_ms, true, oauth_refresh_task, oauth);
    return true;
}

bool oauth_stop_refresh_timer(OAuth* oauth) {
    thread_timer_term(&oauth->refresh_timer);
}

void* oauth_request_task(OAuth* oauth) {
    // pop head from request_queue
    request_data rq_data = oauth->request_queue.head->entry->value;
    map_del_request(&oauth->request_queue, rq_data.id);
    response_data response = request(rq_data.method, rq_data.endpoint, rq_data.header, rq_data.data);
    if (response.data && response.response_code == 200) {
        map_put_response(&oauth->cache, rq_data.id, response);
    }
}

void oauth_start_request_timer(OAuth* oauth) {
    oauth->request_ms = 0;
    thread_timer_start(&oauth->request_timer, &oauth->request_ms, true, oauth_request_task, oauth);
}

void oauth_stop_request_timer(OAuth* oauth) {
    thread_timer_term(&oauth->request_timer);
}

response_data oauth_request(OAuth* oauth, REQUEST method, const char* endpoint) {
    // Prepare the request data
    request_data rq_data;
    rq_data.id = NULL;
    rq_data.options = oauth->current_options;
    rq_data.data = parse_data(oauth->request_data, "&");
    rq_data.endpoint = endpoint;
    rq_data.header = oauth->request_header;
    rq_data.method = method;
    str_append_fmt(&rq_data.id, "/%s/%s", REQUEST_STRING[method], endpoint);
    if (rq_data.data) str_append_fmt(&rq_data.id, "?%s", rq_data.data);

    if (oauth->authed && BIT(oauth->current_options, REQUEST_AUTH)) {
        const char* str = NULL;
        str_append_fmt(&str, "Authorization: %s %s", oauth->args[TOKEN_BEARER], oauth->args[ACCESS_TOKEN]);
        rq_data.header = curl_slist_append(rq_data.header, str);
        str_destroy(&str);
    }

    // Free values for the next request
    sorted_map_free(oauth->request_data);
    oauth->current_options = oauth->default_options;
    oauth->request_data = NULL;

    // Get value if it is in the cache
    response_data response = map_get_response(&oauth->cache, rq_data.id);

    if (BIT(rq_data.options, REQUEST_QUEUE)) {

        if (!BIT(rq_data.options, REQUEST_SKIP)) {
            map_put_request(&oauth->request_queue, rq_data.id, rq_data);
        } else {
            // put in first position
        }

        if (!BIT(rq_data.options, REQUEST_ASYNC)) {
            // wait for request to be completed (unless response is not null and cache is set)
        } else {
            // do not wait for request to be completed
        }

        return response;
    }

    if (BIT(rq_data.options, REQUEST_ASYNC)) {
        // execute the request 
        return response;
    }
    
    return response;
}

int oauth_process_ini(OAuth *oauth, int line, const char *section, const char *key, const char *value) {
    uint32_t i;

    if (!strcmp("Header", section)) {
        char* val = str_create_fmt("%s:%s", key, value);
        oauth->request_header = curl_slist_append(oauth->request_header, val);
        return 0;
    }

    if (!strcmp("Params", section)) {
        for (i = 0; i < NUM_PARAMS; i++) {
            if (!strcmp(PARAM_STRING[i], key)) {
                oauth->args[i] = str_create(value);
                break;
            }
        } return 0;
    }

    if (!strcmp("Options", section)) {
        uint8_t val = 1;
        for (i = 0; i < NUM_OPTIONS; i++) {
            if (!strcmp(OPTION_STRING[i], key) && !strcmp(value, "true")) {
                oauth->default_options |= val;
                oauth->current_options = oauth->default_options;
                break;
            } val *= 2;
        } return 0;
    }

    return 0;
}

bool oauth_load_config(OAuth* oauth) {
    if (oauth->args[CONFIG_FILE] == NULL)
        return NULL;

    char* dir = getfullpath(oauth->args[CONFIG_FILE]);
    return !ini_parse_file(oauth, oauth_process_ini, dir);
}

bool oauth_load_cache(OAuth* oauth) {
    if (oauth->args[CACHE_FILE] == NULL) 
        return NULL;

    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    char* dir = getfullpath(oauth->args[CACHE_FILE]);

    if (!(fp = fopen(dir, "r"))) {
        return NULL;
    }

    char* save = NULL;
    while ((read = getline(&line, &len, fp)) != -1 && strcmp(line, "\n")) {
        const char* key = strtok(line, " ");
        const char* val = strtok(NULL, "");
        response_data response;
        response.content_type = str_create("unknown");
        response.data = str_create(val);
        response.response_code = 200;
        map_put_response(&oauth->cache, str_create(key), response);
    }

    fclose(fp);
    if (line) free(line);
    return 1;    
}

bool oauth_load(OAuth* oauth) {
    oauth_load_config(oauth);
    oauth_load_cache(oauth);

    if (oauth->args[REFRESH_ON_LOAD])
        oauth_start_refresh(oauth, 0);

    if (oauth->args[REQUEST_ON_LOAD])
        oauth_start_request_thread(oauth);

    if (oauth->args[REQUEST_QUEUE_SIZE]) 
        map_set_max_size(&oauth->request_queue, strtol(oauth->args[REQUEST_QUEUE_SIZE], NULL, 10));

    if (oauth->args[CACHE_SIZE]) 
        map_set_max_size(&oauth->cache, strtol(oauth->args[CACHE_SIZE], NULL, 10));

    return oauth->authed;
}

bool oauth_save_config(OAuth* oauth) {
    
    if (oauth->args[CONFIG_FILE] == NULL) 
        return NULL;
    
    char* dir = getfullpath(oauth->args[CONFIG_FILE]);

    ini aux;
    ini_open(&aux, dir);

    // for every value in params save
    for (uint32_t i = 0; i < 15; i++) {
        ini_save(&aux, "Params", PARAM_STRING[i], oauth->args[i]);
    }
    
    // for every value in header save
    char* key; char* value;
    struct curl_slist* ptr = oauth->request_header;
    while (ptr) {
        char* save = NULL;
        char* val = str_create(ptr->data);
        key = str_create(str_token_begin(val, &save, ":"));
        value = str_token_begin(val, &save, "");
        ini_save(&aux, "Header", key, value);
        str_destroy(&val);
        str_destroy(&key);
        ptr = ptr->next;
    }

    ini_close(&aux);
}

bool oauth_save_cache(OAuth* oauth) {

    if (oauth->args[CACHE_FILE] == NULL) 
        return NULL;

    char* dir = getfullpath(oauth->args[CACHE_FILE]);

    FILE *fp = fopen(dir, "w");
    if (fp == NULL) return NULL;

    struct map_link_response* link = oauth->cache.head;
    if (link) {
        fprintf(fp, "%s %s", link->entry->key, link->entry->value.data);
    }

    for (link = link->next; link; link = link->next) {
        fprintf(fp, "\n%s %s", link->entry->key, link->entry->value.data);
    }
        
    // close the file
    fclose(fp);
    return 1;
}

bool oauth_save(OAuth* oauth) {
    oauth_save_config(oauth);
    oauth_save_cache(oauth);
}