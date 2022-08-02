#pragma once
#include <curl/curl.h>
#include <microhttpd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tiny-json.h"

#define MAX_BUFFER 4096 //4KB Buffers

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

const char* json_value(const json_t* parent, const char* key) {
    if ( parent == NULL ) {fprintf(stderr, "Invalid json response\n"); return NULL;}
    json_t const* key_param = json_getProperty( parent, key);
    if ( key_param == NULL ) {fprintf(stderr, "Invalid json response\n"); return NULL;}
    return json_getValue( key_param );
}

char* strdupex(const char* str, size_t val) {
    char* aux = (char*) malloc(strlen(str) + val + 1);
    memcpy(aux, str, strlen(str) + 1);
    return aux;
}

char* genCodeVerifier() {
    return strdupex("", 0);
}

char* genCodeChallenge(int size) {
    return strdupex("iV2oX7XoHYvC-9voqBb5zLXpav5UcTfkUO_o6ldtBzO7dxW-4C_mBu6-X5nr~8pnyy~bwsBAl7QELLtZUVNev9nkiJj1boZ7FZvTqbaNdMxTAz~GeKVY251wsQzID3xA", 0);
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

typedef struct request_params {
    int port;
    int sec;
    MHD_AccessHandlerCallback request_callback;
    void* data;
} request_params;

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

typedef struct data_t {
    char d[MAX_BUFFER];
    struct data_t* next;
    int idx;
} data_t;

typedef struct response_data {
    char* data;
    char* header;
} response_data;

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
    } memcpy(retVal + data_len, "", 1);

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

typedef struct request_data {
    char* key;
    char* value;
    struct request_data* next;
    struct request_data* last;
} request_data;

request_data* request_data_create() {
    request_data* data = (request_data*) malloc(sizeof(request_data));
    data->key = NULL;
    data->value = NULL;
    data->next = NULL;
    data->last = data;
    return data;
}

void request_data_append(request_data* data, const char* key, const char* value) {
    data->last->next = (request_data*) malloc(sizeof(request_data));
    data->last = data->last->next;
    data->last->key = strdupex(key, 0);
    data->last->value = strdupex(value, 0);
    data->last->next = NULL;
}

request_data* request_data_copy(request_data* data) {
    request_data* iter = data;
    request_data* newdata = iter ? request_data_create() : NULL;
    while (iter->next) {
        request_data_append(newdata, iter->next->key, iter->next->value);
        iter = iter->next;
    } return newdata;
}

void request_data_clean(request_data* d) {
    request_data* pd;
    while(d) {
        free(d->key);
        free(d->value);
        pd = d->next;
        free(d);
        d = pd;
    }
}

struct curl_slist* parseHeader(request_data* header) {
    struct curl_slist *list = NULL;
    request_data* iter = header;
    while (iter->next) {
        char* data_str = strdupex("", strlen(iter->next->key) + strlen(iter->next->value) + 1);
        strcpy(data_str, iter->next->key);
        strcat(data_str, "=");
        strcat(data_str, iter->next->value);
        list = curl_slist_append(list, data_str);
        free(data_str);
        iter = iter->next;
    } return list;
}

char* parseData(request_data* data, const char* join) {
    
    // Get the length of the final string
    int len = 0;
    request_data* iter = data;
    while (iter->next) {
        if (len != 0) len += strlen(join);
        len += strlen(iter->next->key) + strlen(iter->next->value) + 1;
        iter = iter->next;
    } if (len < 0) len = 0; 
    char* data_str = strdupex("", len);

    // Concatenate all strings
    iter = data;
    while (iter->next) {
        if (data_str[0] != '\0') strcat(data_str, join);
        strcat(data_str, iter->next->key);
        strcat(data_str, "=");
        strcat(data_str, iter->next->value);
        iter = iter->next;
    } return data_str;
}