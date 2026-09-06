#include "ota_storage.h"
#include "nvs.h"

/* Separate namespace: factory settings reset must not erase upload rejection.
 * Missing records allow a pre-existing stock/previous image; unreadable records
 * never grant permission. A marker is committed BEFORE any image writes. */
esp_err_t shu1_ota_slot_pending(const esp_partition_t *slot, bool pending) {
    if (!slot) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err=nvs_open("shu1_ota",NVS_READWRITE,&h);
    if (err!=ESP_OK) return err;
    err=nvs_set_u8(h,slot->label,pending?1:0);
    if (err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err;
}
bool shu1_ota_slot_boot_allowed(const esp_partition_t *slot) {
    if (!slot) return false;
    nvs_handle_t h;
    esp_err_t err=nvs_open("shu1_ota",NVS_READONLY,&h);
    if (err==ESP_ERR_NVS_NOT_FOUND) return true;
    if (err!=ESP_OK) return false;
    uint8_t pending=1;
    err=nvs_get_u8(h,slot->label,&pending);
    nvs_close(h);
    return err==ESP_ERR_NVS_NOT_FOUND || (err==ESP_OK && pending==0);
}
