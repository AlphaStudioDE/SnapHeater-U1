#pragma once

// Check the generated sdkconfig, not merely sdkconfig.defaults.
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT && (!CONFIG_ESP_TASK_WDT_EN || !CONFIG_ESP_TASK_WDT_INIT || !CONFIG_ESP_TASK_WDT_PANIC)
#error "Active heater requires initialized task watchdog with panic/reset"
#endif
#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "Dual-slot OTA requires rollback-enabled build; verify installed bootloader separately"
#endif
