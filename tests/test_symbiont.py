"""Production curve and serialized Moonraker supervision, no device I/O."""
import unittest
import test_audit_regressions as audit

class SymbiontTests(unittest.TestCase):
    compile=audit.AuditRegressionTests.compile

    def test_curve_hysteresis_ramp_and_invalid_samples(self):
        self.compile(audit.COMMON+r'''
#include "symbiont_rules.h"
int main(void) {
 bool on=false;
 assert(shu1_symbiont_curve(&on,50,45)==0 && !on);
 assert(shu1_symbiont_curve(&on,50.04f,45)==51 && on);
 assert(shu1_symbiont_curve(&on,51,45)==75);
 assert(shu1_symbiont_curve(&on,52,45)==100);
 assert(shu1_symbiont_curve(&on,60,45)==100);
 assert(shu1_symbiont_curve(&on,50,45)==50);
 assert(shu1_symbiont_curve(&on,49,45)==40);
 assert(shu1_symbiont_curve(&on,48,45)==30);
 assert(shu1_symbiont_curve(&on,47.1f,45)==30);
 assert(shu1_symbiont_curve(&on,47,45)==0 && !on);
 assert(shu1_symbiont_curve(&on,49,45)==0);
 assert(shu1_symbiont_curve(&on,49,42)==100); // declining tempering target
 assert(shu1_symbiont_curve(&on,NAN,42)==0 && !on);
 assert(shu1_symbiont_curve(&on,49,0)==0);
 return 0;
}
''')

    def test_feedback_conflicts_top_alarm_auto_reconnect_and_stale_reply(self):
        self.compile(audit.COMMON+r'''
#include "symbiont_engine.h"
static void reply(shu1_symbiont_engine_t *s,int id,const char *result,int64_t t) {
 char text[1400];snprintf(text,sizeof(text),"{\"id\":%d,\"result\":%s}",id,result);
 cJSON *r=cJSON_Parse(text);assert(r);shu1_symbiont_response(s,r,t);cJSON_Delete(r);
}
#define DATA "{\"status\":{\"webhooks\":{\"state\":\"ready\"},\"fan_generic cavity_fan\":{\"speed\":0},\"purifier\":{\"power_detected\":true,\"critical_temp_reported\":false,\"fan_fault_reported\":false,\"exhaust_fan\":{\"speed\":0}}}}"
int main(void) {
 shu1_symbiont_engine_t s={0};char msg[512];
 assert(!shu1_symbiont_next(&s,false,true,true,true,100,1,msg,sizeof(msg)));
 assert(shu1_symbiont_next(&s,true,true,true,true,100,10,msg,sizeof(msg)));
 assert(strstr(msg,"printer.objects.query"));int old=s.pending;
 reply(&s,old,DATA,20);assert(s.phase==1);
 assert(shu1_symbiont_next(&s,true,true,true,true,100,21,msg,sizeof(msg)));
 assert(strstr(msg,"printer.control.generic_fan") && strstr(msg,"\"speed\":100"));
 reply(&s,s.pending,"{\"state\":\"success\"}",30);
 assert(shu1_symbiont_next(&s,true,true,true,true,100,31,msg,sizeof(msg)));
 reply(&s,s.pending,DATA,40);assert(s.phase==2); // no top starvation if slicer keeps overriding AUX
 assert(shu1_symbiont_next(&s,true,true,true,true,100,41,msg,sizeof(msg)));
 assert(strstr(msg,"printer.control.purifier") && strstr(msg,"\"fan\":\"exhaust\""));
 assert(!strstr(msg,"inner") && !strstr(msg,"MODE") && !strstr(msg,"DELAY_OFF"));
 old=s.pending;
 assert(!shu1_symbiont_next(&s,false,true,true,true,100,42,msg,sizeof(msg)));
 reply(&s,old,"{\"state\":\"success\"}",43);assert(!s.active && !s.pending);
 assert(shu1_symbiont_next(&s,true,true,true,true,100,44,msg,sizeof(msg)));
 reply(&s,s.pending,"{\"status\":{\"webhooks\":{\"state\":\"ready\"},\"fan_generic cavity_fan\":{\"speed\":0},\"purifier\":{\"power_detected\":true,\"critical_temp_reported\":true,\"fan_fault_reported\":false}}}",45);
 assert(s.phase==0 && !strcmp(s.status,"top_cover_fault"));
 assert(!shu1_symbiont_next(&s,true,false,true,true,100,50,msg,sizeof(msg)));
 assert(shu1_symbiont_next(&s,true,true,true,true,100,51,msg,sizeof(msg)));
 assert(strstr(msg,"printer.objects.query"));
 assert(!shu1_symbiont_next(&s,true,true,false,true,100,52,msg,sizeof(msg)));
 assert(!strcmp(s.status,"unsupported_fan"));
 return 0;
}
''',json=True)

    def test_absent_top_timeout_partial_notifications_and_bad_data(self):
        self.compile(audit.COMMON+r'''
#include "symbiont_engine.h"
int main(void) {
 shu1_symbiont_engine_t s={0};char msg[512],buf[1000];
 assert(shu1_symbiont_next(&s,true,true,true,true,0,10,msg,sizeof(msg)));
 snprintf(buf,sizeof(buf),"{\"id\":%d,\"result\":{\"status\":{\"webhooks\":{\"state\":\"ready\"},\"fan_generic cavity_fan\":{\"speed\":0},\"purifier\":{\"power_detected\":false,\"critical_temp_reported\":false,\"fan_fault_reported\":false}}}}",s.pending);
 cJSON *r=cJSON_Parse(buf);shu1_symbiont_response(&s,r,20);cJSON_Delete(r);
 assert(!s.top && !strcmp(s.status,"verified"));
 assert(!shu1_symbiont_next(&s,true,true,true,true,0,100,msg,sizeof(msg)));
 r=cJSON_Parse("{\"fan_generic cavity_fan\":{\"speed\":1}}");
 shu1_symbiont_notify(&s,r,110);cJSON_Delete(r);
 assert(shu1_symbiont_next(&s,true,true,true,true,0,110,msg,sizeof(msg)));
 assert(strstr(msg,"printer.objects.query"));
 assert(!shu1_symbiont_next(&s,true,true,true,true,0,2110,msg,sizeof(msg)));
 assert(!strcmp(s.status,"timeout"));
 assert(shu1_symbiont_next(&s,true,true,true,true,0,3110,msg,sizeof(msg)));
 snprintf(buf,sizeof(buf),"{\"id\":%d,\"result\":{\"status\":{}}}",s.pending);
 r=cJSON_Parse(buf);shu1_symbiont_response(&s,r,3120);cJSON_Delete(r);
 assert(!strcmp(s.status,"invalid_data") && s.phase==0);
 return 0;
}
''',json=True)
