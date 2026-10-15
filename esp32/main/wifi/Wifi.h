/**
 * @file Wifi.h
 * @brief Wi-Fi station connectivity used by the MQTT gateway.
 */

#pragma once

#include <stdint.h>
#include <iostream>
#include "json.h"

/**
 * @namespace Wifi
 * @brief Wi-Fi station connectivity helpers for MQTT.
 */
namespace Wifi
{
    void Init();

    /**
     * @brief Chờ kết nối Wi-Fi hoàn tất.
     *
     * @return true nếu kết nối thành công, false nếu timeout.
     */
    bool WaitConnecting();

    /** @brief Wait for a Wi-Fi station connection with an IP address. */
    bool WaitStaConnecting();

    /**
     * @brief Kiểm tra lần kết nối STA gần nhất thất bại do không tìm thấy AP.
     */
    bool WasLastSsidNotFound();

    /** @brief Check whether the Wi-Fi station currently has a valid IP. */
    bool IsNetworkConnected();

    /**
     * @brief Số phiên kết nối mạng, tăng khi interface có IP thay đổi.
     */
    uint32_t GetNetworkRevision();

    /**
     * @brief Lấy địa chỉ MAC hiện tại của thiết bị.
     *
     * @return Chuỗi dạng "A1B2C3D4E5F6".
     */
    std::string GetMacAddress();

    /**
     * @brief Lấy địa chỉ IP hiện tại của thiết bị.
     *
     * @return Chuỗi dạng "192.168.1.10".
     */
    std::string GetIP();

    bool SetStaCredentials(const std::string &ssid, const std::string &password);

    bool ConnectToWifi(const std::string &ssid, const std::string &pass);
    bool ScanWifi(Json::Value &networks);
    std::string GetSSID();

    /**
     * @brief Xóa cấu hình Wi-Fi đã lưu và dừng kết nối hiện tại.
     */
    bool ClearStoredCredentials();
}
