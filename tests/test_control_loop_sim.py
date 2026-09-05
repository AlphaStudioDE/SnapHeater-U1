"""Run the complete production safety loop and actuator logic with offline I/O."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCENARIOS=("chamber_open", "ptc_short", "nan_with_ok_status", "adc_read_error",
           "instant_ptc_overheat", "instant_chamber_overheat", "zero_cross_loss",
           "foldback_33k", "foldback_82k", "frozen_cold_sensors", "ineffective_cooling",
           "overheat_and_nvs_failure", "off", "watchdog_registration_failure",
           "hot_reboot_cooldown", "ota_maintenance", "stalled_task_and_reboot_observation",
           "frozen_near_target_observation", "rest_heartbeat_loss", "ble_heartbeat_loss",
           "ota_rejected_while_heating", "zero_cross_return_stays_disarmed",
           "zero_cross_clear_and_explicit_rearm", "zero_cross_nvs_failure",
           "startup_without_zero_cross", "zero_cross_loss_after_off",
           "zero_cross_loss_while_ssr_off", "zero_cross_gap_between_ticks",
           "zero_cross_stale_clear_cannot_clear_new_fault")

class ControlLoopSimulationTests(unittest.TestCase):
    def test_production_control_loop_scenarios(self):
        idf=Path(os.environ["IDF_PATH"])
        with tempfile.TemporaryDirectory(prefix="shu1-loop-sim-") as temp:
            exe=Path(temp)/"control_loop.exe"
            cmd=[os.environ.get("CC","clang"),"-std=c11","-D_CRT_SECURE_NO_WARNINGS",
                 "-DESP_ERROR_CHECK(x)=assert((x)==0)"]
            for p in [ROOT/"tests/loop_stubs",ROOT/"tests/safety_stubs",ROOT/"tests/stubs",
                      ROOT/"main",ROOT/"build-heater-compile-test/config",idf/"components/json/cJSON"]:
                cmd.append("-I"+str(p))
            for p in ["tests/control_loop_sim.c","main/app_state.c","main/control_lease.c",
                      "main/safety_latch.c","main/heater.c","main/fan_triac.c","main/dc_pid.c","main/profiles.c"]:
                cmd.append(str(ROOT/p))
            result=subprocess.run(cmd+["-o",str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            for index,name in enumerate(SCENARIOS):
                with self.subTest(scenario=name):
                    result=subprocess.run([str(exe),str(index)],capture_output=True,text=True,timeout=15)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    print(name+": "+result.stdout.strip())

if __name__=="__main__": unittest.main()
