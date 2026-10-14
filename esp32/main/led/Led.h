/**
 * @file Led.h
 * @brief Khai báo các hàm và cấu hình điều khiển LED trạng thái và GPIO reset BLE.
 *
 * File này cung cấp các định nghĩa và hàm để điều khiển LED hiển thị trạng thái dịch vụ,
 * trạng thái kết nối Internet, và reset module BLE thông qua GPIO.
 */

#pragma once

#include "../define.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdbool.h>

/**
 * @enum LED_Mode LED chế độ hoạt động
 */
typedef enum
{
    LED_OFF = 0, /**< LED tắt  */
    LED_ON,      /**< LED bật */
    LED_BLINK,   /**< LED nhấp nháy chậm */
    LED_FLASH    /**< LED nhấp nháy nhanh */
} led_mode_t;

/**
 * @enum khai báo gpio num
 */
#define LED_GPIO_SERVICE GPIO_NUM_2   /**< GPIO điều khiển LED Service/Status. */
#define LED_GPIO_INTERNET GPIO_NUM_13 /**< GPIO điều khiển LED Internet/Setting. */

/**
 * @brief Khởi tạo hệ thống điều khiển LED.
 *
 * Thiết lập bộ LED, cấu hình các chân GPIO và trạng thái ban đầu của LED.
 */
void Led_init();

/**
 * @brief Bật hoặc tắt LED Internet.
 *
 * @param status true để bật, false để tắt.
 */
void SetLedInternet(bool on);

/**
 * @brief Bật hoặc tắt LED Service.
 *
 * @param status true để bật, false để tắt.
 */
void SetLedService(bool on);

/**
 * @brief Lấy trạng thái hiện tại của LED Internet.
 *
 * @return true nếu LED đang bật.
 */
bool GetStatusLedInternet();

/**
 * @brief Lấy trạng thái hiện tại của LED Service.
 *
 * @return true nếu LED đang bật.
 */
bool GetStatusLedService();

/**
 * @brief Bật chế độ nhấp nháy chậm cho LED Service.
 */
void BlinkLedService();

/**
 * @brief Bật chế độ nhấp nháy nhanh cho LED Service.
 */
void FlashLedService();

/**
 * @brief Bật chế độ nhấp nháy chậm cho LED Internet.
 */
void BlinkLedInternet();

/**
 * @brief Bật chế độ nhấp nháy nhanh cho LED Internet.
 */
void FlashLedInternet();

/**
 * @brief Lấy chế độ hoạt động hiện tại của LED Service.
 *
 * @return Một trong các giá trị LED_ON_OFF, LED_BLINK, LED_FLASH.
 */
led_mode_t GetModeLedService();

/**
 * @brief Lấy chế độ hoạt động hiện tại của LED Internet.
 *
 * @return Một trong các giá trị LED_ON_OFF, LED_BLINK, LED_FLASH.
 */
led_mode_t GetModeLedInternet();

/**
 * @brief Bật chế độ LED dành riêng cho config mode.
 *
 * Khi bật, LED service nhấp nháy để báo đang cấu hình và LED internet tắt.
 */
void EnterLedConfigMode();

/**
 * @brief API tương thích cho trạng thái xử lý cấu hình HC.
 *
 * Tốc độ LED config hiện được cố định ở 0,25 giây.
 */
void SetLedConfigBusy(bool busy);

/**
 * @brief Cập nhật trạng thái kết nối Wi-Fi trong config mode.
 *
 * Khi kết nối Wi-Fi thành công, LED internet sáng liên tục.
 */
void SetLedConfigWifiConnected(bool connected);

/**
 * @brief Cập nhật trạng thái kết nối server trong config mode.
 *
 * Khi kết nối server thành công, LED service/config sáng liên tục.
 */
void SetLedConfigServerConnected(bool connected);

/**
 * @brief Cập nhật trạng thái kết nối Internet/Wi-Fi.
 *
 * Có kết nối: LED Internet sáng. Mất kết nối: LED Internet nhấp nháy.
 */
void SetLedInternetConnected(bool connected);

/**
 * @brief Cập nhật trạng thái kết nối server.
 *
 * Có kết nối: LED Service sáng. Mất kết nối: LED Service nhấp nháy.
 */
void SetLedServerConnected(bool connected);

/**
 * @brief Thoát chế độ LED dành cho config mode.
 */
void ExitLedConfigMode();

/**
 * @brief Kiểm tra LED có đang bị override bởi config mode hay không.
 */
bool IsLedConfigMode();

/**
 * @brief Nháy LED Internet số lần cố định mà không làm mất trạng thái nền.
 */
void BlinkLedInternetTimes(uint8_t times, uint32_t onMs = 150,
                           uint32_t offMs = 150);

/**
 * @brief Nháy đồng thời cả 2 LED số lần cố định mà không làm mất trạng thái nền.
 */
void BlinkBothLedsTimes(uint8_t times, uint32_t onMs = 150,
                        uint32_t offMs = 150);
