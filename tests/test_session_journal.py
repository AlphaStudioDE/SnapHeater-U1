import unittest
import test_audit_regressions as audit

class SessionJournalTests(unittest.TestCase):
    compile=audit.AuditRegressionTests.compile

    def test_prepare_failure_power_loss_and_clean_idle(self):
        implementation="\n".join(line for line in audit.source("session_journal.c").splitlines()
                                 if not line.startswith("#include"))
        self.compile(audit.COMMON+r'''
#include "session_journal.h"
#include "nvs.h"
#include <stdatomic.h>
static bool held,latched,inhibited,cold=true,maintenance,fail,uncertain_commit;
static uint8_t durable,staged;
static shu1_settings_t live;
shu1_control_guard_t shu1_control_guard_begin(void) {assert(!held);held=true;return (shu1_control_guard_t){true};}
void shu1_control_guard_end(shu1_control_guard_t *g) {if(g->held){held=false;g->held=false;}}
bool shu1_safety_latch_is_set(void) {return latched;}
bool shu1_safety_latch_is_inhibited(void) {return inhibited;}
void shu1_safety_latch_inhibit(void) {inhibited=true;}
void shu1_safety_latch_trip_volatile(shu1_heater_fault_t f) {latched=true;}
void shu1_event_log_add(const char *a,const char *b,const char *c) {}
shu1_settings_t shu1_state_get_settings(void) {return live;}
bool shu1_control_checkpoint_begin(void) {assert(held);maintenance=cold;return cold;}
void shu1_control_maintenance_end(void) {assert(held);maintenance=false;}
esp_err_t nvs_open(const char *n,int mode,nvs_handle_t *h) {assert(!held);*h=1;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v) {*v=durable;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v) {
 assert(!held && !shu1_session_journal_ready());
 if(!v)assert(maintenance);staged=v;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
 assert(!held);
 if(!fail || uncertain_commit)durable=staged;
 return fail ? ESP_ERR_INVALID_STATE:ESP_OK;
}
void nvs_close(nvs_handle_t h) {}
''' + implementation+r'''
int main(void) {
 assert(shu1_session_journal_init()==ESP_OK && !latched && !shu1_session_journal_ready());
 live.work_on=true;fail=true;
 shu1_session_journal_service();
 assert(inhibited && !shu1_session_journal_ready() && durable==0);
 inhibited=false;fail=false;
 shu1_session_journal_service();
 assert(shu1_session_journal_ready() && durable==1);
 // Reboot during heat or before asynchronous hazard persistence: marker wins.
 assert(shu1_session_journal_init()==ESP_OK);
 assert(latched && !shu1_session_journal_ready());
 live.work_on=false;shu1_session_journal_service();assert(durable==1);
 // Explicit safe fault clear does not erase marker until safe idle checkpoint.
 latched=false;cold=false;shu1_session_journal_service();assert(durable==1);
 cold=true;shu1_session_journal_service();assert(durable==0 && !shu1_session_journal_ready());
 assert(shu1_session_journal_init()==ESP_OK && !latched);
 // Ambiguous failed commit may already be durable. Never admit SSR on error.
 live.work_on=true;fail=true;uncertain_commit=true;
 shu1_session_journal_service();assert(inhibited && !shu1_session_journal_ready() && durable==1);
 inhibited=false;assert(shu1_session_journal_init()==ESP_OK && latched);
 durable=2;assert(shu1_session_journal_init()!=ESP_OK);return 0;
}
''')
