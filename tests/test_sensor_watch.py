"""Compile the production passive sensor monitor and ADC producer with offline I/O."""
import unittest
import test_audit_regressions as audit

BASE = audit.COMMON + r'''
#include "sensor_watch.h"
static shu1_sensor_sample_t sample(void) {
    return (shu1_sensor_sample_t){.sequence=1,.started_us=1000000,.completed_us=1000010,
        .chamber_status=SHU1_SENSOR_OK,.ptc_status=SHU1_SENSOR_OK,
        .chamber_instant_c=54,.ptc_instant_c=60,.chamber_raw=2000,.ptc_raw=2100};
}
'''

class SensorWatchTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_freshness_replay_age_order_and_wrap(self):
        self.compile(BASE + r'''
int main(void) {
    shu1_sensor_watch_t w={0};shu1_sensor_sample_t s=sample();
    assert(shu1_sample_fresh(&w,&s,1000000,1000010));
    assert(!shu1_sample_fresh(&w,&s,1000000,1000010));
    s.sequence=2;
    assert(!shu1_sample_fresh(&w,&s,1000001,1000010));
    assert(!shu1_sample_fresh(&w,&s,1000000,1000009));
    assert(!shu1_sample_fresh(&w,&s,1000000,2500001));
    s.completed_us=999999;assert(!shu1_sample_fresh(&w,&s,1000000,1000010));
    s=sample();s.sequence=UINT32_MAX;
    assert(shu1_sample_fresh(&w,&s,1000000,1000010));
    s.sequence=1;assert(shu1_sample_fresh(&w,&s,1000000,1000010));
    s.sequence=0;assert(!shu1_sample_fresh(&w,&s,1000000,1000010));
    return 0;
}
''')

    def test_stable_temperatures_idle_pause_noise_and_one_channel_activity(self):
        self.compile(BASE + r'''
int main(void) {
    for(int mode=0;mode<5;mode++) {
        shu1_sensor_watch_t w={0};shu1_sensor_sample_t s=sample();
        for(int i=0;i<3600;i++) {
            // Constant displayed/instant temperatures with raw activity are healthy.
            s.chamber_raw=2000+(mode==0 ? i%2:0);
            s.ptc_raw=2100+(mode==1 ? i%2:0);
            bool job=mode!=2; // idle or user pause
            bool on=mode==3 ? true:mode==4 ? false:(i/10)%2;
            assert(shu1_sensor_watch_step(&w,&s,true,job,on,1000000LL+i*500000LL)==SHU1_SAMPLE_HEALTHY);
        }
    }
    return 0;
}
''')

    def test_dual_freeze_boundaries_and_recovery(self):
        self.compile(BASE + r'''
int main(void) {
    shu1_sensor_watch_t w={0};shu1_sensor_sample_t s=sample();
    for(int i=0;i<=720;i++) {
        shu1_sample_health_t h=shu1_sensor_watch_step(&w,&s,true,true,(i/10)%2,1000000LL+i*500000LL);
        assert(h==(i==720 ? SHU1_SAMPLE_FROZEN:i>=120 ? SHU1_SAMPLE_WARNING:SHU1_SAMPLE_HEALTHY));
    }
    assert(shu1_sensor_watch_step(&w,&s,true,false,false,362000000)==SHU1_SAMPLE_FROZEN);
    s.chamber_raw++;
    assert(shu1_sensor_watch_step(&w,&s,true,false,false,362500000)==SHU1_SAMPLE_FROZEN);
    assert(shu1_sensor_watch_step(&w,&s,false,false,false,363000000)==SHU1_SAMPLE_STALE);
    s.ptc_raw++;
    assert(shu1_sensor_watch_step(&w,&s,true,false,false,363500000)==SHU1_SAMPLE_HEALTHY);
    return 0;
}
''')

    def test_evidence_resets_on_missing_invalid_or_paused_samples(self):
        self.compile(BASE + r'''
int main(void) {
    for(int mode=0;mode<5;mode++) {
        shu1_sensor_watch_t w={0};shu1_sensor_sample_t s=sample();
        int64_t now=1000000;
        for(int i=0;i<160;i++,now+=500000) {
            bool fresh=true,job=true;
            s.chamber_status=SHU1_SENSOR_OK;s.chamber_instant_c=54;
            if(i==80) {
                if(mode==0)fresh=false;
                if(mode==1)s.chamber_status=SHU1_SENSOR_INVALID;
                if(mode==2)job=false;
                if(mode==3)now+=2000000;
                if(mode==4)s.chamber_instant_c=NAN;
            }
            shu1_sample_health_t h=shu1_sensor_watch_step(&w,&s,fresh,job,(i/10)%2,now);
            assert(h!=SHU1_SAMPLE_FROZEN);
            assert(h!=SHU1_SAMPLE_WARNING);
        }
    }
    return 0;
}
''')

    def test_adc_producer_stamps_after_both_channel_attempts(self):
        self.compile(BASE + r'''
static bool g_ntc_ready=true;
static uint32_t g_sample_sequence;
static int calls,fail_channel=-1;
static int64_t clock_us=1000000;
static int64_t esp_timer_get_time(void) {return clock_us;}
static esp_err_t read_channel(int i,int *raw,float *instant,float *smoothed,shu1_sensor_status_t *status) {
    calls++;clock_us+=20;
    if(i==fail_channel)return ESP_ERR_INVALID_STATE;
    *raw=2000+i;*instant=*smoothed=i==1 ? 106:54;*status=SHU1_SENSOR_OK;return ESP_OK;
}
''' + audit.function("ntc.c", "esp_err_t shu1_ntc_read(") + r'''
int main(void) {
    shu1_sensor_sample_t s={0};
    assert(shu1_ntc_read(&s)==ESP_OK && calls==2);
    assert(s.sequence==1 && s.started_us==1000000 && s.completed_us==1000040);
    fail_channel=0;
    assert(shu1_ntc_read(&s)==ESP_OK && calls==4 && s.sequence==2);
    assert(s.chamber_status==SHU1_SENSOR_INVALID && s.chamber_raw==0);
    assert(s.ptc_status==SHU1_SENSOR_OK && s.ptc_instant_c==106);
    fail_channel=1;
    assert(shu1_ntc_read(&s)==ESP_OK && calls==6);
    assert(s.ptc_status==SHU1_SENSOR_INVALID && s.chamber_status==SHU1_SENSOR_OK);
    g_sample_sequence=UINT32_MAX;assert(shu1_ntc_read(&s)==ESP_OK && s.sequence==1);
    g_ntc_ready=false;assert(shu1_ntc_read(&s)!=ESP_OK && calls==8);
    assert(shu1_ntc_read(NULL)!=ESP_OK);
    return 0;
}
''')

    def test_minimum_output_evidence_and_raw_validity(self):
        self.compile(BASE + r'''
int main(void) {
    shu1_sensor_watch_t w={0};shu1_sensor_sample_t s=sample();
    for(int i=0;i<=360;i++) {
        // Five transitions and more than five minutes still do not suffice.
        shu1_sample_health_t h=shu1_sensor_watch_step(&w,&s,true,true,(i/60)%2,1000000LL+i*1000000LL);
        assert(h==(i==360 ? SHU1_SAMPLE_WARNING:SHU1_SAMPLE_HEALTHY));
    }
    for(int mode=0;mode<2;mode++) {
        w=(shu1_sensor_watch_t){0};
        for(int i=0;i<1000;i++) {
            bool on=i<12 ? i%2:0; // Plenty of transitions but insufficient ON time.
            if(mode)on=!on; // Symmetric OFF-time requirement.
            assert(shu1_sensor_watch_step(&w,&s,true,true,on,1000000LL+i*500000LL)==SHU1_SAMPLE_HEALTHY);
        }
    }
    int invalid[]={-1,0,0x14,0xFFE,0xFFF};
    for(int ch=0;ch<2;ch++)for(int i=0;i<5;i++) {
        w=(shu1_sensor_watch_t){0};s=sample();
        if(ch)s.ptc_raw=invalid[i];else s.chamber_raw=invalid[i];
        assert(shu1_sensor_watch_step(&w,&s,true,true,true,1000000)==SHU1_SAMPLE_INVALID);
    }
    return 0;
}
''')

if __name__ == "__main__":
    unittest.main()
