#include <string.h>
#include <stdlib.h>
#include "base64.h"

char* base64_url_random(size_t size) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    char *str = (char*) malloc(size + 1);
    if (size) {
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        } str[size] = '\0';
    } return str;
}

char* base64_url_encode(const char *plain) {

    if (plain == NULL) return NULL;

    const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    int len = strlen(plain);
    char* encoded = (char*) malloc(len);

    char *p = encoded;

    int i;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = base64[(plain[i] >> 2) & 0x3F];
        *p++ = base64[((plain[i] & 0x3) << 4) | ((int) (plain[i + 1] & 0xF0) >> 4)];
        *p++ = base64[((plain[i + 1] & 0xF) << 2) | ((int) (plain[i + 2] & 0xC0) >> 6)];
        *p++ = base64[plain[i + 2] & 0x3F];
    } 

    if (i < len) {
        *p++ = base64[(plain[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = base64[((plain[i] & 0x3) << 4)];
        } else {
            *p++ = base64[((plain[i] & 0x3) << 4) | ((int) (plain[i + 1] & 0xF0) >> 4)];
            *p++ = base64[((plain[i + 1] & 0xF) << 2)];
        }
    }

    *p++ = '\0';
    return encoded;
}