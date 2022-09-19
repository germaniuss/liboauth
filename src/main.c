#include "OAuth.h";

int main(void) {

    OAuth* oauth = oauth_create();
    oauth_config_dir(oauth, "", "TEST");
    oauth_cache_dir(oauth, "", "TEST");
    oauth_load(oauth);
    
    oauth_set_options(oauth, REQUEST_CACHE | REQUEST_ASYNC);
    response_data* response = oauth_request(oauth, GET, "https://api.myanimelist.net/v2/anime/season/2017/summer");
    free(response);
    response = oauth_request(oauth, GET, "https://api.myanimelist.net/v2/anime/season/2017/summer");

    oauth_save(oauth);
    return 0;
}