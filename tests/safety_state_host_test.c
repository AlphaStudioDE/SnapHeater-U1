// Windows host tests: real production state, lease and latch implementations.
// SRW mutexes are real locks, not the old always-success semaphore stubs.
#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include "app_state.h"
#include "control_lease.h"
#include "safety_latch.h"
#include "safety_rules.h"
#include "nvs.h"

int64_t test_now_us = 1000000;
static bool ssr_on;
static bool persist_fail;
static int persist_attempts;
static HANDLE attempted, completed;

void shu1_heater_cut_power(void) { ssr_on = false; }
bool shu1_fan_triac_is_active(void) { return false; }
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out) {
    (void)ns; *out = 1;
    if (mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    assert(!ssr_on); // Safety trip must drop GPIO BEFORE opening NVS.
    return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) {
    (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) {
    (void)h; (void)k; (void)v; assert(!ssr_on); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    (void)h; assert(!ssr_on); ++persist_attempts;
    return persist_fail ? ESP_ERR_INVALID_STATE : ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; }

static DWORD WINAPI off_thread(void *arg) {
    (void)arg;
    SetEvent(attempted);
    SHU1_CONTROL_GUARD(guard);
    shu1_settings_t st = shu1_state_get_settings();
    shu1_settings_stop(&st);
    shu1_control_release_any();
    shu1_state_update_settings_command(&st);
    SetEvent(completed);
    return 0;
}

int main(void) {
    for (int mask = 0; mask < 16; ++mask) {
        bool request = (mask & 1) != 0, maintenance = (mask & 2) != 0;
        bool watchdog = (mask & 4) != 0, fault = (mask & 8) != 0;
        bool heat = shu1_safety_heat_allowed(request, maintenance, watchdog, fault);
        assert(heat == (request && !maintenance && watchdog && !fault));
        assert(shu1_safety_airflow(false, heat, fault) == (heat || fault));
    }
    assert(shu1_safety_airflow(false, true, false)); // SSR off-window still has heat-mode airflow.
    assert(shu1_safety_airflow(false, false, true)); // restored cold fault, no purge history
    shu1_state_init();
    shu1_settings_t limited = shu1_state_get_settings();
    limited.target_temp_c = limited.preheat_target_temp_c = 60;
    limited.scheduled_preheat_target_c = limited.custom_temp_c = 90;
    shu1_state_update_settings(&limited);
    limited = shu1_state_get_settings();
    assert(limited.target_temp_c == 55 && limited.preheat_target_temp_c == 55);
    assert(limited.scheduled_preheat_target_c == 55 && limited.custom_temp_c == 55);
    assert(shu1_control_lease_init() == ESP_OK);
    assert(shu1_safety_latch_init() == ESP_OK);

    // OFF cannot interleave with snapshot/commit/apply. Once it completes,
    // the previous snapshot can no longer commit with its old epoch.
    shu1_control_guard_t guard = shu1_control_guard_begin();
    shu1_settings_t st = shu1_state_get_settings();
    st.work_on = true; st.output_safety_latch_armed = true;
    shu1_state_update_settings_command(&st);
    uint32_t epoch = shu1_state_command_epoch();
    shu1_runtime_t rt = shu1_state_get_runtime();
    attempted = CreateEvent(NULL, TRUE, FALSE, NULL);
    completed = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE worker = CreateThread(NULL, 0, off_thread, NULL, 0, NULL);
    assert(worker && WaitForSingleObject(attempted, 2000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(completed, 40) == WAIT_TIMEOUT);
    assert(shu1_state_commit_control_if_epoch(&st, &rt, epoch));
    shu1_control_guard_end(&guard);
    assert(WaitForSingleObject(worker, 2000) == WAIT_OBJECT_0);
    assert(!shu1_state_get_settings().work_on);
    assert(!shu1_state_commit_control_if_epoch(&st, &rt, epoch));
    CloseHandle(worker); CloseHandle(attempted); CloseHandle(completed);

    // Pending schedules and actual fan cooldown prevent maintenance.
    guard = shu1_control_guard_begin();
    st = shu1_state_get_settings();
    st.scheduled_preheat_enabled = true;
    shu1_state_update_settings_command(&st);
    assert(!shu1_control_maintenance_begin());
    shu1_settings_stop(&st);
    shu1_state_update_settings_command(&st);
    rt.fan_output_on = true; shu1_state_update_runtime(&rt);
    assert(!shu1_control_maintenance_begin());
    rt.fan_output_on = false; shu1_state_update_runtime(&rt);
    assert(!shu1_control_maintenance_begin()); // unknown/stale temperature is not cold
    rt.last_sensor_ms = test_now_us / 1000;
    rt.chamber_sensor_status = rt.ptc_sensor_status = SHU1_SENSOR_OK;
    rt.chamber_instant_temp_c = 25.0f; rt.ptc_instant_temp_c = 60.0f;
    shu1_state_update_runtime(&rt);
    assert(!shu1_control_maintenance_begin()); // hot, despite outputs OFF
    rt.ptc_instant_temp_c = 25.0f;
    shu1_state_update_runtime(&rt);
    assert(shu1_control_maintenance_begin());
    assert(shu1_control_maintenance_active());
    assert(!shu1_control_maintenance_begin()); // no overlapping operation
    shu1_control_maintenance_end();
    assert(!shu1_control_maintenance_active());
    shu1_control_guard_end(&guard);

    guard = shu1_control_guard_begin();
    assert(!shu1_control_schedule_allowed()); // no owner
    char lease[SHU1_LEASE_ID_LEN + 1];
    assert(shu1_control_claim(SHU1_CONTROL_BLE, true, SHU1_CONTROL_REVISION_ANY, lease) == SHU1_CONTROL_OK);
    assert(shu1_control_schedule_allowed());
    test_now_us += (int64_t)SHU1_LEASE_TIMEOUT_MS * 1000 + 1;
    assert(!shu1_control_schedule_allowed()); // no unattended delayed start
    shu1_control_release_any();
    assert(!shu1_control_maintenance_begin()); // lease time advance made sample stale
    rt.last_sensor_ms = test_now_us / 1000;
    shu1_state_update_runtime(&rt);
    assert(shu1_control_maintenance_begin());
    assert(!shu1_control_start_allowed());
    assert(!shu1_control_schedule_allowed());
    shu1_control_maintenance_end();
    assert(shu1_control_start_allowed());
    shu1_control_guard_end(&guard);

    // Trip must cut before NVS even if persistence fails. Failed clear stays latched.
    ssr_on = true; persist_fail = true;
    assert(shu1_safety_latch_trip(SHU1_HEATER_OVERTEMP) != ESP_OK);
    assert(!ssr_on && shu1_safety_latch_is_set() && persist_attempts == 1);
    assert(shu1_safety_latch_clear() != ESP_OK);
    assert(shu1_safety_latch_is_set());
    persist_fail = false;
    assert(shu1_safety_latch_clear() == ESP_OK);
    assert(!shu1_safety_latch_is_set());

    // Panic is clearable, watchdog inhibit is not.
    shu1_safety_latch_trip_volatile(SHU1_HEATER_PANIC_OFF);
    assert(shu1_safety_latch_clear() == ESP_OK);
    shu1_safety_latch_inhibit();
    int before = persist_attempts;
    shu1_safety_latch_request_clear();
    assert(shu1_safety_latch_clear() == ESP_ERR_INVALID_STATE);
    assert(shu1_safety_latch_is_set() && shu1_safety_latch_is_inhibited());
    assert(persist_attempts == before);
    puts("Safety state/real-mutex OFF/maintenance/SSR-before-NVS/inhibit: PASS");
    return 0;
}
