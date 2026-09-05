import unittest
from pathlib import Path
import test_panda_hardware as harness


class HeaterPidTests(unittest.TestCase):
    def test_production_pid(self):
        program = (Path(__file__).parent / "heater_pid_host_test.c").read_text(encoding="utf-8")
        harness.PandaHardwareTests.compile(self, harness.BASE, program, run=True)

    def test_control_wiring(self):
        safety = (harness.ROOT / "main/safety.c").read_text(encoding="utf-8")
        self.assertIn("if (target > SHU1_VALIDATION_MAX_TARGET_C) target = SHU1_VALIDATION_MAX_TARGET_C;", safety)
        self.assertIn("rt.chamber_instant_temp_c, !inhibited, esp_timer_get_time(), &duty", safety)
        self.assertIn("&g_heater_pid, rt.heater_commanded_duty, esp_timer_get_time()", safety)
        self.assertIn("!shu1_fan_triac_is_running()", safety)


if __name__ == "__main__":
    unittest.main()
