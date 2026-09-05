#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define SHU1_LEASE_ID_LEN 32
#define SHU1_LEASE_TIMEOUT_MS (5U * 60U * 1000U)
#define SHU1_CONTROL_REVISION_ANY UINT32_MAX

typedef enum {
    SHU1_CONTROL_NONE = 0,
    SHU1_CONTROL_REST,
    SHU1_CONTROL_BLE,
    SHU1_CONTROL_PHYSICAL,
} shu1_control_source_t;

typedef enum {
    SHU1_CONTROL_OK = 0,
    SHU1_CONTROL_BUSY,
    SHU1_CONTROL_STALE,
    SHU1_CONTROL_INVALID_LEASE,
    SHU1_CONTROL_NOT_OWNER,
} shu1_control_result_t;

typedef struct {
    shu1_control_source_t owner;
    uint32_t revision;
    bool lease_active;
    uint32_t lease_remaining_ms;
} shu1_control_snapshot_t;

esp_err_t shu1_control_lease_init(void);
shu1_control_result_t shu1_control_claim(shu1_control_source_t source,
                                         bool takeover,
                                         uint32_t expected_revision,
                                         char out_lease[SHU1_LEASE_ID_LEN + 1]);
shu1_control_result_t shu1_control_authorize(shu1_control_source_t source,
                                             const char *lease_id,
                                             uint32_t expected_revision);
bool shu1_control_lease_heartbeat_for(shu1_control_source_t source,
                                      const char *lease_id);
void shu1_control_release_any(void);
bool shu1_control_lease_expired(void);
void shu1_control_snapshot(shu1_control_snapshot_t *out);
void shu1_control_lease_get_id_for(shu1_control_source_t source,
                                   char *out, size_t out_size);
const char *shu1_control_source_str(shu1_control_source_t source);
