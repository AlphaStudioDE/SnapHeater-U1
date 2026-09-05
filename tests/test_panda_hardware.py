"""Host tests of production fan_triac.c and stock-board compile guards.
No hardware access. Run with CC pointing to a native C compiler.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("CC", "clang")
BASE = {
    "FAN_GPIO": 3, "HEATER_GPIO": 18, "ZERO_CROSS_GPIO": 7,
    "FAN_ACTIVE_HIGH": 1, "HEATER_ACTIVE_HIGH": 1,
    "ENABLE_FAN_TRIAC_CONTROL": 1, "ENABLE_HEATER_OUTPUT": 1,
    "RREF_STRAP_GPIO": 19, "CHAMBER_ADC_CH": 0, "PTC_ADC_CH": 1,
    "STATUS_LED_GPIO": -1, "BUTTON_GPIO": -1,
    "BUTTON_POWER_GPIO": 9, "BUTTON_AUTO_GPIO": 8, "BUTTON_ON_GPIO": 10,
    "BUTTON_DRY_GPIO": 2, "LED_AUTO_GPIO": 6, "LED_ON_GPIO": 5,
    "LED_DRY_GPIO": 4, "LED_POWER_GPIO": 21,
    "ENABLE_PHYSICAL_CONTROLS": 1, "BUTTON_ACTIVE_LOW": 1, "LED_ACTIVE_HIGH": 1,
}
STUBS = {
    "hal/adc_types.h": "#pragma once\n",
    "esp_attr.h": "#define IRAM_ATTR\n",
    "esp_err.h": "typedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_ERR_INVALID_ARG 1\n#define ESP_ERR_INVALID_STATE 2\n",
    "esp_check.h": '#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { int e = (expr); if(e) return e; } while(0)\n',
    "esp_log.h": "#define ESP_LOGE(tag, ...) ((void)(tag))\n#define ESP_LOGW(tag, ...) ((void)(tag))\n#define ESP_LOGI(tag, ...) ((void)(tag))\n",
    "esp_timer.h": "#include <stdint.h>\nstatic int64_t test_now;\nstatic inline int64_t esp_timer_get_time(void) { return test_now; }\n",
    "freertos/FreeRTOS.h": "typedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n",
    "driver/gpio.h": """
#include <stdint.h>
typedef int gpio_num_t;
#define GPIO_NUM_3 3
#define GPIO_NUM_7 7
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 2
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_POSEDGE 1
static int gate_level, high_writes, gpio_fail;
static int config_calls, fail_config_at, isr_add_calls, isr_error;
static inline int gpio_set_level(int pin, int level) {
    (void)pin; gate_level = level; high_writes += level != 0; return 0;
}
static inline int gpio_config(const gpio_config_t *c) {
    (void)c; ++config_calls;
    return gpio_fail || (fail_config_at && config_calls == fail_config_at);
}
static inline int gpio_install_isr_service(int flags) { (void)flags; return 0; }
static inline int gpio_isr_handler_add(int pin, void (*fn)(void *), void *arg) {
    (void)pin; (void)fn; (void)arg; ++isr_add_calls; return gpio_fail || isr_error;
}
"""
}
PROGRAM = r"""
#include <assert.h>
#include <stddef.h>
#include "fan_triac.c"
static void edge(int64_t now) { test_now = now; zero_cross_isr(NULL); }
int main(void) {
    assert(shu1_fan_triac_init() == ESP_OK);
    assert(gate_level == 0 && high_writes == 0);
    shu1_fan_triac_set(true);
    assert(gate_level == 0); /* ON waits for ZC */
    edge(10000);
    assert(gate_level == 1 && high_writes == 1);
    edge(11000); edge(10000); edge(9000); /* short, equal, backward */
    assert(shu1_fan_triac_zero_cross_stats().edge_count == 1 && shu1_fan_triac_zero_cross_stats().rejected_edge_count == 3);
    assert(shu1_fan_triac_zero_cross_stats().last_edge_ms == 10 && high_writes == 1);
    edge(13999); assert(shu1_fan_triac_zero_cross_stats().rejected_edge_count == 4);
    edge(14000); assert(shu1_fan_triac_zero_cross_stats().edge_count == 2 && shu1_fan_triac_zero_cross_stats().last_period_us == 4000);
    edge(22333); assert(shu1_fan_triac_zero_cross_stats().last_period_us == 8333); /* 60 Hz */
    edge(32333); assert(shu1_fan_triac_zero_cross_stats().last_period_us == 10000); /* 50 Hz */
    assert(high_writes == 1); /* held, never repulsed */
    shu1_fan_triac_set(false);
    assert(gate_level == 0 && !shu1_fan_triac_is_active());
    edge(42333); assert(gate_level == 0);
    shu1_fan_triac_set(true); shu1_fan_triac_set(false);
    edge(52333); assert(gate_level == 0); /* pending ON cancelled */
    edge(52333LL + UINT32_MAX + 1LL);
    assert(shu1_fan_triac_zero_cross_stats().last_period_us == UINT32_MAX); /* no wrapping */
    shu1_fan_triac_set(true);
    edge(test_now + 10000);
    assert(shu1_fan_triac_is_running());
    test_now += 100001;
    assert(!shu1_fan_triac_is_running());
    shu1_fan_triac_force_off(); assert(gate_level == 0); /* no ZC needed */
    assert(shu1_fan_triac_init() == ESP_OK); /* idempotent, no live ISR reset */
    return 0;
}
"""

class PandaHardwareTests(unittest.TestCase):
    def compile(self, config, program, run=False):
        with tempfile.TemporaryDirectory(prefix="shu1-fan-test-") as temp:
            d = Path(temp)
            for name, body in STUBS.items():
                dest = d / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_text("#pragma once\n" + body, encoding="utf-8")
            (d / "sdkconfig.h").write_text("".join(
                f"#define CONFIG_SHU1_{key} {value}\n" for key, value in config.items()
            ), encoding="utf-8")
            c = d / "test.c"
            c.write_text(program, encoding="utf-8")
            exe = d / ("test.exe" if os.name == "nt" else "test")
            result = subprocess.run([CC, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                                     "-I" + str(d), "-I" + str(ROOT / "main"),
                                     str(c), "-o", str(exe)], capture_output=True, text=True)
            if run:
                self.assertEqual(result.returncode, 0, result.stderr)
                subprocess.run([str(exe)], check=True)
            return result

    def test_production_fan(self):
        self.compile(BASE, PROGRAM, run=True)

    def test_init_failures_never_admit_on(self):
        for stage in (1, 2):
            with self.subTest(config_failure=stage):
                program = ('#include <assert.h>\n#include <stddef.h>\n#include "fan_triac.c"\n'
                           'int main(void) { fail_config_at = ' + str(stage) + ';'
                           'assert(shu1_fan_triac_init() != ESP_OK);'
                           'shu1_fan_triac_set(true); assert(!shu1_fan_triac_is_active());'
                           'assert(!gate_level && !high_writes && !isr_add_calls); return 0; }')
                self.compile(BASE, program, run=True)
        self.compile(BASE, '#include <assert.h>\n#include <stddef.h>\n#include "fan_triac.c"\n'
                     'int main(void) { isr_error = 1; assert(shu1_fan_triac_init() != ESP_OK);'
                     'shu1_fan_triac_set(true); assert(!gate_level && !high_writes); return 0; }',
                     run=True)

    def test_disabled_backend_has_no_gpio_access(self):
        config = dict(BASE, FAN_GPIO=-1, HEATER_GPIO=-1, ZERO_CROSS_GPIO=-1,
                      ENABLE_FAN_TRIAC_CONTROL=0, ENABLE_HEATER_OUTPUT=0)
        self.compile(config, '#include <assert.h>\n#include <stddef.h>\n#include "fan_triac.c"\n'
                     'int main(void) { assert(shu1_fan_triac_init() == ESP_OK);'
                     'shu1_fan_triac_set(true); shu1_fan_triac_force_off();'
                     'assert(!shu1_fan_triac_is_active() && !shu1_fan_triac_is_running());'
                     'assert(!config_calls && !isr_add_calls && !high_writes); return 0; }',
                     run=True)

    def test_console_and_injection_conflicts_rejected(self):
        cases = [
            "#define CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG 1\n",
            "#define CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG 1\n",
            "#define CONFIG_ESP_CONSOLE_UART_CUSTOM 1\n"
            "#define CONFIG_ESP_CONSOLE_UART_TX_GPIO 3\n"
            "#define CONFIG_ESP_CONSOLE_UART_RX_GPIO 20\n",
            "#define CONFIG_ESP_CONSOLE_UART 1\n#define CONFIG_SHU1_ENABLE_POWER_LED 1\n",
            "#define CONFIG_PB_DEVBOARD_SAFE 1\n",
        ]
        for definitions in cases:
            with self.subTest(definitions=definitions):
                result = self.compile(BASE, definitions +
                    '#include "app_config.h"\nint main(void) {return 0;}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Panda hardware:", result.stderr)

    def test_stock_and_disabled_maps_compile(self):
        self.compile(BASE, '#include "app_config.h"\nint main(void) {return 0;}',
                     run=True)
        disabled = dict(BASE, FAN_GPIO=-1, HEATER_GPIO=-1, ZERO_CROSS_GPIO=-1,
                        ENABLE_FAN_TRIAC_CONTROL=0, ENABLE_HEATER_OUTPUT=0)
        self.compile(disabled, '#include "app_config.h"\nint main(void) {return 0;}',
                     run=True)

    def test_unsafe_configs_do_not_compile(self):
        cases = [(key, value) for key, value in [
            ("FAN_GPIO", 18), ("HEATER_GPIO", 3), ("ZERO_CROSS_GPIO", 3),
            ("FAN_ACTIVE_HIGH", 0), ("HEATER_ACTIVE_HIGH", 0),
            ("FAN_GPIO", -1), ("ZERO_CROSS_GPIO", -1),
            ("ENABLE_FAN_TRIAC_CONTROL", 0), ("HEATER_GPIO", -1),
            ("RREF_STRAP_GPIO", 3), ("CHAMBER_ADC_CH", 3), ("PTC_ADC_CH", 0),
            ("BUTTON_ACTIVE_LOW", 0), ("LED_ACTIVE_HIGH", 0)]]
        for key in ("STATUS_LED_GPIO", "BUTTON_GPIO", "LED_AUTO_GPIO", "LED_ON_GPIO",
                    "LED_DRY_GPIO", "LED_POWER_GPIO", "BUTTON_POWER_GPIO",
                    "BUTTON_AUTO_GPIO", "BUTTON_ON_GPIO", "BUTTON_DRY_GPIO"):
            cases.extend((key, pin) for pin in (3, 18, 7, 19, 0, 1))
        for key, value in cases:
            with self.subTest(option=key, value=value):
                result = self.compile(dict(BASE, **{key: value}),
                                      '#include "app_config.h"\nint main(void) {return 0;}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Panda hardware:", result.stderr)

if __name__ == "__main__":
    unittest.main()
