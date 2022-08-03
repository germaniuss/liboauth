#include "OAuth.h"
#include <pthread.h>

enum MHD_Result oauth_get_code (
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method, const char *version,
    const char *upload_data,
    size_t *upload_data_size, void **con_cls) 
{
    OAuth* oauth = (OAuth*) cls;    
    char* code = strdupex(MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code"), 0);
    response_data* token_response = oauth_post_token(oauth, code); 
    free(code);

    // this is assumed to be a json for now (though I will later parse and check the header content-type)
    enum{ MAX_FIELDS = 10 };
    json_t pool[ MAX_FIELDS ];
    json_t const* parent = json_create(token_response->data, pool, MAX_FIELDS);

    ht_set(oauth->params, "token_type", strdupex(json_getValueWrap(parent, "token_type"), 0));
    ht_set(oauth->params, "access_token", strdupex(json_getValueWrap(parent, "access_token"), 0));
    ht_set(oauth->params, "refresh_token", strdupex(json_getValueWrap(parent, "refresh_token"), 0));
    ht_set(oauth->params, "expires_in", strdupex(json_getValueWrap(parent, "expires_in"), 0));

    /////////////////////////////////////////////////////////////////////

    ht_get(oauth->params, "token_type");

    char* str = strdupex(ht_get(oauth->params, "token_type"), strlen(ht_get(oauth->params, "access_token")) + 1);
    strcat(str, " ");
    strcat(str, ht_get(oauth->params, "access_token"));
    ht_set(oauth->header, "Authorization", str);
    free(str);

    oauth->authed = true;

    const char *page  = "<HTML><HEAD>Return to the app now.</BODY></HTML>";
    struct MHD_Response *response;
    int ret;
    response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

OAuth* oauth_create() {
    OAuth* oauth = (OAuth*) malloc(sizeof(OAuth));
    oauth->params = ht_create();
    oauth->data = ht_create();
    oauth->header = ht_create();
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    ht_destroy(oauth->params);
    ht_destroy(oauth->data);
    ht_destroy(oauth->header);
    if (oauth->code_challenge) free(oauth->code_challenge);
    if (oauth->code_verifier) free(oauth->code_verifier);
}

bool oauth_start(OAuth* oauth) {

    if (
        !ht_get(oauth->params, "base_auth_url") || 
        !ht_get(oauth->params, "base_token_url")|| 
        !ht_get(oauth->params, "client_id")
    ) return false;

    oauth_gen_challenge(oauth);
    pthread_t th;
    request_params params;
    params.port = 8000;
    params.sec = 30;
    params.request_callback = &oauth_get_code;
    params.data = oauth;
    char* url = oauth_auth_url(oauth);
    pthread_create(&th, NULL, acceptSingleRequest, (void*) &params);
    openBrowser(url);
    free(url);
    pthread_join(th, NULL);
}

bool oauth_gen_challenge(OAuth* oauth) {
    
    if (!ht_get(oauth->params, "code_challenge_method"))
        return false;

    if (strcmp(ht_get(oauth->params, "code_challenge_method"), "plain") == 0) {
        oauth->code_verifier = base64_url_random(128);
        oauth->code_challenge = strdupex(oauth->code_verifier, 0);
        return true;
    } else if (strcmp(ht_get(oauth->params, "code_challenge_method"), "S256") == 0) {
        oauth->code_verifier = base64_url_random(32);
        uint8_t hash[32];
        char hash_string[65];
        calc_sha_256(hash, oauth->code_verifier, strlen(oauth->code_verifier));
        hash_to_string(hash_string, hash);
        oauth->code_challenge = base64_url_encode(hash_string);
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth) {

    if (!ht_get(oauth->params, "base_auth_url") || !ht_get(oauth->params, "client_id"))
        return NULL;

    ht* data = ht_copy(oauth->data);
    ht_set(data, "client_id", ht_get(oauth->params, "client_id"));
    ht_set(data, "response_type", "code");
    if (((char*)ht_get(oauth->params, "redirect_uri"))[0] != '\0')
        ht_set(data, "redirect_uri", ht_get(oauth->params, "redirect_uri"));
    if (((char*)ht_get(oauth->params, "code_challenge_method"))[0] != '\0') {
        ht_set(data, "code_challenge_method", ht_get(oauth->params, "code_challenge_method"));
        ht_set(data, "code_challenge", oauth->code_challenge);
    }

    char* data_str = parseData(data, "\\&");
    char* authURL = strdupex(ht_get(oauth->params, "base_auth_url"), strlen(data_str) + (data_str[0] != '\0'));
    if (data_str[0] != '\0') {
        strcat(authURL, "?");
        strcat(authURL, data_str);
    }; free(data_str);
    ht_destroy(data);
    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* code) {

    if (!ht_get(oauth->params, "base_token_url") || code == NULL)
        return NULL;

    ht* data = ht_copy(oauth->data);
    ht* header = ht_copy(oauth->header);

    if (((char*)ht_get(oauth->params, "client_secret"))[0] != '\0') 
        ht_set(data, "client_secret", ht_get(oauth->params, "client_secret"));
    if (((char*)ht_get(oauth->params, "redirect_uri"))[0] != '\0') 
        ht_set(data, "redirect_uri", ht_get(oauth->params, "redirect_uri"));
    if (oauth->code_verifier[0] != '\0') 
        ht_set(data, "code_verifier", oauth->code_verifier);

    ht_set(data, "code", code);
    ht_set(data, "client_id", ht_get(oauth->params, "client_id"));
    ht_set(data, "grant_type", "authorization_code");
    response_data* value = oauth_request(NULL, POST, ht_get(oauth->params, "base_token_url"), header, data);
    ht_destroy(data);
    ht_destroy(header);
    return value;
}

void oauth_set_param(OAuth* oauth, const char* key, char* value) {
    ht_set(oauth->params, key, value);
}

void oauth_set_header(OAuth* oauth, ht* header) {\
    ht_destroy(oauth->header);
    oauth->header = header;
}

void oauth_set_data(OAuth* oauth, ht* data) {
    ht_destroy(oauth->data);
    oauth->data = data;
}

response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, ht* header, ht* data) {

    if (oauth != NULL) {
        data = oauth->data;
        header = oauth->header;
    } else {
        if (!data) data = ht_create();
        if (!header) header = ht_create();
    }

    data_t* storage = data_create();
    data_t* header_data = data_create();
    response_data* resp_data = (response_data*) malloc(sizeof(response_data));

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    if (curl) {
        char* data_str = parseData(data, "&");
        struct curl_slist* header_list = parseHeader(header);
        // Set the URL, header and callback function
        curl_easy_setopt(curl, CURLOPT_URL, endpoint);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, storage);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, process_response);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, header_data);
              
        /* Now specify the POST/DELETE/PUT/PATCH data */
        if (method != GET) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, REQUEST_STRING[method]);
            curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, data_str);
        }
        
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

int oauth_load(OAuth* oauth, const char* dir, const char* name) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    char* full_dir;
    if (dir[0] == '\0') full_dir = strdupex(dir, strlen(name) + 5);
    else {
        full_dir = strdupex(dir, strlen(name) + 6);
        strcat(full_dir, "/");
    }
    strcat(full_dir, name);
    strcat(full_dir, ".yaml");

    FILE * fp = fopen(full_dir, "r");
    free(full_dir);
    if (fp == NULL) return NULL;

    while ((read = getline(&line, &len, fp)) != -1) {
        char* key_trim = trim(strtok(line, ":"));
        ht_set(oauth->params, key_trim, trim(strtok(NULL, "")));
        free(key_trim);
    }

    fclose(fp);
    if (line) free(line);
    return oauth;
}

int oauth_save(OAuth* oauth, const char* dir, const char* name) {
    char* full_dir;
    if (dir[0] == '\0') full_dir = strdupex(dir, strlen(name) + 5);
    else {
        full_dir = strdupex(dir, strlen(name) + 6);
        strcat(full_dir, "/");
    }
    strcat(full_dir, name);
    strcat(full_dir, ".yaml");
    FILE *fp = fopen(full_dir, "w");
    free(full_dir);
    if (fp == NULL) {
        printf("Error opening the file %s", dir);
        return NULL;
    }
    // write to the text file
    hti iter = ht_iterator(oauth->params);
    while (ht_next(&iter))
        fprintf(fp, "%s: %s\n", iter.key, iter.value);

    // close the file
    fclose(fp);
    return 1;
}

int main() {
    srand(time(NULL));
    OAuth* oauth = oauth_create();
    oauth_load(oauth, "", "MAL");
    oauth_start(oauth);
    oauth_save(oauth, "", "MAL");
    oauth_delete(oauth);
}