"""Compile production setup parsing and transaction flow with simulated network/NVS."""
import unittest
import test_audit_regressions as audit

class PrinterSetupTests(unittest.TestCase):
    compile=audit.AuditRegressionTests.compile
    def test_request_is_dedicated_bounded_and_header_safe(self):
        self.compile(audit.COMMON+r'''
#include "printer_setup_rules.h"
int main(void) {
 const char *valid="{\"expected_revision\":1,\"printer_setup\":{\"host\":\"printer.local\",\"port\":7125,\"api_key\":\"abc123\",\"id\":\"0123456789abcdef0123456789abcdef\"}}";
 cJSON *r=cJSON_Parse(valid);shu1_device_config_t cfg;char id[33];
 assert(shu1_printer_setup_parse(r,&cfg,id));assert(!strcmp(cfg.moonraker_api_key,"abc123"));
 cJSON *s=cJSON_GetObjectItem(r,"printer_setup");
 cJSON_ReplaceItemInObject(s,"api_key",cJSON_CreateString(""));assert(shu1_printer_setup_parse(r,&cfg,id));
 cJSON_AddBoolToObject(r,"work_on",true);assert(!shu1_printer_setup_parse(r,&cfg,id));cJSON_DeleteItemFromObject(r,"work_on");
 cJSON_ReplaceItemInObject(s,"api_key",cJSON_CreateString("a\r\nInjected: yes"));assert(!shu1_printer_setup_parse(r,&cfg,id));
 cJSON_ReplaceItemInObject(s,"api_key",cJSON_CreateString("abc"));
 cJSON_ReplaceItemInObject(s,"port",cJSON_CreateNumber(7125.5));assert(!shu1_printer_setup_parse(r,&cfg,id));
 cJSON_ReplaceItemInObject(s,"port",cJSON_CreateNumber(65536));assert(!shu1_printer_setup_parse(r,&cfg,id));
 cJSON_ReplaceItemInObject(s,"port",cJSON_CreateNumber(7125));
 cJSON_ReplaceItemInObject(s,"host",cJSON_CreateString("host/path?api_key=x"));assert(!shu1_printer_setup_parse(r,&cfg,id));
 cJSON_ReplaceItemInObject(s,"host",cJSON_CreateString("host"));
 cJSON_AddStringToObject(s,"host","other");assert(!shu1_printer_setup_parse(r,&cfg,id));
 cJSON_Delete(r);return 0;
}
''',json=True)

    def test_required_control_data(self):
        self.compile(audit.COMMON+r'''
#include "printer_setup_rules.h"
int main(void) {
 cJSON *r=cJSON_Parse("{\"print_stats\":{\"state\":\"standby\"},\"heater_bed\":{\"temperature\":25,\"target\":0},\"webhooks\":{\"state\":\"ready\"}}");
 assert(shu1_printer_control_subset(r));
 cJSON *bed=cJSON_GetObjectItem(r,"heater_bed");cJSON_DeleteItemFromObject(bed,"target");assert(!shu1_printer_control_subset(r));
 cJSON_AddNumberToObject(bed,"target",0);cJSON_ReplaceItemInObject(cJSON_GetObjectItem(r,"webhooks"),"state",cJSON_CreateString("shutdown"));
 assert(!shu1_printer_control_subset(r));cJSON_Delete(r);assert(!shu1_printer_control_subset(NULL));return 0;
}
''',json=True)

    def test_failed_tests_restore_old_config_and_success_saves_after_websocket(self):
        self.compile(audit.COMMON+r'''
#include "settings_store.h"
#include <stdatomic.h>
typedef struct {shu1_device_config_t cfg;char id[33];} setup_request_t;
static shu1_device_config_t g_devcfg;
static int scenario,stops,creates,writes,ended,inhibited;
static bool guard_held,g_subscribe_pending,g_key_set;
static int g_setup_mux,g_client;
static atomic_bool g_setup_ws_valid;
static int64_t clock_ms;
static char phase[24];
static const char *CONTROL_QUERY="query";
#define pdMS_TO_TICKS(x) (x)
static int64_t now_ms(void) {return clock_ms;}
static void vTaskDelay(int ms) {clock_ms+=ms;}
static const char *probe_printer(const shu1_device_config_t *cfg) {assert(!guard_held);return scenario==1 ? "auth_failed":NULL;}
static void setup_phase(const char *p) {strcpy(phase,p);}
static bool stop_client(void) {assert(!guard_held);stops++;atomic_store(&g_setup_ws_valid,false);return true;}
static bool create_client(void) {creates++;g_client=1;return true;}
static int esp_websocket_client_start(int c) {return ESP_OK;}
static bool esp_websocket_client_is_connected(int c) {return true;}
static void send_subscription(bool b) {}
static void send_json(const char *s) {if(scenario!=2)atomic_store(&g_setup_ws_valid,true);}
shu1_printer_state_t shu1_state_get_printer(void) {
 shu1_printer_state_t pr={0};pr.klippy_ready=true;pr.subscribed=true;pr.last_update_ms=clock_ms;return pr;
}
esp_err_t shu1_settings_store_save_moonraker(const shu1_device_config_t *cfg) {
 assert(!guard_held);writes++;
 if(writes==1) {assert(atomic_load(&g_setup_ws_valid));assert(!strcmp(cfg->moonraker_host,"candidate"));}
 else assert(!strcmp(cfg->moonraker_host,"old"));
 return scenario==3 && writes==1 ? ESP_ERR_INVALID_STATE:ESP_OK;
}
void shu1_safety_latch_inhibit(void) {inhibited++;}
shu1_control_guard_t shu1_control_guard_begin(void) {guard_held=true;return (shu1_control_guard_t){true};}
void shu1_control_guard_end(shu1_control_guard_t *g) {guard_held=false;}
void shu1_control_release_any(void) {}
void shu1_control_maintenance_end(void) {assert(guard_held);ended++;}
void shu1_event_log_add(const char *l,const char *c,const char *m) {assert(!strstr(m,"secret"));}
''' + audit.function("moonraker_client.c","static void apply_setup(")+r'''
int main(void) {
 for(scenario=0;scenario<4;scenario++) {
  stops=creates=writes=ended=inhibited=0;clock_ms=1000;
  strcpy(g_devcfg.moonraker_host,"old");strcpy(g_devcfg.moonraker_api_key,"old-secret");
  setup_request_t req={0};strcpy(req.cfg.moonraker_host,"candidate");strcpy(req.cfg.moonraker_api_key,"new-secret");
  apply_setup(&req);assert(ended==1 && !inhibited && !req.cfg.moonraker_api_key[0]);
  if(scenario==0)assert(writes==1 && stops==1 && !strcmp(g_devcfg.moonraker_host,"candidate") && !strcmp(phase,"succeeded"));
  else {assert(!strcmp(g_devcfg.moonraker_host,"old") && !strcmp(g_devcfg.moonraker_api_key,"old-secret"));
   if(scenario==1)assert(stops==0 && writes==0 && !strcmp(phase,"auth_failed"));
   if(scenario==2)assert(stops==2 && writes==0 && !strcmp(phase,"websocket_failed"));
   if(scenario==3)assert(stops==2 && writes==2 && !strcmp(phase,"storage_failed"));
  }
 }
 return 0;
}
''')

    def test_no_key_in_status_and_no_direct_legacy_config_writes(self):
        status=audit.function("moonraker_client.c","bool shu1_moonraker_setup_status(")
        self.assertNotIn("moonraker_api_key",status)
        self.assertIn('"key_set"',status)
        for name in ("api_server.c","ble_control.c"):
            text=audit.source(name)
            self.assertIn("shu1_moonraker_setup_request(root)",text)
            self.assertNotIn("cfg.moonraker_host",text)
        net=audit.source("moonraker_client.c")
        self.assertIn('.disable_auto_redirect = true',net)
        self.assertIn('esp_http_client_set_header(h,"X-Api-Key",device->moonraker_api_key)',net)
        self.assertIn('X-Api-Key: %s\\r\\n',net)
        self.assertIn('"mk_config"',audit.function("settings_store.c","esp_err_t shu1_settings_store_factory_reset("))

    def test_real_admission_blocks_jobs_pause_schedule_outputs_and_stale_revision(self):
        self.compile(audit.COMMON+r'''
#include "printer_setup_rules.h"
typedef struct {shu1_device_config_t cfg;char id[33];} setup_request_t;
static int g_setup_queue=1,g_setup_mux,sent,released;
static bool g_maintenance,outputs;
static char g_setup_id[33],g_setup_phase[24];
static shu1_settings_t st;
shu1_settings_t shu1_state_get_settings(void) {return st;}
bool shu1_control_outputs_busy(void) {return outputs;}
void shu1_control_snapshot(shu1_control_snapshot_t *out) {memset(out,0,sizeof(*out));out->revision=7;}
static void setup_phase(const char *p) {strcpy(g_setup_phase,p);}
void shu1_control_maintenance_end(void) {g_maintenance=false;}
void shu1_control_release_any(void) {released++;}
static int xQueueSend(int q,const setup_request_t *r,int timeout) {sent++;assert(g_maintenance);return pdTRUE;}
'''+audit.function("app_state.c","bool shu1_control_network_setup_begin(")+
audit.function("moonraker_client.c","esp_err_t shu1_moonraker_setup_request(")+r'''
int main(void) {
 cJSON *root=cJSON_Parse("{\"expected_revision\":7,\"printer_setup\":{\"host\":\"host\",\"port\":7125,\"api_key\":\"key\",\"id\":\"0123456789abcdef0123456789abcdef\"}}");
 for(int i=0;i<7;i++) {
  memset(&st,0,sizeof(st));outputs=false;g_maintenance=false;
  if(i==0)st.work_on=true;
  if(i==1)st.work_on=st.user_paused=true;
  if(i==2)st.scheduled_preheat_enabled=true;
  if(i==3)outputs=true;
  if(i==4)g_maintenance=true;
  if(i==5)st.drying_running=true;
  if(i==6)cJSON_ReplaceItemInObject(root,"expected_revision",cJSON_CreateNumber(6));
  assert(shu1_moonraker_setup_request(root)!=ESP_OK && sent==0 && released==0);
 }
 cJSON_ReplaceItemInObject(root,"expected_revision",cJSON_CreateNumber(7));
 assert(shu1_moonraker_setup_request(root)==ESP_OK && sent==1 && released==1 && g_maintenance);
 assert(shu1_moonraker_setup_request(root)!=ESP_OK && sent==1);
 cJSON_Delete(root);return 0;
}
''',json=True)

if __name__=="__main__": unittest.main()
