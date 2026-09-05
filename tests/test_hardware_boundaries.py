"""Regression checks for local drivers and their GPIO ownership boundaries."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class HardwareBoundaryTests(unittest.TestCase):
    def test_local_drivers_not_vendor_includes(self):
        for path in (ROOT / "main").glob("*.[ch]"):
            body = path.read_text(encoding="utf-8")
            with self.subTest(path=path.name):
                self.assertFalse("vendor/dragonbreath/" in body, path.name)
                self.assertFalse("pb_fan_set_level(" in body, path.name)
                self.assertFalse("pb_heater_pid_step(" in body, path.name)
        self.assertFalse(list((ROOT / "main/vendor/dragonbreath").glob("*.c")))
        self.assertFalse(list((ROOT / "main/vendor/dragonbreath").glob("*.h")))

    def test_no_other_fan_gpio_owner(self):
        for path in (ROOT / "main").glob("*.c"):
            if path.name == "fan_triac.c":
                continue
            body = path.read_text(encoding="utf-8")
            if "gpio_set_level(" in body or "gpio_config(" in body:
                self.assertFalse("CONFIG_SHU1_FAN_GPIO" in body, path.name)
                self.assertFalse("PB_GPIO_FAN_GATE" in body, path.name)
        owners = {path.name for path in (ROOT / "main").glob("*.c")
                  if "gpio_set_level(" in path.read_text(encoding="utf-8")}
        self.assertEqual(owners, {"fan_triac.c", "heater.c", "physical_controls.c"})

if __name__ == "__main__":
    unittest.main()
