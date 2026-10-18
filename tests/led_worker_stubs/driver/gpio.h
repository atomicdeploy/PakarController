#pragma once
#include "rmt_tx.h"
constexpr int GPIO_MODE_OUTPUT=1,ESP_ERR_NOT_SUPPORTED=0x106;
int gpio_reset_pin(gpio_num_t);
int gpio_set_direction(gpio_num_t,int);
int gpio_set_level(gpio_num_t,int);
