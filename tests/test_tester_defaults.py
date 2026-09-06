"""Configuration regression: functional outputs, not disabled interlocks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class TesterDefaultsTests(unittest.TestCase):
    def test_functional_outputs_and_safeguards(self):
        text = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")
        for key in ("SHU1_ENABLE_HEATER_OUTPUT", "SHU1_ENABLE_FAN_TRIAC_CONTROL",
                    "SHU1_ENABLE_PHYSICAL_CONTROLS", "ESP_TASK_WDT_EN",
                    "ESP_TASK_WDT_INIT", "ESP_TASK_WDT_PANIC",
                    "BOOTLOADER_APP_ROLLBACK_ENABLE"):
            self.assertIn(f"CONFIG_{key}=y", text)
        self.assertIn("CONFIG_SHU1_MAX_TARGET_TEMP_C=55", text)
        self.assertIn("CONFIG_SHU1_ENABLE_GPIO_PROBE=n", text)
