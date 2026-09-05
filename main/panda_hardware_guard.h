/*
 * SPDX-License-Identifier: MIT
 * Stock Panda Breath map based on plastikman/DragonBreath.
 * Included AFTER sdkconfig and app_config defaults; -1 may disable an output,
 * but must never redirect a peripheral onto another physical circuit.
 */
#pragma once

// ESP-IDF peripherals also own pins; application-only checks are insufficient.
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG || CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG
#error "Panda hardware: native USB console conflicts with SSR18/Rref19; use CH340 UART"
#endif
#if CONFIG_ESP_CONSOLE_UART_CUSTOM && \
    (CONFIG_ESP_CONSOLE_UART_TX_GPIO != 21 || CONFIG_ESP_CONSOLE_UART_RX_GPIO != 20)
#error "Panda hardware: custom console must retain UART TX21/RX20"
#endif
#if CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS && CONFIG_SHU1_ENABLE_POWER_LED && CONFIG_ESP_CONSOLE_UART
#error "Panda hardware: Power LED21 and UART console cannot be enabled together"
#endif
#ifdef CONFIG_PB_DEVBOARD_SAFE
#error "Panda hardware: upstream injection backend is not part of production SnapHeater"
#endif

#define SHU1_PIN_DISABLED_OR(pin, expected) ((pin) == -1 || (pin) == (expected))

#if !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_FAN_GPIO, 3) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_HEATER_GPIO, 18) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_ZERO_CROSS_GPIO, 7)
#error "Panda hardware: fan=3, SSR=18, ZC=7; only -1 may disable a pin"
#endif
#if (CONFIG_SHU1_FAN_GPIO >= 0 && !CONFIG_SHU1_FAN_ACTIVE_HIGH) || \
    (CONFIG_SHU1_HEATER_GPIO >= 0 && !CONFIG_SHU1_HEATER_ACTIVE_HIGH)
#error "Panda hardware: fan and SSR must be active HIGH"
#endif
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL && \
    (CONFIG_SHU1_FAN_GPIO != 3 || CONFIG_SHU1_ZERO_CROSS_GPIO != 7)
#error "Panda hardware: enabled fan requires gate 3 and ZC 7"
#endif
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT && \
    (CONFIG_SHU1_HEATER_GPIO != 18 || !CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL)
#error "Panda hardware: enabled heater requires SSR 18 and enabled ZC fan"
#endif
#if CONFIG_SHU1_RREF_STRAP_GPIO != 19 || CONFIG_SHU1_CHAMBER_ADC_CH != 0 || \
    CONFIG_SHU1_PTC_ADC_CH != 1
#error "Panda hardware: Rref=19, chamber ADC1_CH0, PTC ADC1_CH1"
#endif
// There is no spare generic status LED/button on the stock PCB.
#if CONFIG_SHU1_STATUS_LED_GPIO != -1 || CONFIG_SHU1_BUTTON_GPIO != -1
#error "Panda hardware: generic status LED/button must be disabled"
#endif
#if !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_BUTTON_POWER_GPIO, 9) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_BUTTON_AUTO_GPIO, 8) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_BUTTON_ON_GPIO, 10) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_BUTTON_DRY_GPIO, 2) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_LED_AUTO_GPIO, 6) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_LED_ON_GPIO, 5) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_LED_DRY_GPIO, 4) || \
    !SHU1_PIN_DISABLED_OR(CONFIG_SHU1_LED_POWER_GPIO, 21)
#error "Panda hardware: panel remapping is forbidden (power pins and inputs are reserved)"
#endif
#if CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS && \
    (!CONFIG_SHU1_BUTTON_ACTIVE_LOW || !CONFIG_SHU1_LED_ACTIVE_HIGH)
#error "Panda hardware: panel buttons are active LOW and LEDs active HIGH"
#endif

#undef SHU1_PIN_DISABLED_OR
