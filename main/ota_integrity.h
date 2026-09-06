#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SHU1_OTA_SHA256_HEADER "X-SnapHeater-SHA256"

static inline int shu1_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Exact whole-file SHA-256, not an ESP image signature or authentication. */
static inline bool shu1_ota_parse_sha256(const char *hex, uint8_t out[32]) {
    if (!hex || strlen(hex) != 64) return false;
    for (unsigned i = 0; i < 32; ++i) {
        int hi = shu1_hex_nibble(hex[2*i]), lo = shu1_hex_nibble(hex[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}
