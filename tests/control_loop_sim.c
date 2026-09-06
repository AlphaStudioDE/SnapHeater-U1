// Runs the unmodified production control_task and real logical GPIO drivers.
// FreeRTOS time, ADC results, GPIO electrical layer and NVS are simulated.
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "safety.c"
#include "driver/gpio.h"

int64_t test_now_us=1000000;
static jmp_buf done;
static int tick,limit=8,scenario,rref=33;
static int pins[22],high_writes,commits;
static bool zc=true,wdt_ok=true,read_error,persist_error;
static shu1_sensor_sample_t input;
static void (*zc_isr)(void *);
static char lease[SHU1_LEASE_ID_LEN+1];
static int64_t lease_deadline;

int gpio_set_level(int pin,int value) {assert(pin>=0 && pin<22);pins[pin]=value;if(pin==18 && value)++high_writes;return 0;}
int gpio_config(const gpio_config_t *c) {return 0;}
int gpio_install_isr_service(int flags) {return 0;}
int gpio_isr_handler_add(int pin,void (*fn)(void *),void *arg) {assert(pin==7);zc_isr=fn;return 0;}
esp_err_t shu1_ntc_read(shu1_sensor_sample_t *out) {*out=input;return read_error ? ESP_ERR_INVALID_STATE:ESP_OK;}
int shu1_ntc_rref_kohm(void) {return rref;}
void shu1_ble_notify_status_now(void) {}
void shu1_event_log_add(const char *l,const char *c,const char *m) {}
esp_err_t esp_task_wdt_add(void *t) {return wdt_ok ? ESP_OK:ESP_ERR_INVALID_STATE;}
esp_err_t esp_task_wdt_reset(void) {return ESP_OK;}
BaseType_t xTaskCreate(void (*fn)(void *),const char *n,unsigned s,void *a,unsigned p,TaskHandle_t *h) {return pdPASS;}
void xTaskNotifyGive(TaskHandle_t t) {}
void vTaskDelay(TickType_t ticks) {assert(0 && "unexpected blocking delay in control loop");}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h) {
    *h=1;if(mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
    assert(!pins[18]); // persisted fault must cut the real logical SSR first
    return persist_error ? ESP_ERR_INVALID_STATE:ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v) {return ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v) {assert(!pins[18]);return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {assert(!pins[18]);++commits;return ESP_OK;}
void nvs_close(nvs_handle_t h) {}

static void step_time(int milliseconds) {
    for(int i=0;i<milliseconds;i+=10) {test_now_us+=10000;if(zc && zc_isr)zc_isr(NULL);}
}
static void stop_command(void) {
    SHU1_CONTROL_GUARD(g);
    shu1_settings_t st=shu1_state_get_settings();shu1_settings_stop(&st);
    shu1_state_update_settings_command(&st);shu1_control_release_any();
}
static void inject(void) {
    if(scenario==37) {
        shu1_printer_state_t pr=shu1_state_get_printer();
        pr.moonraker_connected=true;pr.klippy_ready=true;pr.subscribed=true;
        pr.last_update_ms=test_now_us/1000;
        pr.bed_temp=pr.bed_target=70;
        snprintf(pr.normalized_state,sizeof(pr.normalized_state),"%s",tick>=610 ? "complete":"printing");
        snprintf(pr.print_state,sizeof(pr.print_state),"%s",pr.normalized_state);
        shu1_state_update_printer(&pr);
    }
    if(scenario==38 && tick==610)stop_command();
    if(scenario==39 && tick==610)zc=false;
    if(scenario==39 && tick==615)zc=true;
    if(scenario==40 && tick==610)input.chamber_instant_c=NAN;
    if((scenario==21 || scenario==22 || scenario==23 || scenario==28 || scenario==31) && tick==4)zc=true;
    if(scenario==22 && (tick==3 || tick==5))shu1_safety_latch_request_clear();
    if(scenario==22 && tick==6) {
        SHU1_CONTROL_GUARD(g);
        shu1_settings_t st=shu1_state_get_settings();
        st.work_on=true;st.output_safety_latch_armed=true;
        shu1_state_update_settings_command(&st);
    }
    if((scenario==18 || scenario==19 || scenario>=37) && tick==10) {
        // Successful warm-up isolates lease timeout from the independent NO_RISE trip.
        input.chamber_c=input.chamber_instant_c=54;
        input.ptc_c=input.ptc_instant_c=60;
    }
    if(tick!=2)return;
    switch(scenario) {
    case 0: input.chamber_status=SHU1_SENSOR_OPEN;input.chamber_c=input.chamber_instant_c=NAN;break;
    case 1: input.ptc_status=SHU1_SENSOR_SHORT;input.ptc_c=input.ptc_instant_c=NAN;break;
    case 2: input.chamber_instant_c=NAN;break; // apparently OK status must not admit NaN
    case 3: read_error=true;break;
    case 4: input.ptc_instant_c=105;break; // smoothed telemetry still cold
    case 5: input.chamber_instant_c=85;break;
    case 6: zc=false;break;
    case 7: input.ptc_instant_c=99;break;
    case 8: input.ptc_instant_c=102;break;
    case 10: stop_command();input.ptc_c=input.ptc_instant_c=60;break;
    case 11: input.ptc_instant_c=105;persist_error=true;break;
    case 12: stop_command();break;
    case 14: input.ptc_c=input.ptc_instant_c=25;input.chamber_c=input.chamber_instant_c=25;break;
    case 21: zc=false;break;
    case 22: zc=false;break;
    case 23: zc=false;persist_error=true;break;
    case 25: stop_command();zc=false;break;
    case 26: input.ptc_instant_c=99;break;
    case 27: zc=false;step_time(200);zc=true;break;
    case 28: shu1_safety_latch_request_clear();zc=false;break;
    case 31: zc=false;break;
    case 32: input.chamber_instant_c=NAN;break;
    }
}
uint32_t ulTaskNotifyTake(int clear,TickType_t ticks) {
    shu1_runtime_t rt=shu1_state_get_runtime();
    if(scenario>=37 && scenario<=41 && tick==605) {
        shu1_control_snapshot_t ctl;shu1_control_snapshot(&ctl);
        assert(ctl.owner==SHU1_CONTROL_LOCAL_JOB && !ctl.lease_active);
        assert(shu1_state_get_settings().work_on && !shu1_safety_latch_is_set());
    }
    if(scenario==37 && tick==611)assert(shu1_state_get_settings().tempering_phase==SHU1_TEMPERING_ACTIVE);
    if(scenario==37 && tick>=731)assert(!shu1_state_get_settings().work_on && !pins[18]);
    if(scenario==38 && tick>=610)assert(!shu1_state_get_settings().work_on && !pins[18]);
    if((scenario==39 || scenario==40) && tick>=610) {
        assert(!shu1_state_get_settings().work_on && !pins[18] && shu1_safety_latch_is_set());
    }
    if(scenario==41 && tick>=632)assert(!shu1_state_get_settings().work_on && !pins[18]);
    if(tick==0)assert(!pins[18]); // fan has not reached first ZC yet
    if(tick==1 && scenario<=12)assert(pins[18]); // baseline really exercised ON
    if(tick==1 && scenario>=18 && scenario!=24 && scenario!=30 && scenario!=42)assert(pins[18]);
    if(scenario==42 && tick==605) {
        shu1_control_snapshot_t ctl;shu1_control_snapshot(&ctl);
        assert(ctl.owner==SHU1_CONTROL_LOCAL_JOB);
        assert(shu1_state_get_settings().scheduled_preheat_enabled && !pins[18]);
    }
    if(scenario==42 && tick==625) {
        assert(shu1_state_get_settings().preheat_running);
        assert(shu1_state_get_settings().work_on && !shu1_safety_latch_is_set());
    }
    if(tick>=2 && (scenario<=6 || scenario==11 || scenario==12))assert(!pins[18]);
    if(tick>=2 && (scenario<=5 || scenario==11)) {
        assert(shu1_safety_latch_is_set());
        assert(shu1_fan_triac_is_active());
    }
    if(tick>=2 && (scenario==7 || scenario==8)) {
        assert(!pins[18] && shu1_fan_triac_is_active());
        assert(!shu1_safety_latch_is_set()); // PTC foldback, not hard trip
    }
    if(scenario==9 && tick>=122)assert(shu1_safety_latch_is_set() && !pins[18]);
    if(scenario==10 && tick>=2)assert(!pins[18] && shu1_fan_triac_is_active()); // cooling remains requested
    if(scenario==13)assert(!pins[18] && shu1_safety_latch_is_inhibited());
    if(scenario==14) {
        assert(!pins[18]);
        if(tick<2)assert(shu1_fan_triac_is_active());
        else assert(!shu1_fan_triac_is_active());
    }
    if(scenario==15 && tick==0) {
        SHU1_CONTROL_GUARD(g);
        assert(shu1_control_maintenance_begin());
        assert(!shu1_control_start_allowed() && !shu1_control_schedule_allowed());
    }
    if(scenario==15)assert(!pins[18]);
    if((scenario==18 || scenario==19) && test_now_us>lease_deadline) {
        shu1_control_snapshot_t ctl;
        shu1_control_snapshot(&ctl);
        assert(ctl.owner==SHU1_CONTROL_LOCAL_JOB && !ctl.lease_active);
        assert(shu1_state_get_settings().work_on && !shu1_safety_latch_is_set());
        assert(rt.heater_fault==SHU1_HEATER_OK);
        assert(!shu1_control_lease_heartbeat_for(scenario==18 ? SHU1_CONTROL_REST : SHU1_CONTROL_BLE,lease));
    }
    if(scenario==20 && tick==1) {
        SHU1_CONTROL_GUARD(g);
        assert(!shu1_control_maintenance_begin()); // OTA cannot reserve an active heater
    }
    if((scenario==6 || scenario==21 || scenario==23 || scenario==26 || scenario==27 || scenario==28 || scenario==31) && tick>= (scenario==26 ? 3:2)) {
        shu1_settings_t st=shu1_state_get_settings();
        assert(!pins[18] && shu1_safety_latch_is_set());
        assert(rt.heater_fault==SHU1_HEATER_ZERO_CROSS_LOST);
        assert(!st.work_on && !st.output_safety_latch_armed);
        assert(shu1_fan_triac_is_active());
        if((scenario==21 || scenario==23) && tick>=4)assert(pins[3]);
    }
    if(scenario==22) {
        if(tick>=2 && tick<=3)assert(shu1_safety_latch_is_set() && !pins[18]);
        if(tick>=4 && tick<=5) {
            assert(!pins[18]);
            assert(!shu1_state_get_settings().output_safety_latch_armed);
        }
        if(tick==5)assert(!shu1_safety_latch_is_set());
        if(tick>=7)assert(pins[18] && !shu1_safety_latch_is_set());
    }
    if(scenario==24)assert(!pins[18] && !shu1_safety_latch_is_set());
    if(scenario==30)assert(!pins[18] && !shu1_state_get_settings().output_safety_latch_armed);
    if(scenario==32 && tick>=2) {
        assert(!pins[18] && shu1_safety_latch_is_set());
        assert(!shu1_state_get_settings().work_on);
        assert(rt.heater_fault==SHU1_HEATER_SENSOR_FAULT);
    }
    if(scenario==29) {
        shu1_settings_t st=shu1_state_get_settings();
        assert(!st.sensors_verified && !st.heater_output_verified && !st.fan_output_verified);
        if(tick>=1)assert(pins[18] && st.output_safety_latch_armed);
    }
    if(scenario==25 && tick>=2)assert(!pins[18] && !shu1_safety_latch_is_set());
    if(scenario==26 && tick==2) {assert(!pins[18]);zc=false;}
    if(scenario==17 && tick>=122) {
        assert(!shu1_safety_latch_is_set());
        // Two plausible frozen values close to target are not detectable as
        // failed thermistors with these inputs alone. Record, do not hide, this limit.
        assert(rt.heater_fault==SHU1_HEATER_OK);
    }
    if(scenario==16 && tick==1) {
        assert(pins[18]);
        // Deliberately do NOT simulate an ESP watchdog interrupt here.
        // CPU code alone cannot cut a pin while it is not being executed.
        step_time(20000);assert(pins[18]);
        shu1_heater_preinit_off();assert(!pins[18] && !pins[3]);
        puts("OBSERVATION: stalled task held SSR; simulated reboot preinit cleared it; real WDT not emulated");
        longjmp(done,1);
    }
    if(++tick>=limit)longjmp(done,1); // guard is already released by production loop
    inject();step_time(500);
    return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);scenario=atoi(argv[1]);
    input.chamber_status=input.ptc_status=SHU1_SENSOR_OK;
    input.chamber_c=input.chamber_instant_c=input.ptc_c=input.ptc_instant_c=25;
    if(scenario==8)rref=82;
    if(scenario==9)limit=130;
    if(scenario==13)wdt_ok=false;
    if(scenario==24)zc=false;
    if(scenario==14)input.ptc_c=input.ptc_instant_c=60;
    if(scenario==17) {input.chamber_c=input.chamber_instant_c=54;input.ptc_c=input.ptc_instant_c=60;limit=130;}
    shu1_state_init();assert(shu1_control_lease_init()==ESP_OK);
    assert(shu1_safety_latch_init()==ESP_OK);
    assert(shu1_heater_preinit_off()==ESP_OK && !pins[18] && !pins[3]);
    assert(shu1_heater_init()==ESP_OK);
    shu1_settings_t st={0};
    if (scenario >= 33 && scenario <= 36) {
        // Exercise the production print-finish/ramp functions with no physical I/O.
        shu1_printer_state_t printer = {0};
        shu1_runtime_t runtime = {0};
        st.work_on = true;
        st.work_mode = SHU1_MODE_AUTO;
        st.target_temp_c = scenario == 36 ? 30 : 50;
        st.tempering_enabled = scenario != 34;
        st.finish_conditioning_mode = scenario == 34 ? SHU1_FINISH_NORMAL_COOLDOWN : SHU1_FINISH_TEMPERING;
        st.tempering_duration_min = 40;
        st.tempering_end_temp_c = 35;
        runtime.chamber_temp_c = st.target_temp_c + 4; // Sensor overshoot must not raise the ramp start.
        printer.moonraker_connected = true;
        printer.last_update_ms = 1000;
        snprintf(printer.normalized_state, sizeof(printer.normalized_state), "printing");
        update_auto_context(&st, &printer, &runtime, 1000);
        assert(st.work_on && st.tempering_phase == SHU1_TEMPERING_IDLE);
        snprintf(printer.normalized_state, sizeof(printer.normalized_state), "complete");
        if (scenario == 35) printer.moonraker_connected = false;
        update_auto_context(&st, &printer, &runtime, 2000);
        if (scenario == 33 || scenario == 36) {
            assert(st.work_on && st.tempering_phase == SHU1_TEMPERING_ACTIVE);
            assert(st.tempering_start_temp_c == st.target_temp_c);
            update_tempering(&st, 2000 + 20 * 60000);
            // 50 -> 35: midpoint 42.5 rounds up to 43. Never ramp UP from 30.
            assert(st.tempering_current_target_c == (scenario == 36 ? 30 : 43) && st.work_on);
            update_tempering(&st, 2000 + 40 * 60000);
            assert(st.tempering_current_target_c == (scenario == 36 ? 30 : 35) && !st.work_on);
            assert(st.tempering_phase == SHU1_TEMPERING_COMPLETE);
        } else {
            assert(st.tempering_phase == SHU1_TEMPERING_IDLE);
            if (scenario == 34) assert(!st.work_on);
        }
        assert(!pins[18]);
        puts("AUTO finish policy PASS");
        return 0;
    }
    st.work_on=scenario!=14 && scenario!=15;
    st.work_mode=SHU1_MODE_POWER_ON;st.target_temp_c=55;st.cool_release_c=35;
    st.manual_session_max_min=120;
    if(scenario==42) {
        st.work_on=false;st.scheduled_preheat_enabled=true;
        st.scheduled_preheat_start_ms=test_now_us/1000+310000;
        st.scheduled_preheat_target_c=55;st.scheduled_preheat_hold_min=15;
    }
    if(scenario==37) {
        st.work_mode=SHU1_MODE_AUTO;
        st.tempering_enabled=true;st.finish_conditioning_mode=SHU1_FINISH_TEMPERING;
        st.tempering_duration_min=1;st.tempering_end_temp_c=35;
        inject();
    }
    if(scenario==41) {
        st.work_mode=SHU1_MODE_DRYING;st.drying_running=true;st.custom_temp_c=55;
        st.drying_end_ms=test_now_us/1000+315000;
    }
    // Model an explicitly supervised arm. Keep the production runtime gate ON.
    st.output_safety_latch_enabled=true;
    st.output_safety_latch_armed=st.work_on;
    st.sensors_verified=st.heater_output_verified=st.fan_output_verified=true;
    if(scenario>=29) {
        st.output_safety_latch_armed=false;
        st.sensors_verified=st.heater_output_verified=st.fan_output_verified=false;
        if(scenario==30)st.work_on=false;
    }
    shu1_state_update_settings(&st);
    step_time(10); // an accepted ZC, but fan and SSR are still physically OFF
    if(scenario==18 || scenario==19 || scenario>=37) {
        SHU1_CONTROL_GUARD(g);
        assert(shu1_control_claim(scenario==18 ? SHU1_CONTROL_REST:SHU1_CONTROL_BLE,
            true,SHU1_CONTROL_REVISION_ANY,lease)==SHU1_CONTROL_OK);
        lease_deadline=test_now_us+(int64_t)SHU1_LEASE_TIMEOUT_MS*1000;
        limit=SHU1_LEASE_TIMEOUT_MS/500+5;
        if(scenario>=37)limit=735;
    }
    if(!setjmp(done))control_task(NULL);
    if(scenario==17)puts("OBSERVATION: plausible frozen sensors near target were NOT diagnosed; heating remained permitted");
    if(scenario==21)puts("Recovered ZC permits cooling only; heating remains latched and disarmed");
    printf("scenario=%d ticks=%d SSR_high_writes=%d fault=%s persisted=%d PASS\n",
        scenario,tick,high_writes,shu1_heater_fault_str(shu1_state_get_runtime().heater_fault),commits);
    return 0;
}
