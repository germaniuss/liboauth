#include <curl/curl.h>
#include <pthread.h>
#include "utils.h"
#include "tiny-json.h"
#include "OAuth.h"

enum MHD_Result oauth_get_code (
    void *cls, struct MHD_Connection *connection,
    const char *url,
    const char *method, const char *version,
    const char *upload_data,
    size_t *upload_data_size, void **con_cls) 
{
    OAuth* oauth = (OAuth*) cls;
    oauth->code = strdupex(MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code"), 0);
        
    response_data* token_response = oauth_post_token(oauth, "", ""); // this is assumed to be a json for now
    enum{ MAX_FIELDS = 10 };
    json_t pool[ MAX_FIELDS ];
    json_t const* parent = json_create(token_response->data, pool, MAX_FIELDS);

    oauth->token_type = strdupex(json_value(parent, "token_type"), 0);
    oauth->access_token = strdupex(json_value(parent, "access_token"), 0);
    oauth->refresh_token = strdupex(json_value(parent, "refresh_token"), 0);
    oauth->expires_in = strdupex(json_value(parent, "expires_in"), 0);

    char* str = strdupex(oauth->token_type, strlen(oauth->access_token) + 1);
    strcat(str, " ");
    strcat(str, oauth->access_token);
    request_data_append(oauth->header, "Authorization", str);
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
    oauth->client_id = strdupex(client_id, 0);
    oauth->client_secret = strdupex(client_secret, 0);
    oauth->redirect_uri = strdupex(redirect_uri, 0);
    oauth->data = request_data_create();
    oauth->header = request_data_create();
    return oauth;
}

void oauth_delete(OAuth* oauth) {
    free(oauth->client_id);
    free(oauth->client_secret);
    free(oauth->redirect_uri);
    if (oauth->code_challenge_method) free(oauth->code_challenge_method);
    if (oauth->code_challenge) free(oauth->code_challenge);
    if (oauth->code_verifier) free(oauth->code_verifier);
    if (oauth->code) free(oauth->code);
    if (oauth->expires_in) free(oauth->expires_in);
    if (oauth->access_token) free(oauth->access_token);
    if (oauth->refresh_token) free(oauth->refresh_token);
    if (oauth->token_type) free(oauth->token_type);
    if (oauth->baseAuthURL) free(oauth->baseAuthURL);
    if (oauth->baseTokenURL) free(oauth->baseTokenURL);
    request_data_clean(oauth->data);
    request_data_clean(oauth->header);
}

void oauth_start(OAuth* oauth, const char* baseAuthURL, const char* baseTokenURL, const char* code_challenge_method) {
    oauth->baseAuthURL = strdupex(baseAuthURL, 0);
    oauth->baseTokenURL = strdupex(baseTokenURL, 0);
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
    if (code_challenge_method[0] != '\0') oauth->code_challenge_method = strdupex(code_challenge_method, 0);
    if (!strcmp(oauth->code_challenge_method, "plain")) {
        oauth->code_verifier = genCodeChallenge(128);
        oauth->code_challenge = strdupex(oauth->code_verifier, 0);
        return true;
    } else if (!strcmp(oauth->code_challenge_method, "SHA256")) {
        oauth->code_verifier = genCodeVerifier();
        oauth->code_challenge = genCodeChallenge(128);
        return true;
    } return false;
}

char* oauth_auth_url(OAuth* oauth, const char* baseAuthURL) {
    request_data* data = request_data_copy(oauth->data);
    if (baseAuthURL[0] == '\0') baseAuthURL = oauth->baseAuthURL;
    request_data_append(data, "client_id", oauth->client_id);
    request_data_append(data, "response_type", "code");
    if (oauth->redirect_uri[0] != '\0')
        request_data_append(data, "redirect_uri", oauth->redirect_uri);
    if (oauth->code_challenge[0] != '\0') {
        request_data_append(data, "code_challenge_method", oauth->code_challenge_method);
        request_data_append(data, "code_challenge", oauth->code_challenge);
    }

    char* data_str = parseData(data, "\\&");
    char* authURL = strdupex(baseAuthURL, strlen(data_str) + (data_str[0] != '\0'));
    if (data_str[0] != '\0') {
        strcat(authURL, "?");
        strcat(authURL, data_str);
    }; free(data_str);
    request_data_clean(data);
    return authURL;
}

response_data* oauth_post_token(OAuth* oauth, const char* baseTokenURL, const char* newcode) {
    request_data* data = request_data_copy(oauth->data);
    request_data* header = request_data_copy(oauth->header);
    if (baseTokenURL[0] == '\0') baseTokenURL = strdupex(oauth->baseTokenURL, 0);
    if (newcode[0] != '\0' && oauth->code != NULL) oauth->code = strdupex(newcode, 0);
    else if (newcode[0] != '\0' && oauth->code == NULL) strcpy(oauth->code, newcode);
    if (oauth->code[0] == '\0') {
        fprintf(stderr, "Auth Code not yet retrieved, please execute auth step befor this one");
        return NULL;
    }
    if (oauth->client_secret[0] != '\0') request_data_append(data, "client_secret", oauth->client_secret);
    if (oauth->code_verifier[0] != '\0') request_data_append(data, "code_verifier", oauth->code_verifier);
    if (oauth->redirect_uri[0] != '\0') request_data_append(data, "redirect_uri", oauth->redirect_uri);
    request_data_append(data, "code", oauth->code);
    request_data_append(data, "client_id", oauth->client_id);
    request_data_append(data, "grant_type", "authorization_code");
    response_data* value = oauth_request(NULL, POST, baseTokenURL, header, data);
    request_data_clean(data);
    request_data_clean(header);
    return value;
}

void oauth_set_header(OAuth* oauth, request_data* header) {\
    request_data_clean(oauth->header);
    oauth->header = header;
}

void oauth_set_data(OAuth* oauth, request_data* data) {
    request_data_clean(oauth->data);
    oauth->data = data;
}

response_data* oauth_request(OAuth* oauth, REQUEST method, const char* endpoint, request_data* header, request_data* data) {

    if (oauth != NULL) {
        data = oauth->data;
        header = oauth->header;
    } else {
        if (!data) data = request_data_create();
        if (!header) header = request_data_create();
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

int main() {
    OAuth* oauth = oauth_create("ed7f347e239153101c9e6fc6b5bdfece", "", "");
    oauth_start(oauth, "https://myanimelist.net/v1/oauth2/authorize", "https://myanimelist.net/v1/oauth2/token", "plain");
    oauth_delete(oauth);
}