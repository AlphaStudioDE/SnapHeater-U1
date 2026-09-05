#pragma once
#include <assert.h>
#include <windows.h>
#define configASSERT(x) assert(x)
#define portMAX_DELAY 0xffffffffU
#define pdTRUE 1
typedef SRWLOCK portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED SRWLOCK_INIT
#define portENTER_CRITICAL(p) AcquireSRWLockExclusive(p)
#define portEXIT_CRITICAL(p) ReleaseSRWLockExclusive(p)
