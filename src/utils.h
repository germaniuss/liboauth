#pragma once
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <microhttpd.h>

#include "base64.h"
#include "sha-256.h"
#include "tiny-json.h"
#include "ht.h"

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

char* strdupex(const char* str, int val);
char * trim(char * s);
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

struct curl_slist* parseHeader(ht* header);
char* parseData(ht* data, const char* join);