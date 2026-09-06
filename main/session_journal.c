#include "session_journal.h"
#include "app_state.h"
#include "safety_latch.h"
#include "event_log.h"
#include "nvs.h"
#include <stdatomic.h>

static atomic_bool prepared;
static bool dirty; // sole storage worker after boot

bool shu1_session_journal_ready(void) {return atomic_load(&prepared);}

esp_err_t shu1_session_journal_init(void) {
    atomic_store(&prepared,false);
    dirty=false;
    nvs_handle_t h;
    esp_err_t err=nvs_open("shu1_session",NVS_READONLY,&h);
    if(err==ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if(err!=ESP_OK) return err;
    uint8_t marker=0;
    err=nvs_get_u8(h,"dirty",&marker);
    nvs_close(h);
    if(err==ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if(err!=ESP_OK || marker>1) return ESP_ERR_INVALID_STATE;
    dirty=marker!=0;
    if(dirty) {
        shu1_safety_latch_trip_volatile(SHU1_HEATER_PERSISTED_FAULT);
        shu1_event_log_add("critical","session_interrupted","Previous heating session did not close safely; explicit safe fault clear required");
    }
    return ESP_OK;
}

static esp_err_t write_marker(bool value) {
    nvs_handle_t h;
    esp_err_t err=nvs_open("shu1_session",NVS_READWRITE,&h);
    if(err!=ESP_OK) return err;
    err=nvs_set_u8(h,"dirty",value ? 1:0);
    if(err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err;
}

void shu1_session_journal_service(void) {
    SHU1_CONTROL_GUARD(guard);
    if(shu1_safety_latch_is_set() || shu1_safety_latch_is_inhibited()) return;
    shu1_settings_t st=shu1_state_get_settings();
    if(st.work_on || st.scheduled_preheat_enabled) {
        if(atomic_load(&prepared)) return;
        // SSR admission remains false throughout storage. No stale job is replayed.
        shu1_control_guard_end(&guard);
        esp_err_t err=write_marker(true);
        guard=shu1_control_guard_begin();
        if(err==ESP_OK) {dirty=true;atomic_store(&prepared,true);}
        else shu1_safety_latch_inhibit();
        return;
    }
    if(!dirty || !shu1_control_checkpoint_begin()) return;
    // Maintenance excludes a new ON while the durable marker is cleared.
    atomic_store(&prepared,false);
    shu1_control_guard_end(&guard);
    esp_err_t err=write_marker(false);
    guard=shu1_control_guard_begin();
    if(err==ESP_OK) dirty=false;
    else shu1_safety_latch_inhibit();
    shu1_control_maintenance_end();
}
