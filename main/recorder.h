#pragma once
#include "cJSON.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
esp_err_t shu1_recorder_start(void);
cJSON *shu1_recorder_page(uint32_t after);
bool shu1_recorder_usage(cJSON *root);
