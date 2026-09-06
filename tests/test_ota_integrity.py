"""Execute the production upload handler with injected transport/hash/flash results."""
import unittest
import test_audit_regressions as audit


class OtaIntegrityTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_upload_commit_gate(self):
        body = audit.function("api_server.c", "static esp_err_t ota_update_post_handler(")
        self.compile(audit.COMMON + r'''
#include "ota_integrity.h"
#include "cJSON.h"
#define ESP_OK 0
#define ESP_FAIL -1
#define SHU1_OTA_BUFFER_SIZE 4096
#define OTA_WITH_SEQUENTIAL_WRITES 0
#define HTTPD_SOCK_ERR_TIMEOUT -3
typedef int esp_err_t;
typedef int esp_ota_handle_t;
typedef struct { int content_len; } httpd_req_t;
typedef struct { bool acquired, keep; } maintenance_scope_t;
typedef struct { size_t size, erase_size; const char *label; } esp_partition_t;
typedef struct { char project_name[32], version[32]; } esp_app_desc_t;
typedef int mbedtls_sha256_context;
static const esp_partition_t slot={8192,4096,"ota_1"};
static bool slot_pending, marker_failure;
static int shu1_ota_slot_pending(const esp_partition_t *p,bool pending) {if(marker_failure)return -1;slot_pending=pending;return 0;}
static bool shu1_ota_slot_boot_allowed(const esp_partition_t *p) {return !slot_pending;}
static char header[80];
static int begins, ends, boots, restarts, aborts, erases, releases;
static bool mismatch, truncated, bad_image, write_failure, hash_failure, erase_failure, kept;
static const char *reply;
static void add_common_headers(httpd_req_t *r) {}
static bool reject_unauthorized(httpd_req_t *r) {return false;}
static size_t httpd_req_get_hdr_value_len(httpd_req_t *r,const char *n) {return strlen(header);}
static int httpd_req_get_hdr_value_str(httpd_req_t *r,const char *n,char *out,size_t len) {snprintf(out,len,"%s",header);return 0;}
static int httpd_resp_set_status(httpd_req_t *r,const char *s) {return 0;}
static int httpd_resp_sendstr(httpd_req_t *r,const char *s) {reply=s;return 0;}
static maintenance_scope_t maintenance_acquire(void) {return (maintenance_scope_t){true,false};}
static void maintenance_release(maintenance_scope_t *s) {releases++;kept=s->keep;}
shu1_settings_t shu1_state_get_settings(void) {shu1_settings_t s={0};s.ota_enabled=true;return s;}
static const esp_partition_t *esp_ota_get_next_update_partition(void *p) {return &slot;}
static int esp_ota_begin(const esp_partition_t *p,int size,esp_ota_handle_t *h) {begins++;*h=1;return 0;}
static int esp_ota_abort(esp_ota_handle_t h) {aborts++;return 0;}
static int esp_ota_write(esp_ota_handle_t h,void *b,size_t n) {return write_failure ? -1:0;}
static int esp_ota_end(esp_ota_handle_t h) {ends++;return bad_image ? -1:0;}
static int esp_ota_get_partition_description(const esp_partition_t *p,esp_app_desc_t *d) {return 0;}
static bool accepted_image_identity(const esp_app_desc_t *d) {return true;}
static int esp_partition_erase_range(const esp_partition_t *p,int off,size_t len) {erases++;return erase_failure ? -1:0;}
static int esp_ota_set_boot_partition(const esp_partition_t *p) {boots++;return 0;}
static int64_t esp_timer_get_time(void) {return 1000;}
static int httpd_req_recv(httpd_req_t *r,char *b,int n) {memset(b,0,n);return truncated ? 0:n;}
static void mbedtls_sha256_init(mbedtls_sha256_context *s) {}
static void mbedtls_sha256_free(mbedtls_sha256_context *s) {}
static int mbedtls_sha256_starts(mbedtls_sha256_context *s,int n) {return 0;}
static int mbedtls_sha256_update(mbedtls_sha256_context *s,void *b,size_t n) {return 0;}
static int mbedtls_sha256_finish(mbedtls_sha256_context *s,uint8_t *out) {memset(out,mismatch?0xbb:0xaa,32);return hash_failure ? -1:0;}
static void shu1_event_log_add(const char *a,const char *b,const char *c) {}
static bool schedule_restart(void) {restarts++;return true;}
''' + body + r'''
static void reset(void) {
 memset(header,'a',64);header[64]=0;
 begins=ends=boots=restarts=aborts=erases=releases=0;reply="";
 mismatch=truncated=bad_image=write_failure=hash_failure=erase_failure=kept=false;
 slot_pending=marker_failure=false;
}
int main(void) {
 uint8_t out[32];
 assert(!shu1_ota_parse_sha256(NULL,out));
 httpd_req_t req={1024};
 reset();header[0]=0;ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();header[63]=0;ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();header[64]='a';header[65]=0;ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();header[2]='g';ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();header[2]=' ';ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();mismatch=true;ota_update_post_handler(&req);
 assert(begins==1&&aborts==1&&erases==1&&!ends&&!boots&&!restarts&&releases==1&&!kept);
 assert(strstr(reply,"sha256_mismatch"));
 assert(slot_pending);
 reset();mismatch=erase_failure=true;ota_update_post_handler(&req);assert(kept&&!boots&&!restarts);
 reset();truncated=true;ota_update_post_handler(&req);assert(aborts==1&&!ends&&!boots&&!restarts&&slot_pending);
 reset();write_failure=true;ota_update_post_handler(&req);assert(aborts==1&&!boots&&!restarts);
 reset();hash_failure=true;ota_update_post_handler(&req);assert(aborts==1&&!boots&&!restarts);
 reset();bad_image=true;ota_update_post_handler(&req);assert(ends==1&&!boots&&!restarts&&slot_pending);
 reset();marker_failure=true;ota_update_post_handler(&req);assert(!begins&&!boots);
 reset();ota_update_post_handler(&req);assert(ends==1&&boots==1&&restarts==1&&kept&&!aborts);
 assert(!slot_pending);
 reset();memset(header,'A',64);ota_update_post_handler(&req);assert(boots==1&&restarts==1);
 return 0;
}
''', json=True)

    def test_android_supplies_hash_before_upload(self):
        source = (audit.ROOT / "apps/android/SnapHeaterU1/app/src/main/java/com/alphastudio/snapheateru1/data/SnapHeaterApiClient.kt").read_text(encoding="utf-8")
        self.assertLess(source.index('digest(image)'), source.index('connection.outputStream'))
        self.assertLess(source.index('setRequestProperty("X-SnapHeater-SHA256",hash)'), source.index('connection.outputStream'))
