#pragma once
#include "settings_store.h"
#include "cJSON.h"
#include <string.h>
#include <math.h>

static inline bool shu1_printer_setup_parse(const cJSON *root, shu1_device_config_t *cfg, char id[33]) {
    if (!cJSON_IsObject(root)) return false;
    for (const cJSON *p=root->child;p;p=p->next) {
        if(!p->string || (strcmp(p->string,"printer_setup") && strcmp(p->string,"expected_revision") && strcmp(p->string,"lease_id"))) return false;
        for(const cJSON *q=p->next;q;q=q->next) if(q->string && !strcmp(p->string,q->string)) return false;
    }
    const cJSON *setup=cJSON_GetObjectItemCaseSensitive(root,"printer_setup");
    if(!cJSON_IsObject(setup)) return false;
    for(const cJSON *p=setup->child;p;p=p->next) {
        if(!p->string || (strcmp(p->string,"host") && strcmp(p->string,"port") && strcmp(p->string,"api_key") && strcmp(p->string,"id"))) return false;
        for(const cJSON *q=p->next;q;q=q->next) if(q->string && !strcmp(p->string,q->string)) return false;
    }
    const char *host=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(setup,"host"));
    const char *key=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(setup,"api_key"));
    const char *request=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(setup,"id"));
    const cJSON *port=cJSON_GetObjectItemCaseSensitive(setup,"port");
    if(!host || strlen(host)<1 || strlen(host)>63 || !key || strlen(key)>128 || !request || strlen(request)!=32 ||
       !cJSON_IsNumber(port) || !isfinite(port->valuedouble) || port->valuedouble<1 || port->valuedouble>65535 || floor(port->valuedouble)!=port->valuedouble) return false;
    if(strspn(host,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-")!=strlen(host) ||
       strspn(key,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=strlen(key) ||
       strspn(request,"0123456789abcdefABCDEF")!=32) return false;
    memset(cfg,0,sizeof(*cfg));
    strcpy(cfg->moonraker_host,host);cfg->moonraker_port=port->valueint;
    strcpy(cfg->moonraker_api_key,key);strcpy(id,request);
    return true;
}

static inline bool shu1_printer_control_subset(const cJSON *status) {
    const cJSON *ps=cJSON_GetObjectItemCaseSensitive(status,"print_stats");
    const cJSON *bed=cJSON_GetObjectItemCaseSensitive(status,"heater_bed");
    const cJSON *wh=cJSON_GetObjectItemCaseSensitive(status,"webhooks");
    const char *state=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(ps,"state"));
    const char *ready=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(wh,"state"));
    const cJSON *temp=cJSON_GetObjectItemCaseSensitive(bed,"temperature");
    const cJSON *target=cJSON_GetObjectItemCaseSensitive(bed,"target");
    return state && state[0] && ready && !strcmp(ready,"ready") &&
        cJSON_IsNumber(temp) && isfinite(temp->valuedouble) &&
        cJSON_IsNumber(target) && isfinite(target->valuedouble);
}
