#pragma once
#include "settings_store.h"
// Called under the policy guard. Copies into a bounded latest-value RAM mailbox.
esp_err_t shu1_settings_defer(const shu1_settings_t *settings);
esp_err_t shu1_settings_deferred_start(void);
// Worker-only; no flash while holding the policy guard or during heating/purge.
void shu1_settings_deferred_flush(void);
void shu1_settings_deferred_status(bool *pending, bool *last_ok);
