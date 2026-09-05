#pragma once
#include "freertos/FreeRTOS.h"
#include <stdint.h>
typedef void *TaskHandle_t;
typedef unsigned TickType_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
BaseType_t xTaskCreate(void (*fn)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
void xTaskNotifyGive(TaskHandle_t);
void vTaskDelay(TickType_t);
uint32_t ulTaskNotifyTake(int, TickType_t);
