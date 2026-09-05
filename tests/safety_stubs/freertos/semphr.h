#pragma once
#include "FreeRTOS.h"
#include <stdlib.h>
typedef SRWLOCK *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t h = malloc(sizeof(*h));
    if (h) InitializeSRWLock(h);
    return h;
}
static inline int xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks) {
    if (!ticks) return TryAcquireSRWLockExclusive(h) ? 1 : 0;
    AcquireSRWLockExclusive(h); return 1;
}
static inline int xSemaphoreGive(SemaphoreHandle_t h) { ReleaseSRWLockExclusive(h); return 1; }
