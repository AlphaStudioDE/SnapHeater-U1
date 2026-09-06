#include "recorder.h"
#include "app_state.h"
#include "settings_store.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_INTERVAL_S 10U
#define HISTORY_WINDOW_S (120U * 60U)
#define CAP (HISTORY_WINDOW_S / SAMPLE_INTERVAL_S)
#define PAGE 32
typedef struct { uint32_t seq, seconds; int16_t chamber, ptc, target; uint8_t flags, fault; } record_t;
_Static_assert(sizeof(record_t)==16, "Recorder RAM budget changed");
static record_t records[CAP];
_Static_assert(sizeof(records)==11520, "Two-hour history must use 11.25 KiB");
static uint32_t sequence;
static char boot[17];
static SemaphoreHandle_t lock;
static uint64_t heater_base, fan_base, heater_total, fan_total;
static bool storage_ok;
static bool recorder_ready;
static bool last_checkpoint_ok=true;
static int16_t temperature(float t) { return isfinite(t) && t>-3276 && t<3276 ? (int16_t)lroundf(t*10) : INT16_MIN; }
static bool checkpoint_begin(void) {
    SHU1_CONTROL_GUARD(guard);
    return shu1_control_checkpoint_begin();
}
static void checkpoint_end(void) {
    SHU1_CONTROL_GUARD(guard);
    shu1_control_maintenance_end();
}

static void recorder_task(void *arg) {
    shu1_state_t *state=arg; // Keep the large copied state off the task stack.
    uint32_t sampled=UINT32_MAX, persisted=0;
    uint64_t saved_heater=heater_base, saved_fan=fan_base;
    while (true) {
        const uint32_t seconds=(uint32_t)(esp_timer_get_time()/1000000);
        // Sampling never acquires the policy guard or writes any actuator.
        shu1_state_get(state);
        const shu1_runtime_t *rt=&state->runtime;
        const shu1_settings_t *st=&state->settings;
        xSemaphoreTake(lock,portMAX_DELAY);
        heater_total=heater_base+rt->heater_usage_35c_ms;
        fan_total=fan_base+rt->fan_on_accum_ms;
        if (sampled==UINT32_MAX || seconds-sampled>=SAMPLE_INTERVAL_S) {
            sampled=seconds;
            record_t r={.seq=++sequence,.seconds=seconds,
                .chamber=rt->chamber_sensor_status==SHU1_SENSOR_OK?temperature(rt->chamber_temp_c):INT16_MIN,
                .ptc=rt->ptc_sensor_status==SHU1_SENSOR_OK?temperature(rt->ptc_temp_c):INT16_MIN,
                .target=temperature(st->work_on && !st->user_paused?rt->heater_effective_target_c:0),
                .flags=(rt->heater_output_on?1:0)|(rt->fan_output_on?2:0)|(st->user_paused?4:0),
                .fault=(uint8_t)rt->heater_fault};
            records[(r.seq-1)%CAP]=r;
        }
        uint64_t totals[2]={heater_total,fan_total};
        xSemaphoreGive(lock);
        // Never add flash/cache stalls during heating or cooldown. Maintenance
        // atomically excludes other channels from starting while NVS is written.
        if (storage_ok && (totals[0]!=saved_heater || totals[1]!=saved_fan) &&
            seconds-persisted>=60 && checkpoint_begin()) {
            nvs_handle_t h;
            esp_err_t err=nvs_open("shu1_usage",NVS_READWRITE,&h);
            if (err==ESP_OK) {
                err=nvs_set_blob(h,"totals",totals,sizeof(totals));
                if (err==ESP_OK) err=nvs_commit(h);
                nvs_close(h);
            }
            checkpoint_end();
            if(err==ESP_OK) {saved_heater=totals[0];saved_fan=totals[1];}
            xSemaphoreTake(lock,portMAX_DELAY);
            last_checkpoint_ok=err==ESP_OK;
            xSemaphoreGive(lock);
            persisted=seconds;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
esp_err_t shu1_recorder_start(void) {
    shu1_state_t *state=calloc(1,sizeof(*state));
    if (!state) return ESP_ERR_NO_MEM;
    lock=xSemaphoreCreateMutex();
    if (!lock) {free(state);return ESP_ERR_NO_MEM;}
    snprintf(boot,sizeof(boot),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    uint64_t totals[2]={0,0}; size_t size=sizeof(totals); nvs_handle_t h;
    esp_err_t err=nvs_open("shu1_usage",NVS_READONLY,&h);
    if(err==ESP_OK) { err=nvs_get_blob(h,"totals",totals,&size); nvs_close(h); }
    storage_ok=err==ESP_ERR_NVS_NOT_FOUND || (err==ESP_OK && size==sizeof(totals));
    if(err==ESP_OK && size==sizeof(totals)) { heater_base=totals[0]; fan_base=totals[1]; }
    heater_total=heater_base; fan_total=fan_base;
    if (xTaskCreate(recorder_task,"shu1_recorder",8192,state,1,NULL)!=pdPASS) {
        free(state);
        vSemaphoreDelete(lock);
        lock=NULL;
        return ESP_ERR_NO_MEM;
    }
    recorder_ready=true;
    return ESP_OK;
}
bool shu1_recorder_usage(cJSON *root) {
    if (!root) return false;
    if (!recorder_ready) return cJSON_AddBoolToObject(root,"usage_available",false)!=NULL;
    xSemaphoreTake(lock,portMAX_DELAY);
    double heater=(double)heater_total, fan=(double)fan_total;
    bool available=storage_ok && last_checkpoint_ok;
    xSemaphoreGive(lock);
    return cJSON_AddBoolToObject(root,"usage_available",available)!=NULL &&
        cJSON_AddNumberToObject(root,"usage_heater_ms",heater)!=NULL &&
        cJSON_AddNumberToObject(root,"usage_filter_ms",fan)!=NULL;
}
cJSON *shu1_recorder_page(uint32_t after) {
    cJSON *root=cJSON_CreateObject();
    if(!root) return NULL;
    if(!recorder_ready) {cJSON_Delete(root);return NULL;}
    record_t page[PAGE]; unsigned count=0;
    xSemaphoreTake(lock,portMAX_DELAY);
    uint32_t last=sequence, first=last>CAP ? last-CAP+1:1;
    if(after>last) after=0; // New boot; caller also keys by boot ID.
    bool lost=after<first-1;
    uint32_t next=after;
    for(uint32_t i=after+1<first?first:after+1;i<=last && count<PAGE;i++) {
        page[count++]=records[(i-1)%CAP]; next=i;
    }
    const uint32_t now_s=(uint32_t)(esp_timer_get_time()/1000000);
    xSemaphoreGive(lock);
    char id[13]; shu1_device_id(id,sizeof(id));
    bool ok=cJSON_AddStringToObject(root,"device_id",id)!=NULL &&
        cJSON_AddStringToObject(root,"boot",boot)!=NULL &&
        cJSON_AddNumberToObject(root,"now_s",now_s)!=NULL &&
        cJSON_AddNumberToObject(root,"first",first)!=NULL &&
        cJSON_AddNumberToObject(root,"next",next)!=NULL &&
        cJSON_AddNumberToObject(root,"last",last)!=NULL &&
        cJSON_AddBoolToObject(root,"gap",lost)!=NULL;
    cJSON *rows=cJSON_AddArrayToObject(root,"samples");
    ok=ok && rows!=NULL;
    for(unsigned i=0;i<count && ok;i++) {
        record_t *r=&page[i];
        cJSON *row=cJSON_CreateArray();
        if(!row) {ok=false;break;}
        ok=cJSON_AddItemToArray(row,cJSON_CreateNumber(r->seq)) &&
            cJSON_AddItemToArray(row,cJSON_CreateNumber(r->seconds));
        int16_t values[]={r->chamber,r->ptc,r->target};
        for(unsigned j=0;j<3 && ok;j++) ok=cJSON_AddItemToArray(row,values[j]==INT16_MIN?cJSON_CreateNull():cJSON_CreateNumber(values[j]/10.0));
        if(ok) ok=cJSON_AddItemToArray(row,cJSON_CreateNumber(r->flags)) &&
            cJSON_AddItemToArray(row,cJSON_CreateNumber(r->fault));
        cJSON_AddItemToArray(rows,row);
    }
    if(!ok) {cJSON_Delete(root);return NULL;}
    return root;
}
