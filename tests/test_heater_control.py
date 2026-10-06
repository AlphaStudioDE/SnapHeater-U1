import unittest
from pathlib import Path
import test_panda_hardware as harness


class HeaterControlTests(unittest.TestCase):
    def test_production_hysteresis(self):
        program = (Path(__file__).parent / "heater_control_host_test.c").read_text(encoding="utf-8")
        harness.PandaHardwareTests.compile(self, harness.BASE, program, run=True)

    def test_control_wiring(self):
        safety = (harness.ROOT / "main/safety.c").read_text(encoding="utf-8")
        self.assertIn("if (target > SHU1_VALIDATION_MAX_TARGET_C) target = SHU1_VALIDATION_MAX_TARGET_C;", safety)
        self.assertIn("rt.chamber_instant_temp_c, inhibited, &duty", safety)
        self.assertIn("request_heat = rt.heater_commanded_duty > 0.0f", safety)
        self.assertNotIn("shu1_pid_", safety)
        self.assertIn("!shu1_fan_triac_is_running()", safety)


if __name__ == "__main__":
    unittest.main()
