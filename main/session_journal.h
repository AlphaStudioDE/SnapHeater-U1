#pragma once
#include <stdbool.h>
#include "esp_err.h"
esp_err_t shu1_session_journal_init(void);
bool shu1_session_journal_ready(void);
// Sole storage worker; writes before first SSR ON and after safe idle only.
void shu1_session_journal_service(void);
