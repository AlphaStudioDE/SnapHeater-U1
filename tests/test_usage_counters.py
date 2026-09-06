"""Compile actual usage-counter policy; no actuators or NVS writes."""
import unittest
import test_audit_regressions as audit

class UsageCounterTests(unittest.TestCase):
    compile=audit.AuditRegressionTests.compile

    def test_temperature_boundary_output_state_and_sensor_validity(self):
        self.compile(audit.COMMON + r'''
#include "usage_counters.h"
int main(void) {
    shu1_runtime_t rt={0};
    rt.heater_output_on=rt.fan_output_on=true;
    rt.chamber_sensor_status=SHU1_SENSOR_OK;rt.last_sensor_ms=1000;
    rt.chamber_instant_temp_c=34.99f;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==0 && rt.fan_on_accum_ms==500);
    rt.chamber_instant_temp_c=35.0f;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==500);
    rt.chamber_instant_temp_c=55.0f;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==1000);
    rt.heater_output_on=false;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==1000 && rt.fan_on_accum_ms==2000);
    rt.heater_output_on=true;rt.chamber_instant_temp_c=NAN;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==1000);
    rt.chamber_instant_temp_c=55;rt.chamber_sensor_status=SHU1_SENSOR_INVALID;
    shu1_usage_accumulate(&rt,500,1000);assert(rt.heater_usage_35c_ms==1000);
    rt.chamber_sensor_status=SHU1_SENSOR_OK;
    shu1_usage_accumulate(&rt,500,2501);assert(rt.heater_usage_35c_ms==1000);
    shu1_usage_accumulate(&rt,500,999);assert(rt.heater_usage_35c_ms==1000);
    rt.fan_output_on=false;
    uint64_t fan=rt.fan_on_accum_ms;
    shu1_usage_accumulate(&rt,0,1000);shu1_usage_accumulate(&rt,-1,1000);shu1_usage_accumulate(&rt,6000,1000);
    assert(rt.heater_usage_35c_ms==1000 && rt.fan_on_accum_ms==fan);
    assert(rt.heater_on_accum_ms==0 && rt.session_heater_on_ms==0); // Energy counters are separate.
    return 0;
}
''')

    def test_persistent_usage_uses_filtered_counter_without_resetting_saved_base(self):
        source=audit.source("recorder.c")
        self.assertIn("heater_total=heater_base+rt->heater_usage_35c_ms",source)
        self.assertIn("fan_total=fan_base+rt->fan_on_accum_ms",source)
        self.assertIn("heater_base=totals[0]; fan_base=totals[1]",source)
        energy=audit.function("safety.c","static void update_energy_and_stability(")
        self.assertIn("shu1_usage_accumulate",energy)
        self.assertIn("rt->heater_on_accum_ms +=",energy)

if __name__=="__main__": unittest.main()
