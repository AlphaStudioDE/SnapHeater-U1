#include "settings_deferred.h"
#include "event_log.h"
#include "safety_latch.h"
#include "session_journal.h"
#include "freertos/task.h"
#include <string.h>

static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static shu1_settings_t pending_settings;
static uint64_t revision, saved_revision;
static bool ready, last_ok=true;

esp_err_t shu1_settings_defer(const shu1_settings_t *settings) {
    if(!settings || !ready) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&lock);
    pending_settings=*settings;
    ++revision;
    portEXIT_CRITICAL(&lock);
    return ESP_OK;
}

void shu1_settings_deferred_status(bool *pending,bool *ok) {
    portENTER_CRITICAL(&lock);
    *pending=revision!=saved_revision;
    *ok=ready && last_ok;
    portEXIT_CRITICAL(&lock);
}

void shu1_settings_deferred_flush(void) {
    // Sole storage worker owns this buffer; keep it off the task stack.
    static shu1_settings_t snapshot;
    uint64_t writing;
    SHU1_CONTROL_GUARD(guard);
    portENTER_CRITICAL(&lock);
    writing=revision;
    bool dirty=writing!=saved_revision;
    portEXIT_CRITICAL(&lock);
    if(!dirty || !shu1_control_checkpoint_begin()) return;
    portENTER_CRITICAL(&lock);
    snapshot=pending_settings;
    portEXIT_CRITICAL(&lock);
    shu1_control_guard_end(&guard);
    esp_err_t err=shu1_settings_store_save_settings(&snapshot);
    // Only acknowledgement metadata is committed, NEVER replay the saved job.
    guard=shu1_control_guard_begin();
    portENTER_CRITICAL(&lock);
    if(err==ESP_OK) saved_revision=writing;
    last_ok=err==ESP_OK;
    portEXIT_CRITICAL(&lock);
    shu1_control_maintenance_end();
    shu1_control_guard_end(&guard);
    if(err!=ESP_OK) shu1_event_log_add("warn","settings_save_failed","Settings remain in RAM; retrying storage when cold and idle");
}

static void storage_task(void *unused) {
    (void)unused;
    unsigned cycles=0;
    for(;;) {
        shu1_safety_latch_service();
        shu1_session_journal_service();
        if(cycles++%100==0) shu1_settings_deferred_flush();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t shu1_settings_deferred_start(void) {
    if(ready) return ESP_OK;
    ready=true;
    if(xTaskCreate(storage_task,"shu1_storage",8192,NULL,1,NULL)!=pdPASS) {
        ready=false;return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
