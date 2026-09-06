"""Regression checks for storage/log isolation; not hardware timing qualification."""
import unittest
import test_audit_regressions as audit


class NonblockingSafetyTests(unittest.TestCase):
    compile = audit.AuditRegressionTests.compile

    def test_runtime_output_paths_have_no_log_sink(self):
        for name in ("void shu1_heater_set(", "void shu1_heater_force_off(",
                     "void shu1_heater_cut_power("):
            body = audit.function("heater.c", name)
            for forbidden in ("ESP_LOG", "printf(", "nvs_", "SemaphoreTake"):
                self.assertNotIn(forbidden, body)
        for name in ("safety.c", "ntc.c", "physical_controls.c", "ble_control.c"):
            self.assertIn("#define LOG_LOCAL_LEVEL ESP_LOG_NONE", audit.source(name))
        body = audit.source("safety.c")
        self.assertNotIn("shu1_safety_latch_retry_persist(", body)
        self.assertNotIn("shu1_safety_latch_trip(", body)
        self.assertNotIn("shu1_safety_latch_clear(", body)

    def test_event_writer_drops_instead_of_waiting(self):
        self.compile(audit.COMMON + r'''
typedef struct {uint32_t seq; int64_t ms; char level[9],code[32],message[96];} shu1_event_t;
#define SHU1_EVENT_LOG_CAP 32
#define pdTRUE 1
static shu1_event_t g_events[32];
static uint32_t g_seq,g_head;
static int g_lock=1,contended=1;
#define xSemaphoreTake test_take
#define xSemaphoreGive test_give
static int xSemaphoreTake(int lock,unsigned timeout) {assert(timeout==0);return !contended;}
static void xSemaphoreGive(int lock) {}
static int64_t esp_timer_get_time(void) {return 1000000;}
''' + audit.function("event_log.c", "static void copy_event_text(") +
            audit.function("event_log.c", "void shu1_event_log_add(") + r'''
int main(void) {
 shu1_event_log_add("critical","fault","blocked reader"); assert(g_seq==0);
 contended=0; shu1_event_log_add("critical","fault","stored");
 assert(g_seq==1 && !strcmp(g_events[0].level,"critical"));
 return 0;
}
''')

    def test_settings_write_releases_guard_and_never_replays_job(self):
        self.compile(audit.COMMON + r'''
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static int held,writes;
static uint64_t revision,saved_revision;
static bool ready=true,last_ok=true,admit=true,fail;
static shu1_settings_t pending_settings,live;
shu1_control_guard_t shu1_control_guard_begin(void) {assert(!held);held=1;return (shu1_control_guard_t){true};}
void shu1_control_guard_end(shu1_control_guard_t *g) {if(g->held){held=0;g->held=false;}}
bool shu1_control_checkpoint_begin(void) {assert(held);return admit;}
void shu1_control_maintenance_end(void) {assert(held);}
void shu1_event_log_add(const char *a,const char *b,const char *c) {assert(!held);}
esp_err_t shu1_settings_store_save_settings(const shu1_settings_t *s) {
 assert(!held); ++writes; assert(s->target_temp_c==45);
 // Model OFF being accepted while storage is running.
 shu1_control_guard_t g=shu1_control_guard_begin(); live.work_on=false;
 shu1_control_guard_end(&g); return fail ? ESP_ERR_INVALID_STATE:ESP_OK;
}
''' + audit.function("settings_deferred.c", "esp_err_t shu1_settings_defer(") +
            audit.function("settings_deferred.c", "void shu1_settings_deferred_flush(") + r'''
int main(void) {
 shu1_settings_t st={0}; st.target_temp_c=40; st.work_on=true;live=st;
 assert(shu1_settings_defer(&st)==ESP_OK);
 st.target_temp_c=45; assert(shu1_settings_defer(&st)==ESP_OK);
 admit=false;shu1_settings_deferred_flush();assert(writes==0);
 admit=true;fail=true;shu1_settings_deferred_flush();
 assert(writes==1 && saved_revision!=revision && !last_ok && !live.work_on);
 fail=false;shu1_settings_deferred_flush();
 assert(writes==2 && saved_revision==revision && last_ok && !live.work_on);
 shu1_settings_deferred_flush();assert(writes==2);return 0;
}
''')
