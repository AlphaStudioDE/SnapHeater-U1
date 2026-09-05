#pragma once
#include <assert.h>
#define ESP_RETURN_ON_ERROR(expr, ...) do {int err=(expr);if(err)return err;}while(0)
