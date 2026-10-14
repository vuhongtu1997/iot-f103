/**
 * @file WifiProvision.h
 * @brief Khai báo các hàm phục vụ Wi-Fi provisioning (BLE hoặc SoftAP).
 *
 * Module này hỗ trợ cấu hình Wi-Fi cho thiết bị thông qua ứng dụng (app)
 * bằng giao thức BLE hoặc SoftAP. Người dùng có thể truyền SSID/password
 * để thiết bị kết nối vào mạng Wi-Fi thực tế.
 *
 * Một số thông số cấu hình như SSID mặc định, mật khẩu Wi-Fi, POP dùng cho provisioning
 * được định nghĩa ở phần macro.
 */

#pragma once

#include <stdint.h>
#include <iostream>
#include "json.h"

/**
 * @brief Prefix cho SSID chế độ AP.
 *
 * Khi dùng provisioning qua SoftAP, thiết bị phát Wi-Fi với SSID bắt đầu bằng "DONGA_".
 */
#define SSID_PREFIX "HC_MINI"

/**
 * @brief POP (Proof of Possession) dùng cho quá trình provisioning.
 *
 * Thiết bị cần POP để xác thực yêu cầu provisioning từ app.
 */
#define POP_PARAMS "donga2025@"

/**
 * @brief Mật khẩu Wi-Fi mặc định cho SoftAP (nếu cần).
 */
#define PASS_WIFI ""

/**
 * @brief Tự động reset provisioning manager khi lỗi quá nhiều lần.
 */
#define RD_RESET_PROV_MGR_ON_FAILURE 1

/**
 * @brief Số lần retry tối đa khi provisioning thất bại.
 */
#define PROV_MGR_MAX_RETRY_CNT 5

/**
 * @enum WifiProvisionTransportType
 * @brief Loại phương thức provisioning Wi-Fi.
 */
enum
{
    WIFI_PROVISION_TRANSPORT_BLE,   ///< Provisioning qua BLE (Bluetooth Low Energy)
    WIFI_PROVISION_TRANSPORT_SOFTAP ///< Provisioning qua SoftAP (thiết bị phát Wi-Fi)
};

/**
 * @namespace Wifi
 * @brief Không gian tên chứa các hàm quản lý provisioning và Wi-Fi cơ bản.
 */
namespace Wifi
{
    /**
     * @brief Khởi tạo driver Wi-Fi.
     *
     * Cần được gọi trước khi thực hiện provisioning hoặc kết nối Wi-Fi.
     */
    void Init(bool configMode = false);

    /**
     * @brief Chờ kết nối Wi-Fi hoàn tất.
     *
     * @return true nếu kết nối thành công, false nếu timeout.
     */
    bool WaitConnecting();

    /**
     * @brief Chờ riêng kết nối Wi-Fi STA có địa chỉ IP.
     *
     * Khác với WaitConnecting(), hàm này không coi Ethernet là kết nối Wi-Fi
     * thành công.
     */
    bool WaitStaConnecting();

    /**
     * @brief Kiểm tra lần kết nối STA gần nhất thất bại do không tìm thấy AP.
     */
    bool WasLastSsidNotFound();

    /**
     * @brief Kiểm tra LAN hoặc Wi-Fi STA hiện có địa chỉ IP hợp lệ.
     */
    bool IsNetworkConnected();

    /**
     * @brief Số phiên kết nối mạng, tăng khi interface có IP thay đổi.
     */
    uint32_t GetNetworkRevision();

    /**
     * @brief Bật chế độ Access Point để thực hiện provisioning qua SoftAP.
     */
    void WifiStartAP(void);

    /**
     * @brief Bật chế độ Station, thiết bị kết nối vào Wi-Fi thật.
     */
    void WifiStartSta(void);

    /**
     * @brief Tắt SoftAP cấu hình nhưng giữ kết nối STA hiện tại.
     *
     * Dùng sau khi HC đã kết nối server thành công để ngừng phát SSID
     * `HC_MINI_...` mà không làm gián đoạn kết nối Internet.
     */
    bool DisableConfigAccessPoint(void);

    /**
     * @brief Kiểm tra chế độ Wi-Fi hiện tại có phải AP-mode hay không.
     *
     * @return true nếu thiết bị đang ở chế độ AP.
     */
    bool WifiIsAPMode(void);

    /**
     * @brief Kiểm tra thiết bị có đang ở chế độ Station hay không.
     *
     * @return true nếu ở chế độ STA.
     */
    bool WifiIsStaMode(void);

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

    /**
     * @brief Ghi SSID/password mới cho STA vào driver Wi-Fi.
     *
     * Hàm này chỉ lưu cấu hình STA; việc áp dụng đầy đủ sẽ hoàn tất sau reboot
     * hoặc khi gọi lại WifiStartSta().
     */
    bool SetStaCredentials(const std::string &ssid, const std::string &password);

    /**
     * @brief Cấu hình và bắt đầu quá trình provisioning Wi-Fi.
     *
     * @param typeTransport Loại phương thức provisioning:
     *  - `WIFI_PROVISION_TRANSPORT_BLE`
     *  - `WIFI_PROVISION_TRANSPORT_SOFTAP`
     *
     * Nếu dùng BLE thì thiệt bị phải hỗ trợ BLE Mesh hoặc BLE GATT.
     * Nếu dùng SoftAP thì thiết bị sẽ phát Wi-Fi với SSID dạng `SSID_PREFIX + MAC`.
     */
    void WifiProvisionConfig(int typeTransport);

    /**
     * @brief Dừng provisioning manager.
     */
    void WifiProvisionStop();

    /**
     * @brief Kiểm tra quá trình provisioning có thành công hay không.
     *
     * @return true nếu thiết bị đã kết nối Wi-Fi với SSID/password do người dùng cung cấp.
     */
    bool IsSuccessWifiProvision();
    bool ConnectToWifi(const std::string &ssid, const std::string &pass);
    bool ScanWifi(Json::Value &networks);
    std::string GetSSID();

    /**
     * @brief Xóa cấu hình Wi-Fi đã lưu và dừng kết nối hiện tại.
     */
    bool ClearStoredCredentials();
}
