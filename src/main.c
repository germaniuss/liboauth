#include "OAuth.h";

int main(void) {

    OAuth* oauth = oauth_create();
    oauth_config_dir(oauth, "", "TEST");
    oauth_load(oauth);
    oauth_append_header(oauth, "Key", "Value");
    oauth_save(oauth);
    return 0;
}