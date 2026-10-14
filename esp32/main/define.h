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
#define BTN GPIO_NUM_35
#endif

#ifndef ETH_MDC_GPIO
#define ETH_MDC_GPIO GPIO_NUM_23
#endif

#ifndef ETH_MDIO_GPIO
#define ETH_MDIO_GPIO GPIO_NUM_18
#endif

#ifndef ETH_MCU_RESET
#define ETH_MCU_RESET GPIO_NUM_5
#endif

#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR 1
#endif

// RMII clock input on GPIO0.
#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE GPIO_NUM_0
#endif

#ifndef CONFIG_SDA_GPIO
#define CONFIG_SDA_GPIO GPIO_NUM_33
#endif

#ifndef CONFIG_SCL_GPIO
#define CONFIG_SCL_GPIO GPIO_NUM_32
#endif

#ifndef UART_TX_PIN
#define UART_TX_PIN GPIO_NUM_14
#endif

#ifndef UART_RX_PIN
#define UART_RX_PIN GPIO_NUM_12
#endif

#ifndef ETH_POWER_PIN
#define ETH_POWER_PIN -1
#endif

#ifndef ETH_RESET_PIN
#define ETH_RESET_PIN -1
#endif
