#include "utils.h"
#include "str.h"
#include "time.h"

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

struct curl_slist* parse_header(map* header) {
    if (!header) return NULL;
    char* key; char* value;
    struct curl_slist* header_list = NULL;
    map_iterator* header_iter = map_iterator_alloc(header);
    while (map_iterator_has_next(header_iter)) {
        map_iterator_next(header_iter, &key, &value);
        char* data_str;
        str_append_fmt(&data_str, "%s: %s", key, value);
        header_list = curl_slist_append(header_list, data_str);
        str_destroy(&data_str);
    } map_iterator_free(header_iter);
    return header_list;
}

response_data* request(REQUEST method, const char* endpoint, struct curl_slist* header, const char* data) {
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
        } else str_append_fmt(endpoint, "?%s", data);
        
        curl_easy_setopt(curl, CURLOPT_URL, endpoint);
        
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
    } 
    curl_global_cleanup();
    return resp_data;
}