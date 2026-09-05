"""Offline regressions for the 2026-09-05 firmware audit.

Compile current production C functions/headers, with explicit fault-injection
stubs. These tests do not connect to hardware or qualify mains circuitry.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("CC", "clang")
IDF = Path(os.environ["IDF_PATH"])
JSON = IDF / "components/json/cJSON"


def source(name):
    return (ROOT / "main" / name).read_text(encoding="utf-8")


def function(name, signature):
    text = source(name)
    start = text.index(signature)
    pos = text.index("{", start)
    depth, quote, escape = 0, None, False
    for end in range(pos, len(text)):
        c = text[end]
        if quote:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if not depth:
                return text[start:end + 1]
    raise ValueError(signature)


COMMON = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "app_state.h"
#include "app_config.h"
#include "control_lease.h"
#define ESP_ERR_INVALID_ARG 101
#define SHU1_AUTH_HEADER "X-DragonBreath-Auth"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
'''


class AuditRegressionTests(unittest.TestCase):
    def compile(self, program, json=False, success=True):
        with tempfile.TemporaryDirectory(prefix="shu1-audit-regression-") as tmp:
            src = Path(tmp) / "test.c"
            exe = Path(tmp) / ("test.exe" if os.name == "nt" else "test")
            src.write_text(program, encoding="utf-8")
            command = [CC, "-std=c11", "-D_CRT_SECURE_NO_WARNINGS",
                       "-I" + str(ROOT / "tests/safety_stubs"),
                       "-I" + str(ROOT / "tests/stubs"), "-I" + str(ROOT / "main"),
                       "-I" + str(ROOT / "build/config"), "-I" + str(JSON), str(src)]
            if json:
                command.append(str(JSON / "cJSON.c"))
            result = subprocess.run(command + ["-o", str(exe)], capture_output=True, text=True)
            if success:
                self.assertEqual(result.returncode, 0, result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            else:
                self.assertNotEqual(result.returncode, 0)

    def test_rest_auth_fails_closed(self):
        self.compile(COMMON + r'''
typedef int nvs_handle_t;
typedef int httpd_req_t;
#define NVS_READONLY 0
static int failure;
static const char *supplied="wrong";
static int nvs_open(const char *n,int mode,int *h) { *h=1; return failure==1 ? -1:0; }
static int nvs_get_str(int h,const char *k,char *out,size_t *n) {
    if (failure==2) return -2;
    strcpy(out, failure==3 ? "" : "configured-secret"); return 0;
}
static void nvs_close(int h) {}
static size_t httpd_req_get_hdr_value_len(int *r,const char *h) { return strlen(supplied); }
static int httpd_req_get_hdr_value_str(int *r,const char *h,char *out,size_t n) { strcpy(out,supplied); return 0; }
''' + function("api_server.c", "static esp_err_t load_control_token(")
           + function("api_server.c", "static bool auth_ok(") + r'''
int main(void) {
    int req=0;
    for (failure=0;failure<=3;failure++) assert(!auth_ok(&req));
    failure=0; supplied="configured-secret"; assert(auth_ok(&req));
    failure=1; assert(!auth_ok(&req));
    failure=2; assert(!auth_ok(&req));
    return 0;
}
''')

    def test_rise_detector_observes_session_and_latches(self):
        safety = source("safety.c")
        self.assertIn("update_rise_detector(&rt, pid_active", safety)
        self.assertNotIn("update_rise_detector(&rt, request_heat", safety)
        self.assertIn("SHU1_HEATER_NO_RISE", safety.split("const bool persistent_hazard =")[1].split(";")[0])
        self.compile(COMMON + function("safety.c", "static void update_rise_detector(") + r'''
int main(void) {
    for (int duty=10;duty<=100;duty+=30) {
        shu1_runtime_t rt={0}; rt.ptc_temp_c=rt.chamber_temp_c=25;
        for (int64_t ms=0;ms<=60000;ms+=500) {
            bool instantaneous_ssr = ms%10000 < duty*100;
            (void)instantaneous_ssr;
            update_rise_detector(&rt,true,ms); // admitted warm-up session, not SSR phase
        }
        assert(rt.heater_fault==SHU1_HEATER_NO_RISE);
    }
    shu1_runtime_t rt={0}; rt.ptc_temp_c=rt.chamber_temp_c=25;
    update_rise_detector(&rt,true,0);
    rt.ptc_temp_c=35;
    update_rise_detector(&rt,true,60000);
    assert(rt.heater_fault==SHU1_HEATER_OK);
    update_rise_detector(&rt,false,60500);
    assert(!rt.rise_detector.active);
    return 0;
}
''')

    def test_every_nvs_write_failure_is_reported(self):
        self.compile(COMMON + r'''
#include "settings_store.h"
typedef int nvs_handle_t;
static int call, fail_at, closed, logged;
static int step(void) { return ++call==fail_at ? 99:ESP_OK; }
static int open_rw(int *h) { *h=1;return step(); }
static int nvs_set_i32(int h,const char *k,int v) {return step();}
static int nvs_set_u32(int h,const char *k,uint32_t v) {return step();}
static int nvs_set_u16(int h,const char *k,uint16_t v) {return step();}
static int nvs_set_str(int h,const char *k,const char *v) {return step();}
static int nvs_commit(int h) {return step();}
static void nvs_close(int h) {++closed;}
static void shu1_event_log_add(const char *a,const char *b,const char *c) {++logged;}
''' + function("settings_store.c", "esp_err_t shu1_settings_store_save_settings(")
           + function("settings_store.c", "esp_err_t shu1_settings_store_save_device_config(") + r'''
int main(void) {
    shu1_settings_t st={0}; shu1_device_config_t cfg={0};
    for (int kind=0;kind<2;kind++) {
        call=0;fail_at=0;closed=0;logged=0;
        assert((kind ? shu1_settings_store_save_device_config(&cfg) : shu1_settings_store_save_settings(&st))==ESP_OK);
        int total=call;
        assert(closed==1 && logged==1);
        for (int i=1;i<=total;i++) {
            call=0; fail_at=i; closed=0; logged=0;
            assert((kind ? shu1_settings_store_save_device_config(&cfg) : shu1_settings_store_save_settings(&st))!=ESP_OK);
            assert(call==i && logged==0 && closed==(i==1?0:1));
        }
    }
    return 0;
}
''')

    def test_factory_reset_preserves_safety_keys_and_does_not_reboot_on_error(self):
        self.compile(COMMON + r'''
typedef int nvs_handle_t;
#define ESP_ERR_NVS_NOT_FOUND 77
#define pdPASS 1
static int calls,fail_at,restarts,closed;
static bool maintenance_allowed=true;
static int step(void) {return ++calls==fail_at ? 99:0;}
shu1_control_guard_t shu1_control_guard_begin(void) {return (shu1_control_guard_t){.held=true};}
void shu1_control_guard_end(shu1_control_guard_t *g) {g->held=false;}
bool shu1_control_maintenance_begin(void) {return maintenance_allowed;}
static int open_rw(int *h) {*h=1;return step();}
static int nvs_erase_key(int h,const char *k) {
    assert(strcmp(k,"fault_latch") && strcmp(k,"fault_code") && strcmp(k,"ctl_token"));
    assert(strcmp(k,"ntc_off_ch") && strcmp(k,"ntc_off_ptc"));
    return step();
}
static int nvs_commit(int h) {return step();}
static void nvs_close(int h) {++closed;}
static void reset_restart_task(void *arg) {}
static int xTaskCreate(void (*fn)(void *),const char *name,int stack,void *arg,int priority,void *out) {++restarts;return pdPASS;}
''' + function("settings_store.c", "esp_err_t shu1_settings_store_factory_reset(") + r'''
int main(void) {
    assert(shu1_settings_store_factory_reset()==ESP_OK);
    int total=calls;assert(restarts==1 && closed==1);
    for (int i=1;i<=total;i++) {
        calls=restarts=closed=0;fail_at=i;
        assert(shu1_settings_store_factory_reset()!=ESP_OK);
        assert(calls==i && restarts==0 && closed==(i==1?0:1));
    }
    maintenance_allowed=false;calls=restarts=0;
    assert(shu1_settings_store_factory_reset()!=ESP_OK && !calls && !restarts);
    return 0;
}
''')

    def test_reset_rejects_mixed_and_duplicate_fields(self):
        self.compile('#include <assert.h>\n#include "command_validation.h"\n' + r'''
int main(void) {
    const char *cases[]={
        "{\"factory_reset\":\"factory-reset\",\"expected_revision\":1}",
        "{\"factory_reset\":\"factory-reset\",\"expected_revision\":1,\"work_on\":true}",
        "{\"factory_reset\":\"wrong\",\"expected_revision\":1}",
        "{\"factory_reset\":\"factory-reset\",\"factory_reset\":\"factory-reset\",\"expected_revision\":1}",
        "{\"factory_reset\":\"factory-reset\"}"};
    for (int i=0;i<5;i++) {cJSON *j=cJSON_Parse(cases[i]); assert(shu1_reset_request_valid(j)==(i==0));cJSON_Delete(j);}
    return 0;
}
''', json=True)

    def test_watchdog_and_rollback_are_compile_requirements(self):
        options = ["SHU1_ENABLE_HEATER_OUTPUT", "ESP_TASK_WDT_EN", "ESP_TASK_WDT_INIT",
                   "ESP_TASK_WDT_PANIC", "BOOTLOADER_APP_ROLLBACK_ENABLE"]
        for bad in [None] + options[1:]:
            program = "".join(f"#define CONFIG_{key} {int(key != bad)}\n" for key in options)
            self.compile(program + '#include "firmware_build_guard.h"\nint main(void){return 0;}\n', success=bad is None)

    def test_websocket_fragmentation_and_bounds(self):
        self.compile('#include <assert.h>\n#include "ws_message_buffer.h"\n' + r'''
int main(void) {
    shu1_ws_buffer_t b={0}; size_t n;
    assert(!shu1_ws_accumulate(&b,1,true,4,0,"ab",2,&n));
    assert(shu1_ws_accumulate(&b,1,true,4,2,"cd",2,&n));
    assert(n==4 && strcmp(b.bytes,"abcd")==0);
    assert(!shu1_ws_accumulate(&b,1,false,2,0,"ab",2,&n));
    assert(!shu1_ws_accumulate(&b,9,true,1,0,"!",1,&n)); // interleaved ping
    assert(shu1_ws_accumulate(&b,0,true,2,0,"cd",2,&n));
    assert(n==4 && strcmp(b.bytes,"abcd")==0);
    assert(!shu1_ws_accumulate(&b,0,true,2,0,"cd",2,&n)); // orphan continuation
    assert(!shu1_ws_accumulate(&b,1,true,20000,0,"ab",2,&n));
    assert(!b.active);
    assert(!shu1_ws_accumulate(&b,1,true,4,0,"ab",2,&n));
    assert(!shu1_ws_accumulate(&b,1,true,4,3,"c",1,&n)); // missing byte
    assert(!b.active);
    return 0;
}
''')

    def test_ntc_strap_configuration_failures_propagate(self):
        self.compile(COMMON + r'''
typedef int gpio_num_t;
typedef struct {uint64_t pin_bit_mask; int mode,intr_type,pull_up_en,pull_down_en;} gpio_config_t;
#define GPIO_MODE_INPUT 0
#define GPIO_INTR_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define ESP_RETURN_ON_ERROR(expr,...) do {int e=(expr);if(e)return e;}while(0)
static int calls, fail_at, reads, level0, level1;
static int gpio_config(const gpio_config_t *c) {return ++calls==fail_at ? 99:0;}
static int gpio_get_level(int pin) {return reads++==0 ? level0:level1;}
static void esp_rom_delay_us(unsigned us) {}
''' + function("ntc.c", "static esp_err_t detect_rref_kohm(") + r'''
int main(void) {
    for (int i=1;i<=3;i++) {
        fail_at=i;calls=reads=0;int out=-1;
        assert(detect_rref_kohm(&out)!=ESP_OK && out==-1);
    }
    fail_at=0;
    for (int i=0;i<3;i++) {
        calls=reads=0;level0=i!=0;level1=i==1;int out=-1;
        assert(detect_rref_kohm(&out)==ESP_OK);
        assert(out==(i==0?82:33) && calls==3);
    }
    return 0;
}
''')

    def test_ble_json_survives_nan_quotes_and_short_buffer(self):
        self.compile(COMMON + r'''
#include "cJSON.h"
static shu1_state_t snapshot;
static bool g_ble_unlocked, g_ble_lease_proven;
void shu1_state_get(shu1_state_t *out) {*out=snapshot;}
int64_t esp_timer_get_time(void) {return 1000000;}
void shu1_control_snapshot(shu1_control_snapshot_t *out) {memset(out,0,sizeof(*out));}
void shu1_control_lease_get_id_for(shu1_control_source_t src,char *out,size_t n) {out[0]=0;}
const char *shu1_control_source_str(shu1_control_source_t src) {return "none";}
const char *shu1_heater_fault_str(shu1_heater_fault_t f) {return "sensor_fault";}
const char *shu1_sensor_status_str(shu1_sensor_status_t s) {return "invalid";}
static const char *shu1_profile_name(int n) {return "test";}
static unsigned shu1_event_log_count(void) {return 0;}
static int shu1_ntc_rref_kohm(void) {return 33;}
''' + function("ble_control.c", "static void build_status_json(") + r'''
int main(void) {
    snapshot.runtime.chamber_temp_c=NAN; snapshot.runtime.ptc_temp_c=INFINITY;
    strcpy(snapshot.printer.active_material,"ABS\"\n\\test");
    char out[8192];
    for (int diagnostic=0;diagnostic<2;diagnostic++) {
        build_status_json(out,sizeof(out),diagnostic);
        cJSON *root=cJSON_Parse(out);assert(root);
        assert(!cJSON_GetObjectItem(root,"err") || !diagnostic);
        if (!diagnostic) {
            assert(cJSON_IsNull(cJSON_GetObjectItem(root,"tc")));
            assert(cJSON_IsNull(cJSON_GetObjectItem(root,"tp")));
            assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(root,"mat")),snapshot.printer.active_material)==0);
        }
        cJSON_Delete(root);
    }
    build_status_json(out,40,false);
    cJSON *root=cJSON_Parse(out);assert(root);
    assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(root,"err")),"status_unavailable")==0);
    cJSON_Delete(root);
    return 0;
}
''', json=True)

    def test_moonraker_only_complete_control_data_renews_freshness(self):
        self.compile(COMMON + r'''
#include "cJSON.h"
static shu1_printer_state_t state;
static int64_t clock_ms=1000;
static char g_chamber_object[64],g_cavity_fan_object[64];
static int64_t now_ms(void) {return clock_ms;}
shu1_printer_state_t shu1_state_get_printer(void) {return state;}
void shu1_state_update_printer(const shu1_printer_state_t *p) {state=*p;}
static void safe_copy(char *d,size_t n,const char *s) {snprintf(d,n,"%s",s);}
static const char *normalize_state(const char *s) {return s;}
static void update_active_tool_from_status(shu1_printer_state_t *p,cJSON *j) {}
static void update_tool_temp_from_status(shu1_printer_state_t *p,cJSON *j) {}
static void update_print_task_config(shu1_printer_state_t *p,cJSON *j) {}
static void apply_chamber_ema(shu1_printer_state_t *p,float f) {}
''' + function("moonraker_client.c", "static void parse_object_update(") + r'''
int main(void) {
    const char *full="{\"print_stats\":{\"state\":\"printing\"},\"heater_bed\":{\"temperature\":65,\"target\":65},\"webhooks\":{\"state\":\"ready\"}}";
    cJSON *j=cJSON_Parse(full);parse_object_update(j);cJSON_Delete(j);
    assert(state.last_update_ms==1000);
    clock_ms=20000;
    const char *partial[]={"{}","{\"heater_bed\":{\"temperature\":64}}","{\"print_stats\":{\"state\":\"printing\"}}"};
    for(int i=0;i<3;i++) {j=cJSON_Parse(partial[i]);parse_object_update(j);cJSON_Delete(j);assert(state.last_update_ms==1000);}
    j=cJSON_Parse(full);parse_object_update(j);cJSON_Delete(j);
    assert(state.last_update_ms==20000);
    return 0;
}
''', json=True)

    def test_control_path_ordering(self):
        for name in ("api_server.c", "ble_control.c"):
            text = source(name)
            self.assertLess(text.index("shu1_reset_request_valid(root)"), text.index("const bool was_work_on"))
            commit = text.index("esp_err_t persist_err = calibration")
            tail = text[commit:]
            self.assertLess(tail.index("if (persist_err != ESP_OK)"), tail.index("shu1_state_update_settings_command(&st)"))
            self.assertNotIn("shu1_control_guard_end", tail[:tail.index("if (persist_err != ESP_OK)")])
        panel = source("physical_controls.c")
        self.assertNotIn("nvs_flash_erase(", panel)
        self.assertNotIn("esp_restart(", panel)
        self.assertIn("shu1_settings_store_factory_reset()", panel)
        boot = source("app_main.c")
        self.assertLess(boot.index("shu1_safety_start()"), boot.index("shu1_wifi_start()"))
        store = source("settings_store.c")
        self.assertNotIn("nvs_erase_all(", store)
        self.assertNotIn("g_state.printer.last_update_ms = esp_timer", source("app_state.c"))
        self.assertIn("printer.objects.query", source("moonraker_client.c"))


if __name__ == "__main__":
    unittest.main()
