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

const char* json_value(const json_t* parent, const char* key);
char* strdupex(const char* str, size_t val);
int openBrowser(const char* url);

// THIS IS RELATED TO REQUESTS

typedef struct request_params {
    int port;
    int sec;
    MHD_AccessHandlerCallback request_callback;
    void* data;
} request_params;

void request_completed (void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode code);
void* acceptSingleRequest(void* params);

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

data_t* data_create();
void data_clean(data_t* d);
char* process_response_data(data_t* storage);
size_t process_response(void *ptr, size_t size, size_t nmemb, void *userdata);

// THIS IS ALL RELATED TO HEADER AND DATA

typedef struct request_data {
    char* key;
    char* value;
    struct request_data* next;
    struct request_data* last;
} request_data;

request_data* request_data_create();
void request_data_append(request_data* data, const char* key, const char* value);
request_data* request_data_copy(request_data* data);
void request_data_clean(request_data* d);

struct curl_slist* parseHeader(request_data* header);
char* parseData(request_data* data, const char* join);

// Base 64 encoding stuff
char* base64_url_random(size_t size);
char* base64_url_encode(const char *plain);