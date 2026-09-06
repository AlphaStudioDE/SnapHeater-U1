#pragma once
#include "esp_partition.h"
#include "esp_err.h"
#include <stdbool.h>
esp_err_t shu1_ota_slot_pending(const esp_partition_t *slot, bool pending);
bool shu1_ota_slot_boot_allowed(const esp_partition_t *slot);
