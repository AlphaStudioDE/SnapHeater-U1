#pragma once
#include <stddef.h>
#include <stdint.h>
static inline void esp_fill_random(void *dst, size_t len) {
    static uint8_t seed = 1;
    uint8_t *p = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) p[i] = seed++;
}
