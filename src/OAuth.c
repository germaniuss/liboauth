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
    ht_set(oauth->params, "code", strdupex(MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code"), 0));
    
    // THIS PART IS NOT IDEAL RIGHT NOW (THOUGH I DO NOT LIKE HOW IT WORKS IN GENERAL RN)

    response_data* token_response = oauth_post_token(oauth, "", ""); // this is assumed to be a json for now (though I will later parse and check the header content-type)
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

OAuth* oauth_create(const char* client_id, const char* client_secret, const char* redirect_uri) {
    OAuth* oauth = (OAuth*) malloc(sizeof(OAuth));
    oauth->params = ht_create();
    ht_set(oauth->params, "client_id", strdupex(client_id, 0));
    ht_set(oauth->params, "client_secret", strdupex(client_secret, 0));
    ht_set(oauth->params, "redirect_uri", strdupex(redirect_uri, 0));
    oauth->data = ht_create();
    oauth->header = ht_create();
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    ht_destroy(oauth->params);
    ht_destroy(oauth->data);
    ht_destroy(oauth->header);
}

void oauth_start(OAuth* oauth, const char* baseAuthURL, const char* baseTokenURL, const char* code_challenge_method) {
    ht_set(oauth->params, "base_auth_url", strdupex(baseAuthURL, 0));
    ht_set(oauth->params, "base_token_url", strdupex(baseTokenURL, 0));
    oauth_gen_challenge(oauth, code_challenge_method);
    pthread_t th;
    request_params params;
    params.port = 8000;
    params.sec = 30;
    params.request_callback = &oauth_get_code;
    params.data = oauth;
    pthread_create(&th, NULL, acceptSingleRequest, (void*) &params);
    openBrowser(oauth_auth_url(oauth, ""));
    pthread_join(th, NULL);
}

bool oauth_gen_challenge(OAuth* oauth, const char* code_challenge_method) {
    if (code_challenge_method[0] != '\0') ht_set(oauth->params, "code_challenge_method", strdupex(code_challenge_method, 0));
    if (strcmp(ht_get(oauth->params, "code_challenge_method"), "plain") == 0) {
        char* code_verifier = base64_url_random(128);
        ht_set(oauth->params, "code_verifier", code_verifier);
        ht_set(oauth->params, "code_challenge", code_verifier);
        return true;
    } else if (strcmp(ht_get(oauth->params, "code_challenge_method"), "S256") == 0) {
        char* code_verifier = base64_url_random(32);
        ht_set(oauth->params, "code_verifier", code_verifier);
        uint8_t hash[32];
        char hash_string[65];
        calc_sha_256(hash, code_verifier, strlen(code_verifier));
        hash_to_string(hash_string, hash);
        ht_set(oauth->params, "code_challenge", base64_url_encode(hash_string));
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth, const char* baseAuthURL) {
    if (((char*)ht_get(oauth->params, "client_id"))[0] == '\0') return NULL;
    ht* data = ht_copy(oauth->data);
    if (baseAuthURL[0] == '\0') baseAuthURL = ht_get(oauth->params, "base_auth_url");
    ht_set(data, "client_id", ht_get(oauth->params, "client_id"));
    ht_set(data, "response_type", "code");
    if (((char*)ht_get(oauth->params, "redirect_uri"))[0] != '\0')
        ht_set(data, "redirect_uri", ht_get(oauth->params, "redirect_uri"));
    if (((char*)ht_get(oauth->params, "code_challenge_method"))[0] != '\0') {
        ht_set(data, "code_challenge_method", ht_get(oauth->params, "code_challenge_method"));
        ht_set(data, "code_challenge", ht_get(oauth->params, "code_challenge"));
    }

    char* data_str = parseData(data, "\\&");
    char* authURL = strdupex(baseAuthURL, strlen(data_str) + (data_str[0] != '\0'));
    if (data_str[0] != '\0') {
        strcat(authURL, "?");
        strcat(authURL, data_str);
    }; free(data_str);
    ht_destroy(data);
    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* baseTokenURL, const char* newcode) {
    ht* data = ht_copy(oauth->data);
    ht* header = ht_copy(oauth->header);
    if (baseTokenURL[0] == '\0') baseTokenURL = strdupex(ht_get(oauth->params, "base_token_url"), 0);
    if (newcode[0] != '\0' && ht_get(oauth->params, "code") != NULL)
        ht_set(oauth->params, "code", strdupex(newcode, 0));
    else if (newcode[0] != '\0') strcpy(ht_get(oauth->params, "code"), newcode);
    if (((char*)ht_get(oauth->params, "code"))[0] == '\0') {
        fprintf(stderr, "Auth Code not yet retrieved, please execute auth step befor this one");
        return NULL;
    }
    if (((char*)ht_get(oauth->params, "client_secret"))[0] != '\0') 
        ht_set(data, "client_secret", ht_get(oauth->params, "client_secret"));
    if (((char*)ht_get(oauth->params, "code_verifier"))[0] != '\0') 
        ht_set(data, "code_verifier", ht_get(oauth->params, "code_verifier"));
    if (((char*)ht_get(oauth->params, "redirect_uri"))[0] != '\0') 
        ht_set(data, "redirect_uri", ht_get(oauth->params, "redirect_uri"));

    ht_set(data, "code", ht_get(oauth->params, "code"));
    ht_set(data, "client_id", ht_get(oauth->params, "client_id"));
    ht_set(data, "grant_type", "authorization_code");
    response_data* value = oauth_request(NULL, POST, baseTokenURL, header, data);
    ht_destroy(data);
    ht_destroy(header);
    return value;
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
    OAuth* oauth = oauth_create("ed7f347e239153101c9e6fc6b5bdfece", "", "");
    oauth_start(oauth, "https://myanimelist.net/v1/oauth2/authorize", "https://myanimelist.net/v1/oauth2/token", "plain");
    oauth_save(oauth, "", "MAL");
    oauth_delete(oauth);
}