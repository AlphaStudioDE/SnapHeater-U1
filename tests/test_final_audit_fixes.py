import unittest
import test_audit_regressions as audit

class FinalAuditFixTests(unittest.TestCase):
    compile=audit.AuditRegressionTests.compile

    def test_mixed_stop_and_duplicate_fields(self):
        self.compile(audit.COMMON+r'''
#include "command_validation.h"
int main(void) {
 const char *cases[]={
 "{\"safe_stop\":true,\"heartbeat\":\"old\"}",
 "{\"unlock\":\"123456\",\"safe_stop\":true}",
 "{\"rest_token\":\"1234567890123456\",\"safe_stop\":true}",
 "{\"factory_reset\":\"factory-reset\",\"work_on\":false}",
 "{\"work_on\":true,\"work_on\":false}",
 "{\"safe_stop\":false,\"safe_stop\":true,\"expected_revision\":0}"};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  cJSON *r=cJSON_Parse(cases[i]);assert(shu1_stop_requested(r));cJSON_Delete(r);
 }
 cJSON *r=cJSON_Parse("{\"work_on\":true}");assert(!shu1_stop_requested(r));cJSON_Delete(r);
 return 0;
}
''',json=True)
        ble=audit.source("ble_control.c")
        body=ble[ble.index("static int handle_control_write("):]
        self.assertLess(body.index("if (shu1_stop_requested(root))"),body.index("if (rest_token)"))
        self.assertLess(body.index("if (shu1_stop_requested(root))"),body.index('cJSON *heartbeat'))

    def test_thermal_limits_cannot_be_weakened(self):
        self.compile(audit.COMMON+r'''
#include "thermal_limits.h"
int main(void) {
 for(int offset=-5;offset<=5;offset++) {
  assert(shu1_safety_temperature(105+offset,offset)>=105);
  assert(shu1_safety_temperature(85+offset,offset)>=85);
 }
 assert(isnan(shu1_safety_temperature(NAN,0)));
 assert(shu1_foldback_limit(33,104)==99);
 assert(shu1_foldback_limit(82,104)==102);
 assert(shu1_foldback_limit(33,90)==90);
 assert(shu1_foldback_limit(82,0)==102);
 return 0;
}
''')

    def test_boot_off_before_logging(self):
        body=audit.function("app_main.c","void app_main(")
        self.assertLess(body.index("shu1_heater_preinit_off"),body.index("ESP_LOG"))
