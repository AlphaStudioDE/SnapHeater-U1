#pragma once
#include "cJSON.h"
#include <stdbool.h>

/* Iterative preflight also protects builds/tests whose cJSON has a higher limit.
 * Brackets inside escaped strings are data, not nesting. Require an object root. */
static inline cJSON *shu1_json_parse(const char *text) {
    if (!text) return NULL;
    unsigned depth=0;
    bool quoted=false, escaped=false;
    for (const unsigned char *p=(const unsigned char *)text; *p; ++p) {
        if (quoted) {
            if (escaped) escaped=false;
            else if (*p=='\\') escaped=true;
            else if (*p=='"') quoted=false;
        } else if (*p=='"') quoted=true;
        else if (*p=='{' || *p=='[') { if (++depth>16) return NULL; }
        else if (*p=='}' || *p==']') { if (!depth) return NULL; --depth; }
    }
    if (depth || quoted) return NULL;
    cJSON *root=cJSON_ParseWithOpts(text,NULL,true);
    if (!cJSON_IsObject(root)) {cJSON_Delete(root);return NULL;}
    return root;
}
