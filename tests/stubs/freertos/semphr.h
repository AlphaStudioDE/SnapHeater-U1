#pragma once
#include <stdlib.h>
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return malloc(1); }
static inline int xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks) { (void)h; (void)ticks; return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t h) { (void)h; return 1; }
