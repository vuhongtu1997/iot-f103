#pragma once

#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#else
#define GPIO_NUM_0 0
#define GPIO_NUM_2 2
#define GPIO_NUM_5 5
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_18 18
#define GPIO_NUM_23 23
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33
#define GPIO_NUM_35 35
#endif

#ifndef LED_SETTING
#define LED_SETTING GPIO_NUM_2
#endif

#ifndef LED_STATUS
#define LED_STATUS GPIO_NUM_13
#endif

#ifndef BTN
#if CONFIG_IDF_TARGET_ESP32C3
#define BTN GPIO_NUM_9
#else
#define BTN GPIO_NUM_35
#endif
#endif

#ifndef RS485_TX_PIN
#define RS485_TX_PIN GPIO_NUM_4
#endif

#ifndef RS485_RX_PIN
#define RS485_RX_PIN GPIO_NUM_5
#endif

#ifndef RS485_DE_PIN
#define RS485_DE_PIN GPIO_NUM_6
#endif
