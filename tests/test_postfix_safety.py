"""Production-function regressions for OFF, HTTP receive budgets and BLE frames."""
import re
import unittest
import test_audit_regressions as audit


class PostfixSafetyTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_off_cannot_be_revived_by_print_completion(self):
        helpers = '\n'.join(audit.function('safety.c', signature) for signature in (
            'static bool str_eq(', 'static bool printer_state_is_printing(',
            'static bool printer_state_is_complete_or_idle(', 'static bool printer_data_fresh(',
            'static void update_auto_context('))
        self.compile(audit.COMMON + r'''
static bool g_auto_print_context_seen;
static char g_last_auto_printer_state[24]="idle";
static int tempering_calls;
static void shu1_event_log_add(const char *a,const char *b,const char *c) {}
static void shu1_ble_notify_status_now(void) {}
static void start_tempering_if_user_enabled(shu1_settings_t *s,const shu1_runtime_t *r,int64_t n) {tempering_calls++;}
''' + helpers + audit.function('api_server.c', 'static void apply_safe_stop(') + r'''
int main(void) {
 shu1_runtime_t r={0};
 for(int finish=0;finish<=2;finish++) {
  shu1_settings_t s={.work_mode=SHU1_MODE_AUTO,.work_on=true,
   .finish_conditioning_mode=finish,.tempering_enabled=true,.keep_warm_max_min=60};
  shu1_printer_state_t p={.moonraker_connected=true,.last_update_ms=1000};
  strcpy(p.normalized_state,"printing");strcpy(p.print_state,"printing");
  update_auto_context(&s,&p,&r,1000);assert(g_auto_print_context_seen);
  apply_safe_stop(&s);tempering_calls=0;
  strcpy(p.normalized_state,"complete");strcpy(p.print_state,"complete");
  update_auto_context(&s,&p,&r,1000);
  assert(!s.work_on&&!s.keep_warm_active&&!g_auto_print_context_seen&&!tempering_calls);
  strcpy(p.normalized_state,"printing");strcpy(p.print_state,"printing");
  update_auto_context(&s,&p,&r,1000);assert(!s.work_on&&!g_auto_print_context_seen);
 }
 // A legitimate active Keep Warm job still continues, then expires without revival.
 shu1_settings_t s={.work_mode=SHU1_MODE_AUTO,.work_on=true,
  .finish_conditioning_mode=SHU1_FINISH_KEEP_WARM,.keep_warm_max_min=1};
 shu1_printer_state_t p={.moonraker_connected=true,.last_update_ms=1000};
 strcpy(p.normalized_state,"printing");update_auto_context(&s,&p,&r,1000);
 strcpy(p.normalized_state,"complete");update_auto_context(&s,&p,&r,1000);
 assert(s.work_on&&s.keep_warm_active);
 update_auto_context(&s,&p,&r,61000);assert(!s.work_on&&!s.keep_warm_active);
 strcpy(p.normalized_state,"printing");p.last_update_ms=62000;
 update_auto_context(&s,&p,&r,62000);assert(!s.work_on&&!g_auto_print_context_seen);
 return 0;
}
''')
        code = audit.source('safety.c')
        self.assertIn('auto_context_epoch != command_epoch', code)
        self.assertNotIn('s->work_on = true;', audit.function('safety.c', 'static void update_auto_context('))

    def test_ble_full_frame_before_admission(self):
        body = audit.function('ble_control.c', 'static int handle_control_write(')
        parser = next(line for line in body.splitlines() if 'cJSON *root =' in line)
        self.assertLess(body.index('memchr('), body.index('SHU1_CONTROL_GUARD'))
        self.compile(audit.COMMON + '#include "json_guard.h"\n' +
            'static cJSON *parse(char *buf,int len) {\n' + parser + '\nreturn root;}\n' + r'''
int main(void) {
 char hidden[]="{\"work_on\":true}\0{\"safe_stop\":true}";
 assert(!parse(hidden,sizeof(hidden)-1));
 char leading[]="\0{}";assert(!parse(leading,sizeof(leading)-1));
 char trailing[]="{}\0";assert(!parse(trailing,sizeof(trailing)-1));
 char valid[]="{\"text\":\"escaped \\u0000 data\"}";
 cJSON *r=parse(valid,sizeof(valid)-1);assert(r);cJSON_Delete(r);
 char plain[]="{}";r=parse(plain,2);assert(r);cJSON_Delete(r);
 return 0;
}
''', json=True)

    def test_http_slow_headers_and_all_routes_close_without_drain(self):
        self.compile(audit.COMMON + r'''
#define ESP_FAIL -1
#define HTTPD_SOCK_ERR_FAIL -2
#define HTTP_GET 0
typedef void *httpd_handle_t;
typedef struct {int64_t deadline_us;} api_receive_budget_t;
typedef struct {void *user_ctx;httpd_handle_t handle;int content_len;} httpd_req_t;
typedef struct {const char *uri;int method;esp_err_t (*handler)(httpd_req_t *);} httpd_uri_t;
static api_receive_budget_t budget;
static int64_t now,delay;
static int received,handled;
static int64_t esp_timer_get_time(void) {return now;}
static void *httpd_sess_get_transport_ctx(httpd_handle_t h,int fd) {return &budget;}
static int httpd_req_to_sockfd(httpd_req_t *r) {return 1;}
#define recv test_socket_recv
static int recv(int fd,char *p,size_t len,int flags) {now+=delay;received++;return 1;}
static int httpd_resp_set_hdr(httpd_req_t *r,const char *a,const char *b) {return 0;}
static int httpd_resp_set_status(httpd_req_t *r,const char *s) {return 0;}
static int httpd_resp_sendstr(httpd_req_t *r,const char *s) {return 0;}
static esp_err_t handler(httpd_req_t *r) {handled++;return ESP_OK;}
''' + audit.function('api_server.c', 'static int api_bounded_recv(')
            + audit.function('api_server.c', 'static esp_err_t api_guarded_handler(') + r'''
int main(void) {
 budget.deadline_us=2000000;delay=500000;char b;
 for(int i=0;i<3;i++) assert(api_bounded_recv(NULL,1,&b,1,0)==1);
 assert(api_bounded_recv(NULL,1,&b,1,0)==HTTPD_SOCK_ERR_FAIL);
 assert(now==2000000&&received==4);
 assert(api_bounded_recv(NULL,1,&b,1,0)==HTTPD_SOCK_ERR_FAIL&&received==4);
 const char *paths[]={"/api/health","/api/history","/api/status","/api/events","/api/v2/ota","/api/v2/auth","/api/v2/boot-inactive"};
 for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++) {
  httpd_uri_t route={paths[i],i==6?1:HTTP_GET,handler};
  httpd_req_t req={&route,NULL,1000};now=0;budget.deadline_us=2000000;handled=0;
  assert(api_guarded_handler(&req)==ESP_FAIL&&!handled&&received==4);
  req.content_len=0;assert(api_guarded_handler(&req)==ESP_FAIL&&handled==1);
 }
 httpd_uri_t route={"/api/settings",1,handler};httpd_req_t req={&route,NULL,10};
 now=0;budget.deadline_us=2000000;handled=0;
 assert(api_guarded_handler(&req)==ESP_FAIL&&handled==1&&budget.deadline_us==2000000);
 route.uri="/api/v2/update";assert(api_guarded_handler(&req)==ESP_FAIL&&budget.deadline_us==60000000);
 now=60000000;assert(api_guarded_handler(&req)==ESP_FAIL&&handled==2);
 return 0;
}
''')
        code = audit.source('api_server.c')
        self.assertIn('config.open_fn = api_connection_open', code)
        self.assertIn('guarded.handler = api_guarded_handler', code)
        self.assertNotRegex(code, r'(?m)^    httpd_uri_t \w+ =')  # user_ctx targets static storage
        self.assertEqual(len(re.findall(r'static const httpd_uri_t \w+ =', code)), 15)
