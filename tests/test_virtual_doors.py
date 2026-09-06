"""Production detector: advisory in all modes; receipts cannot clear newer events."""
import unittest
import test_audit_regressions as audit
from test_audit_regressions import COMMON, function


class VirtualDoorTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_all_modes_and_retained_event(self):
        self.compile(COMMON + r'''
static void shu1_event_log_add(const char *a, const char *b, const char *c) {}
static void shu1_ble_notify_status_now(void) {}
''' + function("safety.c", "static void reset_virtual_door_window(")
        + function("safety.c", "static bool update_virtual_door_detection(") + r'''
int main(void) {
 for(int mode=1; mode<=6; ++mode) for(int on=0; on<=1; ++on) {
  shu1_settings_t s={0};
  shu1_runtime_t rt={0};
  shu1_printer_state_t p={0};
  s.work_mode=mode; s.work_on=on; s.tempering_phase=1; s.keep_warm_active=true;
  s.virtual_door_detection_enabled=true;
  s.virtual_door_window_sec=60; s.virtual_door_drop_c=4;
  s.virtual_door_rate_c_per_min=4; s.virtual_door_min_base_temp_c=35;
  s.virtual_door_action=SHU1_VDOOR_ACTION_STOP_HEATER;
  rt.chamber_sensor_status=SHU1_SENSOR_OK; rt.chamber_temp_c=50;
  assert(!update_virtual_door_detection(&s,&rt,&p,1000));
  rt.chamber_temp_c=45;
  assert(!update_virtual_door_detection(&s,&rt,&p,2000));
  assert(update_virtual_door_detection(&s,&rt,&p,61000));
  assert(s.work_on==on && s.work_mode==mode && s.tempering_phase==1 && s.keep_warm_active);
  assert(!s.door_open && !s.door_open_pending);
  assert(s.virtual_door_open_pending && s.virtual_door_detected_ms==61000);
  assert(s.virtual_door_last_drop_c==5);
  assert(!update_virtual_door_detection(&s,&rt,&p,121000));
  assert(s.virtual_door_open_pending && s.virtual_door_last_drop_c==5);
  s.virtual_door_detection_enabled=false;
  assert(!update_virtual_door_detection(&s,&rt,&p,181000));
  assert(s.virtual_door_open_pending);
  s.virtual_door_detection_enabled=true; rt.chamber_temp_c=NAN;
  assert(!update_virtual_door_detection(&s,&rt,&p,241000));
  rt.chamber_temp_c=30;
  assert(!update_virtual_door_detection(&s,&rt,&p,301000));
  rt.chamber_temp_c=25;
  assert(!update_virtual_door_detection(&s,&rt,&p,361000));
 }
 return 0;
}
''')

    def test_receipt_is_timestamp_scoped_and_preserves_job(self):
        self.compile(COMMON + r'''
static shu1_settings_t stored;
shu1_settings_t shu1_state_get_settings(void) { return stored; }
void shu1_state_update_settings_command(const shu1_settings_t *s) { stored=*s; }
''' + function("app_state.c", "void shu1_virtual_door_ack(") + r'''
int main(void) {
 stored.work_on=true; stored.virtual_door_open_pending=true;
 stored.virtual_door_detected_ms=200;
 shu1_virtual_door_ack(100);
 assert(stored.virtual_door_open_pending && stored.work_on);
 shu1_virtual_door_ack(200);
 assert(!stored.virtual_door_open_pending && stored.work_on);
 return 0;
}
''')
