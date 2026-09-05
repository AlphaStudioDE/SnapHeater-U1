"""Static integration regressions; complementary to the executable host tests, not HIL."""
import pathlib
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

class SafetyIntegrationTest(unittest.TestCase):
    def test_no_production_demo(self):
        paths = list((ROOT / "main").glob("*.[ch]"))
        paths += list((ROOT / "apps/android/SnapHeaterU1/app/src/main").rglob("*.kt"))
        for path in paths:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("demo_mode_enabled", text, str(path))
            self.assertNotIn("demoModeEnabled", text, str(path))
            self.assertNotIn("AppSession.Demo", text, str(path))

    def test_command_authorization_precedes_calibration(self):
        for file in ("main/api_server.c", "main/ble_control.c"):
            text = source(file)
            self.assertIn("SHU1_CONTROL_GUARD(policy_guard)", text)
            self.assertLess(text.index("shu1_control_authorize("), text.index("shu1_ntc_set_offset_c("))
            self.assertIn("calibration && !stopping", text)
            self.assertNotIn("shu1_heater_force_off();", text)

    def test_tick_guard_covers_snapshot_and_actuation(self):
        text = source("main/safety.c")
        tick = text[text.index("static void control_task"):text.index("esp_err_t shu1_safety_start")]
        self.assertLess(tick.index("SHU1_CONTROL_GUARD"), tick.index("shu1_state_get_settings"))
        self.assertLess(tick.index("shu1_state_commit_control_if_epoch"), tick.index("shu1_heater_set"))
        self.assertLess(tick.index("shu1_heater_set"), tick.index("shu1_control_guard_end"))
        self.assertIn("shu1_safety_airflow(request_fan, heat_mode, faulted)", tick)
        self.assertIn("shu1_safety_heat_allowed", tick)
        self.assertNotIn("shu1_heater_set(false, false)", tick)

    def test_ota_reservation_spans_upload_and_boot_selection(self):
        text = source("main/api_server.c")
        for name in ("ota_update_post_handler", "boot_inactive_post_handler"):
            body = text.split("static esp_err_t " + name, 1)[1].split("\nstatic ", 1)[0]
            self.assertIn("cleanup(maintenance_release)", body)
            self.assertIn("maintenance_acquire()", body)
            self.assertIn("maintenance.keep = true", body)
            self.assertLess(body.index("maintenance_acquire"), body.index("esp_ota_set_boot_partition"))
        self.assertIn("upload_deadline", text)
        self.assertIn("shu1_control_maintenance_active()", text)

    def test_pending_schedule_has_lease_and_dry_has_deadline(self):
        safety = source("main/safety.c")
        self.assertIn("!shu1_control_schedule_allowed()", safety)
        self.assertIn("(st.work_on || st.scheduled_preheat_enabled)", safety)
        for file in ("main/api_server.c", "main/ble_control.c"):
            self.assertIn("!st.work_on && !st.scheduled_preheat_enabled", source(file))
        panel = source("main/physical_controls.c")
        dry = panel.split("static void start_dry_mode", 1)[1].split("\nstatic ", 1)[0]
        self.assertIn("st.drying_end_ms =", dry)
        self.assertIn("hours > 12", dry)
        self.assertIn("!shu1_control_start_allowed()", panel)

    def test_no_probe_second_writer_and_independent_ntc_reads(self):
        heater = source("main/heater.c")
        probe = heater.split("esp_err_t shu1_heater_probe_pulse", 1)[1]
        self.assertNotIn("shu1_fan_triac_set", probe)
        ntc = source("main/ntc.c").split("esp_err_t shu1_ntc_read", 1)[1].split("int shu1_ntc_rref", 1)[0]
        self.assertIn("read_channel(0", ntc)
        self.assertIn("read_channel(1", ntc)
        self.assertNotIn("ESP_RETURN_ON_ERROR(read_channel", ntc)

if __name__ == "__main__":
    unittest.main()
