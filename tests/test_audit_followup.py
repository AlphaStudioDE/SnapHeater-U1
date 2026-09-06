"""September 6 fixes: execute production code with bounded fault injection."""
import re
import unittest
import test_audit_regressions as audit


class AuditFollowupTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_json_depth_and_complete_object(self):
        self.compile(audit.COMMON + r'''
#include "json_guard.h"
int main(void) {
 assert(!shu1_json_parse(NULL));
 assert(!shu1_json_parse("[]"));assert(!shu1_json_parse("true"));
 assert(!shu1_json_parse("{} garbage"));assert(!shu1_json_parse("{}{}"));
 assert(!shu1_json_parse("{\"a\": [}"));
 cJSON *p=shu1_json_parse("{\"text\":\"[{}]\\\"\\\\\"}");assert(p);cJSON_Delete(p);
 char b[5000];
 for(int depth=1;depth<=1000;depth++) {
  int n=0;b[n++]='{';b[n++]='"';b[n++]='a';b[n++]='"';b[n++]=':';
  for(int i=1;i<depth;i++)b[n++]='[';
  b[n++]='0';for(int i=1;i<depth;i++)b[n++]=']';b[n++]='}';b[n]=0;
  p=shu1_json_parse(b);assert((p!=NULL)==(depth<=16));cJSON_Delete(p);
 }
 return 0;
}
''', json=True)
        for path in ("api_server.c", "ble_control.c", "moonraker_client.c"):
            self.assertNotIn("cJSON_Parse(", audit.source(path))
        self.assertIn("CJSON_NESTING_LIMIT=16", (audit.ROOT / "CMakeLists.txt").read_text())

    def test_slow_rest_has_total_deadline_and_closes(self):
        self.compile(audit.COMMON + r'''
#include "json_guard.h"
#define ESP_ERR_INVALID_SIZE 201
#define ESP_FAIL -1
typedef struct {int content_len;} httpd_req_t;
static int64_t now, delay_us;
static int calls;
static int64_t esp_timer_get_time(void) {return now;}
static int httpd_req_recv(httpd_req_t *r,char *p,int n) {now+=delay_us;*p=' ';calls++;return 1;}
''' + audit.function("api_server.c", "static esp_err_t read_json_body(") + r'''
int main(void) {
 httpd_req_t r={2048};cJSON *out=(void*)1;
 delay_us=400000;assert(read_json_body(&r,&out)!=ESP_OK);assert(!out&&calls==5);
 now=calls=0;delay_us=1000000;assert(read_json_body(&r,&out)!=ESP_OK);assert(!out&&calls==2);
 return 0;
}
''', json=True)
        code = audit.source("api_server.c")
        errors = re.findall(r'if \(read_json_body\(req, &root\) != ESP_OK\) \{([^}]+)', code)
        # JSON reply contains braces, so inspect each full handler instead.
        for name in ("settings", "heartbeat", "probe", "token"):
            body = audit.function("api_server.c", f"static esp_err_t {name}_post_handler(")
            self.assertIn("return ESP_FAIL;", body[body.index("if (read_json_body"):][:300])
        self.assertEqual(len(errors), 4)
        self.assertIn("config.recv_wait_timeout=1", code.replace(" ", ""))
        self.assertNotIn("if (reject_unauthorized(req)) return ESP_OK", code)
        ota = audit.function("api_server.c", "static esp_err_t ota_update_post_handler(")
        self.assertNotIn("return ESP_OK;", ota)  # No automatic body drain on early upload rejection.

    def test_ota_marker_survives_restart_and_storage_errors_fail_closed(self):
        self.compile(audit.COMMON + r'''
typedef int nvs_handle_t;
typedef struct {const char *label;} esp_partition_t;
#define NVS_READWRITE 1
#define NVS_READONLY 0
#define ESP_ERR_NVS_NOT_FOUND 201
static int open_error, read_error, write_error, commit_error;
static uint8_t persisted;
static int nvs_open(const char *n,int mode,int *h) {assert(!strcmp(n,"shu1_ota"));return open_error;}
static int nvs_set_u8(int h,const char *key,uint8_t v) {if(!write_error)persisted=v;return write_error;}
static int nvs_get_u8(int h,const char *key,uint8_t *v) {*v=persisted;return read_error;}
static int nvs_commit(int h) {return commit_error;}
static void nvs_close(int h) {}
''' + audit.function("ota_storage.c", "esp_err_t shu1_ota_slot_pending(")
            + audit.function("ota_storage.c", "bool shu1_ota_slot_boot_allowed(") + r'''
int main(void) {
 esp_partition_t slot={"ota_1"};
 assert(shu1_ota_slot_pending(&slot,true)==0);assert(!shu1_ota_slot_boot_allowed(&slot));
 // No RAM state: a fresh read, as after reboot, must still reject the slot.
 assert(!shu1_ota_slot_boot_allowed(&slot));
 assert(shu1_ota_slot_pending(&slot,false)==0);assert(shu1_ota_slot_boot_allowed(&slot));
 read_error=77;assert(!shu1_ota_slot_boot_allowed(&slot));
 read_error=ESP_ERR_NVS_NOT_FOUND;assert(shu1_ota_slot_boot_allowed(&slot));
 open_error=77;assert(!shu1_ota_slot_boot_allowed(&slot));assert(shu1_ota_slot_pending(&slot,true)==77);
 open_error=0;write_error=77;assert(shu1_ota_slot_pending(&slot,true)==77);
 write_error=0;commit_error=77;assert(shu1_ota_slot_pending(&slot,true)==77);
 assert(!shu1_ota_slot_boot_allowed(NULL));return 0;
}
''')
        boot = audit.function("api_server.c", "static esp_err_t boot_inactive_post_handler(")
        self.assertLess(boot.index("shu1_ota_slot_boot_allowed"), boot.index("esp_ota_set_boot_partition"))
        reset = audit.function("settings_store.c", "esp_err_t shu1_settings_store_factory_reset(")
        self.assertNotIn("shu1_ota", reset)

    def test_panel_replaces_old_workflow_and_drying_respects_limit(self):
        self.compile(audit.COMMON + r'''
static shu1_settings_t state;
shu1_settings_t shu1_state_get_settings(void) {return state;}
void shu1_state_update_settings_command(const shu1_settings_t *s) {state=*s;}
static bool output_latch_allows_start(void) {return true;}
shu1_control_result_t shu1_control_claim(shu1_control_source_t owner,bool takeover,uint32_t revision,char *lease) {return SHU1_CONTROL_OK;}
static int64_t esp_timer_get_time(void) {return 1000000;}
static void shu1_safety_wake(void) {}
static void set_notification(int n,const char *c,const char *m) {}
static void shu1_event_log_add(const char *a,const char *b,const char *c) {}
''' + audit.function("app_state.c", "void shu1_settings_stop(")
            + "\n".join(audit.function("physical_controls.c", f"static void start_{mode}_mode(") for mode in ("auto", "dry", "manual")) + r'''
int main(void) {
 for(int mode=0;mode<3;mode++) {
  state=(shu1_settings_t){.work_on=true,.user_paused=true,.tempering_phase=SHU1_TEMPERING_ACTIVE,
   .tempering_current_target_c=37,.drying_running=true,.preheat_running=true,.dryout_running=true,
   .health_test_running=true,.session_started_ms=1,.manual_session_max_min=90,.target_temp_c=50};
  if(mode==0)start_auto_mode();else if(mode==1)start_dry_mode();else start_manual_mode();
  assert(state.work_on&&!state.user_paused&&state.tempering_phase==SHU1_TEMPERING_IDLE);
  assert(!state.preheat_running&&!state.dryout_running&&!state.health_test_running);
  assert(state.target_temp_c==50&&state.session_started_ms==1000);
  assert(state.drying_running==(mode==1));
  if(mode==1)assert(state.drying_end_ms==1000+90*60000);
 }
 return 0;
}
''')

    def test_reconnect_continues_without_changing_network(self):
        self.compile(audit.COMMON + r'''
#include <setjmp.h>
#include <stdatomic.h>
#define WIFI_IF_STA 0
#define pdMS_TO_TICKS(x) (x)
typedef struct {struct {char ssid[33];} sta;} wifi_config_t;
static atomic_bool ready=true,busy=false,testing=false,connected=false;
static int ticks, attempts;
static jmp_buf finished;
static void vTaskDelay(int ms) {assert(ms==10000);if(++ticks==51)longjmp(finished,1);}
static int esp_wifi_get_config(int i,wifi_config_t *cfg) {strcpy(cfg->sta.ssid,"saved-network");return 0;}
static int esp_wifi_connect(void) {attempts++;return -1;}
''' + audit.function("wifi_sta.c", "static void reconnect_worker(") + r'''
int main(void) {
 if(!setjmp(finished))reconnect_worker(NULL);assert(attempts==50);
 ticks=attempts=0;busy=true;if(!setjmp(finished))reconnect_worker(NULL);assert(!attempts);
 ticks=0;busy=false;connected=true;if(!setjmp(finished))reconnect_worker(NULL);assert(!attempts);
 return 0;
}
''')

    def test_drying_command_clamps_to_panda_session(self):
        self.compile(audit.COMMON + r'''
#include "job_commands.h"
int main(void) {
 cJSON *p=cJSON_Parse("{\"work_on\":true,\"work_mode\":2,\"drying_duration_min\":1440}");
 for(int limit=1;limit<=720;limit++) {
  shu1_settings_t s={.work_mode=SHU1_MODE_DRYING,.drying_running=true,.manual_session_max_min=limit};
  shu1_finish_job_command(&s,p,1000);assert(s.drying_end_ms==1000+(int64_t)limit*60000);
 }
 cJSON_Delete(p);return 0;
}
''', json=True)

    def test_wifi_record_is_coherent_and_no_implicit_printer(self):
        self.compile(audit.COMMON + r'''
#include "settings_store.h"
typedef int nvs_handle_t;
typedef struct {uint32_t version;char ssid[33],password[65];} wifi_record_t;
typedef struct {uint32_t version;char host[64];uint32_t port;char key[129];} moonraker_record_t;
#define NVS_READONLY 0
#define ESP_ERR_NVS_NOT_FOUND 201
static const char *NS="app_nvs";
static wifi_record_t record;
static int mode, writes;
static int nvs_open(const char *n,int mode,int *h) {return 0;}
static int open_rw(int *h) {return 0;}
static void nvs_close(int h) {}
static int nvs_commit(int h) {return 0;}
static int nvs_get_u16(int h,const char *key,uint16_t *v) {return ESP_ERR_NVS_NOT_FOUND;}
static int nvs_get_str(int h,const char *key,char *v,size_t *n) {
 if(!strcmp(key,"ssid"))strcpy(v,"legacy-ssid");
 else if(!strcmp(key,"password"))strcpy(v,"legacy-password");
 else return ESP_ERR_NVS_NOT_FOUND;
 return 0;
}
static int nvs_get_blob(int h,const char *key,void *v,size_t *n) {
 if(strcmp(key,"wifi_config")||mode==0)return ESP_ERR_NVS_NOT_FOUND;
 if(mode==2)return 77;
 memcpy(v,&record,sizeof(record));*n=sizeof(record);return 0;
}
static int nvs_set_blob(int h,const char *key,const void *v,size_t n) {
 assert(!strcmp(key,"wifi_config")&&n==sizeof(record));writes++;memcpy(&record,v,n);return 0;
}
static void shu1_event_log_add(const char *a,const char *b,const char *c) {}
''' + audit.function("settings_store.c", "void shu1_device_config_defaults(")
            + audit.function("settings_store.c", "esp_err_t shu1_settings_store_load_device_config(")
            + audit.function("settings_store.c", "esp_err_t shu1_settings_store_save_device_config(") + r'''
int main(void) {
 shu1_device_config_t c;
 shu1_device_config_defaults(&c);assert(!c.moonraker_host[0]);
 assert(!shu1_settings_store_load_device_config(&c));assert(!strcmp(c.wifi_ssid,"legacy-ssid"));
 assert(!strcmp(c.wifi_password,"legacy-password")&&!c.moonraker_host[0]);
 strcpy(c.wifi_ssid,"new-ssid");strcpy(c.wifi_password,"new-password");
 assert(!shu1_settings_store_save_device_config(&c)&&writes==1);
 mode=1;memset(&c,0,sizeof(c));assert(!shu1_settings_store_load_device_config(&c));
 assert(!strcmp(c.wifi_ssid,"new-ssid")&&!strcmp(c.wifi_password,"new-password"));
 mode=2;assert(shu1_settings_store_load_device_config(&c)!=0);assert(!c.wifi_ssid[0]&&!c.moonraker_host[0]);
 mode=1;record.version=99;assert(shu1_settings_store_load_device_config(&c)!=0);
 return 0;
}
''')


if __name__ == "__main__":
    unittest.main()
