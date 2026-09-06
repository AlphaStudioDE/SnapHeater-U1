"""Execute the production provisioning worker with offline Wi-Fi/DHCP/NVS faults."""
import unittest
import test_audit_regressions as audit
from test_audit_regressions import COMMON, function


class WifiSetupTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_dhcp_auth_missing_network_timeout_and_restore(self):
        self.compile(COMMON + r'''
#include <stdatomic.h>
#include "settings_store.h"
#undef FAILED
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 1102
#define CONNECTED 1
#define FAILED 2
#define SCANNED 4
#define STOPPED 8
#define SETUP_TIMEOUT_MS 15000
#define AP_LIMIT 8
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
#define WIFI_IF_STA 0
#define WIFI_AUTH_WPA2_PSK 3
#define WIFI_AUTH_OPEN 0
#define WPA3_SAE_PWE_BOTH 3
#define WIFI_REASON_AUTH_FAIL 202
#define WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT 15
#define WIFI_REASON_HANDSHAKE_TIMEOUT 204
#define WIFI_REASON_NO_AP_FOUND 201
typedef unsigned EventBits_t;
typedef struct { bool scan; char ssid[33]; char password[65]; } setup_request_t;
typedef struct { struct { char ssid[32], password[64]; struct {int authmode;} threshold; int sae_pwe_h2e; } sta; } wifi_config_t;
typedef struct { bool show_hidden; } wifi_scan_config_t;
typedef struct { char ssid[33]; int rssi, authmode; } wifi_ap_record_t;
static int events, requests, scenario, dequeued, writes, retries, maintenance_ended;
static SemaphoreHandle_t status_lock;
static atomic_bool testing, busy, connected;
static atomic_int disconnect_reason;
static wifi_ap_record_t networks[AP_LIMIT];
static uint16_t network_count;
static char phase[24];
static wifi_config_t active;
static void set_phase(const char *p) {snprintf(phase,sizeof(phase),"%s",p);}
shu1_control_guard_t shu1_control_guard_begin(void) {shu1_control_guard_t g={0};return g;}
void shu1_control_guard_end(shu1_control_guard_t *g) {}
void shu1_control_maintenance_end(void) {maintenance_ended++;}
static void vTaskDelete(void *p) {}
static int xQueueReceive(int q,setup_request_t *out,unsigned timeout) {
    if(dequeued++) return 0;
    memset(out,0,sizeof(*out));strcpy(out->ssid,"candidate");strcpy(out->password,"candidate-secret");
    out->scan=scenario>=7;return pdTRUE;
}
static void xEventGroupClearBits(int e,unsigned bits) {}
static unsigned xEventGroupWaitBits(int e,unsigned bits,int clear,int all,unsigned ticks) {
    if(bits==STOPPED) return STOPPED;
    assert(ticks==SETUP_TIMEOUT_MS);
    if(bits==SCANNED)return scenario==7 ? SCANNED:0;
    if(scenario==0 || scenario==4) {atomic_store(&connected,true);return CONNECTED;}
    if(scenario==3)return 0;
    atomic_store(&disconnect_reason,scenario==1 ? WIFI_REASON_AUTH_FAIL:
        scenario==2 ? WIFI_REASON_NO_AP_FOUND:WIFI_REASON_HANDSHAKE_TIMEOUT);
    return FAILED;
}
static int esp_wifi_stop(void) {atomic_store(&connected,false);return ESP_OK;}
static int esp_wifi_start(void) {return ESP_OK;}
static int esp_wifi_connect(void) {return scenario==5 ? ESP_FAIL:ESP_OK;}
static int esp_wifi_set_config(int iface,const wifi_config_t *cfg) {active=*cfg;return ESP_OK;}
static int esp_wifi_get_config(int iface,wifi_config_t *cfg) {*cfg=active;return ESP_OK;}
static int esp_wifi_scan_start(const wifi_scan_config_t *cfg,bool block) {assert(!block);return ESP_OK;}
static int esp_wifi_scan_stop(void) {return ESP_OK;}
static int esp_wifi_clear_ap_list(void) {return ESP_OK;}
static int esp_wifi_scan_get_ap_records(uint16_t *n,wifi_ap_record_t *found) {*n=1;strcpy(found[0].ssid,"test-ap");return ESP_OK;}
void shu1_device_config_defaults(shu1_device_config_t *cfg) {memset(cfg,0,sizeof(*cfg));}
esp_err_t shu1_settings_store_load_device_config(shu1_device_config_t *cfg) {return ESP_OK;}
esp_err_t shu1_settings_store_save_device_config(const shu1_device_config_t *cfg) {
    assert(atomic_load(&connected));assert(strcmp(cfg->wifi_ssid,"candidate")==0);
    assert(strcmp(cfg->wifi_password,"candidate-secret")==0);
    writes++;return scenario==4 ? ESP_FAIL:ESP_OK;
}
''' + function("wifi_sta.c", "static const char *failure_phase(")
            + function("wifi_sta.c", "static bool stop_wifi(")
            + function("wifi_sta.c", "static void setup_worker(") + r'''
int main(void) {
    status_lock=xSemaphoreCreateMutex();assert(status_lock);
    const char *expected[]={"connected","auth_failed","not_found","timeout","save_failed",
                            "connection_failed","auth_failed","scan_complete","timeout"};
    for(scenario=0;scenario<9;scenario++) {
        dequeued=writes=maintenance_ended=0;
        atomic_store(&busy,true);atomic_store(&connected,false);
        memset(&active,0,sizeof(active));strcpy(active.sta.ssid,"previous");
        setup_worker(NULL);
        assert(strcmp(phase,expected[scenario])==0);
        assert(writes==((scenario==0 || scenario==4) ? 1:0));
        assert(strcmp(active.sta.ssid,scenario==0 ? "candidate":"previous")==0);
        assert(!atomic_load(&busy) && maintenance_ended==1);
    }
    puts("Wi-Fi worker: DHCP success, authentication, missing AP, timeouts, persistence failure, restore and scan PASS");
    return 0;
}
''')
