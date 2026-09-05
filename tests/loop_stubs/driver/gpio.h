#pragma once
#include <stdint.h>
typedef int gpio_num_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_POSEDGE 1
int gpio_set_level(int,int);
int gpio_config(const gpio_config_t *);
int gpio_install_isr_service(int);
int gpio_isr_handler_add(int,void (*)(void *),void *);
