"""Offline checks of production recorder/event serialization. No hardware writes."""
import unittest
import test_audit_regressions as audit
from test_audit_regressions import COMMON, function, source


class RecordingTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_event_queue_is_bounded_and_does_not_export_messages(self):
        text=source("event_log.c")
        body=text[text.index("#define SHU1_EVENT_LOG_CAP"):]
        self.compile(COMMON + r'''
#include "cJSON.h"
static int64_t esp_timer_get_time(void) {return 123456;}
static uint32_t esp_random(void) {return 123;}
void shu1_device_id(char *out,size_t size) {snprintf(out,size,"AABBCCDDEEFF");}
void shu1_event_log_add(const char*,const char*,const char*);
''' + body + r'''
int main(void) {
 shu1_event_log_init();
 for(int i=0;i<40;i++) shu1_event_log_add("critical","1234567890123456789012345678901","SECRET_NOT_FOR_NOTIFICATION");
 cJSON *page=shu1_event_notifications(); assert(page);
 cJSON *rows=cJSON_GetObjectItem(page,"events");
 assert(cJSON_GetArraySize(rows)==32);
 assert(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"seq")->valueint==10);
 assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"level")->valuestring,"critical"));
 assert(cJSON_GetObjectItem(page,"now_ms")->valueint==123);
 assert(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"ms")->valueint==123);
 char buffer[4096]; assert(cJSON_PrintPreallocated(page,buffer,sizeof(buffer),false));
 assert(!strstr(buffer,"SECRET"));
 assert(strstr(buffer,"AABBCCDDEEFF"));
 cJSON_Delete(page);
 return 0;
}
''', json=True)

    def test_recorder_overwrite_and_budget(self):
        text=source("recorder.c")
        structs=text[text.index("#define SAMPLE_INTERVAL_S"):text.index("static int16_t temperature")]
        self.compile(COMMON + r'''
#include "cJSON.h"
#include <limits.h>
static int64_t esp_timer_get_time(void) {return 8000000000LL;}
void shu1_device_id(char *out,size_t size) {snprintf(out,size,"AABBCCDDEEFF");}
''' + structs + function("recorder.c","cJSON *shu1_recorder_page(") + r'''
int main(void) {
 assert(SAMPLE_INTERVAL_S==10 && CAP==720 && sizeof(records)==11520);
 lock=xSemaphoreCreateMutex(); strcpy(boot,"0123456789abcdef");
 assert(shu1_recorder_page(0)==NULL);
 recorder_ready=true;
 for(uint32_t i=1;i<=800;i++) records[(i-1)%CAP]=(record_t){.seq=i,.seconds=i*10,.chamber=350,.ptc=400,.target=450};
 sequence=800;
 cJSON *page=shu1_recorder_page(1);
 assert(cJSON_IsTrue(cJSON_GetObjectItem(page,"gap")));
 cJSON *rows=cJSON_GetObjectItem(page,"samples");
 assert(cJSON_GetArraySize(rows)==32);
 assert(cJSON_GetArrayItem(cJSON_GetArrayItem(rows,0),0)->valueint==81);
 assert(cJSON_GetObjectItem(page,"next")->valueint==112);
 char buf[4096];assert(cJSON_PrintPreallocated(page,buf,sizeof(buf),false));
 cJSON_Delete(page);
 page=shu1_recorder_page(800);
 assert(cJSON_GetArraySize(cJSON_GetObjectItem(page,"samples"))==0);
 cJSON_Delete(page);
 page=shu1_recorder_page(0);
 assert(cJSON_IsTrue(cJSON_GetObjectItem(page,"gap"))); // First connection already lost early samples.
 cJSON_Delete(page);
 page=shu1_recorder_page(UINT32_MAX);
 assert(cJSON_GetObjectItem(page,"next")->valueint==112); // No uint32 cursor overflow.
 cJSON_Delete(page);
 return 0;
}
''', json=True)

    def test_flash_checkpoints_are_guarded_and_skip_unchanged_totals(self):
        text=source("recorder.c")
        begin=function("recorder.c","static bool checkpoint_begin(")
        end=function("recorder.c","static void checkpoint_end(")
        self.assertIn("SHU1_CONTROL_GUARD",begin)
        self.assertIn("shu1_control_checkpoint_begin()",begin)
        self.assertIn("SHU1_CONTROL_GUARD",end)
        self.assertIn("shu1_control_maintenance_end()",end)
        self.assertLess(text.index("checkpoint_begin())"),text.index("nvs_set_blob"))
        self.assertLess(text.index("nvs_commit"),text.index("checkpoint_end();"))
        self.assertIn("totals[0]!=saved_heater || totals[1]!=saved_fan",text)
        self.assertIn("seconds-persisted>=60",text)

    def test_rest_history_cursor_validation_is_read_only(self):
        self.compile(COMMON + r'''
#include "cJSON.h"
typedef struct {const char *query;int status;} httpd_req_t;
static unsigned calls;
static uint32_t last_cursor;
static void add_common_headers(httpd_req_t *r) {}
static int httpd_req_get_url_query_len(httpd_req_t *r) {return (int)strlen(r->query);}
static int httpd_req_get_url_query_str(httpd_req_t *r,char *out,size_t n) {
 if(strlen(r->query)>=n) return -1;strcpy(out,r->query);return ESP_OK;
}
static int httpd_query_key_value(char *q,const char *key,char *out,size_t n) {
 if(strncmp(q,"after=",6) || strlen(q+6)>=n) return -1;strcpy(out,q+6);return ESP_OK;
}
static cJSON *shu1_recorder_page(uint32_t after) {calls++;last_cursor=after;return cJSON_CreateObject();}
static void httpd_resp_set_status(httpd_req_t *r,const char *s) {r->status=atoi(s);}
static void httpd_resp_sendstr(httpd_req_t *r,const char *s) {}
''' + function("api_server.c","static esp_err_t history_get_handler(") + r'''
int main(void) {
 const char *valid[]={"","after=0","after=4294967295"};
 for(unsigned i=0;i<3;i++) {httpd_req_t r={valid[i],200};history_get_handler(&r);assert(r.status==200);}
 assert(calls==3 && last_cursor==UINT32_MAX);
 const char *bad[]={"after=-1","after=1.5","after=4294967296","after=","after=99999999999999999999","x=1"};
 for(unsigned i=0;i<6;i++) {httpd_req_t r={bad[i],200};history_get_handler(&r);assert(r.status==400);}
 assert(calls==3);
 return 0;
}
''',json=True)
        handler=function("api_server.c","static esp_err_t history_get_handler(")
        self.assertNotIn("shu1_control_",handler)
        self.assertNotIn("shu1_state_update",handler)
        gatt=function("ble_control.c","static int gatt_access_cb(")
        self.assertIn("memcpy(payload,g_history_payload",gatt)
        self.assertIn("OS_MBUF_PKTLEN(ctxt->om)!=sizeof(bytes)",gatt)
