#include "utils.h"
#include <ctype.h>

char* strdupex(const char* str, int val) {
    char* aux = (char*) malloc(strlen(str) + val + 1);
    int shrink = (val < 0) ? val : 0;
    memcpy(aux, str, strlen(str) + 1 + shrink);
    aux[strlen(str) + shrink] = '\0';
    return aux;
}

char * trim(char * s) {
    int l = strlen(s);
    while(isspace(s[l - 1])) --l;
    while(* s && isspace(* s)) ++s, --l;
    if (s[0] == '\0') l = 0;
    l -= strlen(s);
    return strdupex(s, l);
}

int openBrowser(const char* url) {
    char* command;
    switch(platform) {
        case WINDOWS:
            command = strdupex("explorer ", strlen(url));
            break;
        case MACOS:
            command = strdupex("open ", strlen(url));
            break;
        case LINUX:
            command = strdupex("xdg-open ", strlen(url));
            break;
        case ANDROID:
            command = strdupex("start --user 0 com.android.browser/com.android.browser.BrowserActivity -a android.intent.action.VIEW -d ", strlen(url));
            break;
        case IOS:
            command = strdupex(" ", strlen(url));
            break;
        default:
            return 0;
    }

    strcat(command, url); /* add the extension */
    system(command);
    free(command);
    return 1;
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

struct curl_slist* parseHeader(ht* header) {
    struct curl_slist *list = NULL;
    hti iter = ht_iterator(header);
    while (ht_next(&iter)) {
        char* data_str = strdupex(iter.key, strlen(iter.value) + 1);
        strcat(data_str, "=");
        strcat(data_str, iter.value);
        list = curl_slist_append(list, data_str);
        free(data_str);
    } return list;
}

char* parseData(ht* data, const char* join) {
    
    // Get the length of the final string
    int len = 0;
    hti iter = ht_iterator(data);
    while (ht_next(&iter)) {
        if (len != 0) len += strlen(join);
        len += strlen(iter.key) + strlen(iter.value) + 1;
    } if (len < 0) len = 0;
    char* data_str = strdupex("", len);

    // Concatenate all strings
    iter = ht_iterator(data);
    while (ht_next(&iter)) {
        if (data_str[0] != '\0') strcat(data_str, join);
        strcat(data_str, iter.key);
        strcat(data_str, "=");
        strcat(data_str, iter.value);
    } return data_str;
}