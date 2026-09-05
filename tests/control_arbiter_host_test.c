#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "control_lease.h"

int64_t test_now_us = 1000000;

int main(void) {
    assert(shu1_control_lease_init() == ESP_OK);
    shu1_control_snapshot_t s;
    shu1_control_snapshot(&s);
    assert(s.owner == SHU1_CONTROL_NONE && s.revision == 0);

    char rest_lease[SHU1_LEASE_ID_LEN + 1];
    assert(shu1_control_claim(SHU1_CONTROL_REST, false,
                              SHU1_CONTROL_REVISION_ANY, rest_lease) == SHU1_CONTROL_OK);
    assert(strlen(rest_lease) == SHU1_LEASE_ID_LEN);
    shu1_control_snapshot(&s);
    assert(s.owner == SHU1_CONTROL_REST && s.lease_active && s.revision == 1);
    assert(shu1_control_claim(SHU1_CONTROL_BLE, false, s.revision, NULL) == SHU1_CONTROL_BUSY);
    assert(shu1_control_authorize(SHU1_CONTROL_REST, "wrong", s.revision) ==
           SHU1_CONTROL_INVALID_LEASE);
    assert(shu1_control_authorize(SHU1_CONTROL_REST, rest_lease, s.revision) ==
           SHU1_CONTROL_OK);
    assert(shu1_control_authorize(SHU1_CONTROL_REST, rest_lease, s.revision) ==
           SHU1_CONTROL_STALE);
    assert(!shu1_control_lease_heartbeat_for(SHU1_CONTROL_BLE, rest_lease));
    assert(shu1_control_lease_heartbeat_for(SHU1_CONTROL_REST, rest_lease));

    shu1_control_snapshot(&s);
    char ble_lease[SHU1_LEASE_ID_LEN + 1];
    assert(shu1_control_claim(SHU1_CONTROL_BLE, true, s.revision, ble_lease) == SHU1_CONTROL_OK);
    assert(strlen(ble_lease) == SHU1_LEASE_ID_LEN && strcmp(ble_lease, rest_lease) != 0);
    assert(!shu1_control_lease_heartbeat_for(SHU1_CONTROL_REST, rest_lease));
    test_now_us += (int64_t)SHU1_LEASE_TIMEOUT_MS * 1000 + 1;
    assert(shu1_control_lease_expired());

    shu1_control_release_any();
    shu1_control_snapshot(&s);
    assert(s.owner == SHU1_CONTROL_NONE && !s.lease_active);
    assert(shu1_control_claim(SHU1_CONTROL_PHYSICAL, true,
                              SHU1_CONTROL_REVISION_ANY, NULL) == SHU1_CONTROL_OK);
    shu1_control_snapshot(&s);
    assert(s.owner == SHU1_CONTROL_PHYSICAL && !s.lease_active);
    puts("SnapHeater control arbiter host tests: PASS");
    return 0;
}
