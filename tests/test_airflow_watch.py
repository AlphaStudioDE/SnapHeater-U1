"""Production advisory with synthetic thermal inputs, not airflow qualification."""
import unittest
import test_audit_regressions as audit

class AirflowWatchTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_ssr_off_phases_do_not_erase_observation(self):
        self.compile(audit.COMMON + r'''
#include "airflow_watch.h"
int main(void) {
    for(int mode=0;mode<5;mode++) {
        shu1_airflow_watch_t w={0};
        for(int i=0;i<=240;i++) {
            bool on=mode==0 ? true:mode==1 ? (i%20)<10:false;
            if(mode==4)on=true;
            float chamber=mode==4 ? 50+(float)i/240:50;
            shu1_airflow_result_t r=shu1_airflow_watch_step(&w,mode!=3,on,chamber,100,i*500);
            if(i<240 || mode==2 || mode==3)assert(r==SHU1_AIRFLOW_OBSERVING);
            else assert(r==(mode==4 ? SHU1_AIRFLOW_NO_ANOMALY:SHU1_AIRFLOW_SUSPECT));
        }
    }
    return 0;
}
''')

    def test_pause_invalid_gap_and_time_reversal_reset_evidence(self):
        self.compile(audit.COMMON + r'''
#include "airflow_watch.h"
int main(void) {
    for(int mode=0;mode<4;mode++) {
        shu1_airflow_watch_t w={0}; int64_t now=0;
        for(int i=0;i<400;i++,now+=500) {
            bool active=true;float chamber=50;
            if(i==200) {
                if(mode==0)active=false;
                if(mode==1)chamber=NAN;
                if(mode==2)now+=2000;
                if(mode==3)now=0;
            }
            assert(shu1_airflow_watch_step(&w,active,true,chamber,100,now)==SHU1_AIRFLOW_OBSERVING);
        }
    }
    return 0;
}
''')
