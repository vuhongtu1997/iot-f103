/**
 * @file Gateway.h
 * @brief Lớp Gateway — điều phối chính cho gateway: xử lý MQTT (CloudProtocol),
 *        quản lý thiết bị Modbus qua RS485, MQTT và OTA.
 *
 * File này khai báo lớp `Gateway` kế thừa từ `CloudProtocol` (giao tiếp MQTT)
 * và đóng vai trò trung tâm điều khiển của hệ thống:
 *  - Đăng ký/điều khiển các topic MQTT liên quan đến thiết bị và rule.
 *  - Cầu nối giữa cloud, DeviceManager và RuleManager.
 *  - Quản lý quá trình OTA (FirmwareUpdate, chunk, finish).
 *  - Cung cấp API để đọc/ghi thông tin gateway (id, mac, version, tokens...).
 *
 * Ghi chú thiết kế:
 *  - Sử dụng singleton (static getInstance()) để truy cập toàn cục.
 *  - Các hàm `InitMqttMessage*` dùng để đăng ký callback MQTT.
 *
 * @note Đây là file header — không chứa hiện thực. Các thay đổi logic cần chỉnh
 *       trong file .cpp tương ứng.
 */

#pragma once

#include "CloudProtocol.h"
#include "Define.h"
#include "DeviceManager.h"
#include "ErrorCode.h"
#include "Ota.h"
#include "Util.h"
#include "json.h"
#include <functional>
#include <string>
#include <thread>
#include "ModbusBus.h"
#include "RuleManager.h"

/**
 * @class Gateway
 * @brief Lớp trung tâm điều khiển gateway, mở rộng CloudProtocol cho logic
 * gateway.
 *
 * Gateway chịu trách nhiệm:
 *  - Xử lý callback MQTT (RPC/attributes/telemetry) đặc thù của gateway.
 *  - Quản lý thiết bị Modbus và rule.
 *  - Cung cấp API firmware update (OTA), reset factory và đồng bộ dữ
 * liệu.
 *
 * @warning Một số chức năng phụ thuộc macro cấu hình (ví dụ:
 * Modbus RTU qua RS485).
 */
class Gateway : public CloudProtocol
{
private:
  /* ----- Thông tin gateway (lưu trong DB / NVS) ----- */
  std::string id;            /**< Id gateway (ví dụ: gateway id trên cloud) */
  std::string mac;           /**< MAC vật lý của gateway */
  std::string version;       /**< Phiên bản firmware / version chuỗi */
  // Kept only for compatibility with the legacy Gateway SQLite columns.
  std::string ble_netkey;
  std::string ble_appkey;
  std::string ble_devicekey;
  uint16_t ble_addr;
  uint32_t ble_iv_index;
  std::string dormitoryId;   /**< Id khu/nhà/chi nhánh nếu có */
  std::string refresh_token; /**< Token refresh (nếu có) */
  std::string data;          /**< Chuỗi data gốc (metadata) */

  /* ----- Constructor / Destructor (riêng tư để support singleton) ----- */
  /**
   * @brief Khởi tạo Gateway (riêng tư: dùng getInstance()).
   */
  Gateway(string mac, string address, int port, string clientId, string username, string password, int keepalive);
  ~Gateway();

  /* ----- Override callback khi cloud connect/disconnect ----- */
  /**
   * @brief Gọi khi MQTT (CloudProtocol) connect/disconnect.
   * @param isConnected true nếu đã kết nối thành công, false nếu không.
   * @param isReconnect true nếu đây là reconnect.
   *
   * @note Implement sẽ dùng để (re)subscribe, đồng bộ trạng thái ban đầu...
   */
  void OnCloudConnect(bool isConnected, bool isReconnect);

  /* =========================
   *  Device related handlers
   *  =========================
   *  Các hàm On* trả về CODE_OK / CODE_ERROR tuỳ kết quả và viết
   *  theo signature dùng để đăng ký làm OnRpcCallbackFunc.
   */
  /* Khởi tạo các message/device-related MQTT callback */
  void InitMqttMessageDevice();

  /* Xử lý điều khiển một device */
  int OnControlDeviceV3(Json::Value &reqValue, Json::Value &respValue);
  int OnControlAllDeviceV3(Json::Value &reqValue, Json::Value &respValue);

  /* =========================
   *  Rule related handlers
   * ========================= */
  void InitMqttMessageRule();
  int OnCreateRule(Json::Value &reqValue, Json::Value &respValue);
  int OnEditRule(Json::Value &reqValue, Json::Value &respValue);
  int OnDeleteRule(Json::Value &reqValue, Json::Value &respValue);
  int OnActiveRule(Json::Value &reqValue, Json::Value &respValue);
  int OnActionRule(Json::Value &reqValue, Json::Value &respValue);

  /* =========================
   *  HC / Scan / Provision handlers
   * ========================= */
  int OnRegisterHc(Json::Value &reqValue, Json::Value &respValue,
                   std::string &topicRsp);
  int OnControlHc(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnGetHcInfo(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnResetHC(Json::Value &reqValue, Json::Value &respValue,
                std::string &topicRsp);
  int OnVersionHC(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnOtaHc(Json::Value &reqValue, Json::Value &respValue, std::string &topicRsp);
  int OnModbusDiscover(Json::Value &reqValue, Json::Value &respValue);
  int OnModbusAssignAddress(Json::Value &reqValue, Json::Value &respValue);

public:
  /**
   * @brief Lấy instance singleton của Gateway.
   * @return con trỏ tới Gateway duy nhất.
   */
  static Gateway *getInstance();

  /**
   * @brief Khởi tạo gateway: khởi tạo CloudProtocol, đăng ký callback, init các
   * manager.
   */
  void Init();
  void InitMqttMessageRs485();

  /* ----- Firmware / OTA overrides ----- */
  /**
   * @brief Kiểm tra xem firmware mới có cần cập nhật không (override).
   */
  // bool FirmwareCheckNeedUpdate(std::string fwVersion) override;

  // /**
  //  * @brief Cập nhật firmware bằng URL (override).
  //  */
  // int FirmwareUpdate(std::string fwUrl, std::string fwChecksumAlgorithm,
  //                    std::string fwChecksum) override;

  // /**
  //  * @brief Chuẩn bị cập nhật bằng phân đoạn theo kích thước tổng (override).
  //  */
  // int FirmwareUpdate(int fwSize, std::string fwChecksumAlgorithm,
  //                    std::string fwChecksum) override;

  // /**
  //  * @brief Ghi một chunk firmware (override).
  //  */
  // int FirmwareUpdateChunk(int chunkId, uint8_t *data, int dataLen) override;

  // /**
  //  * @brief Hoàn tất quá trình cập nhật firmware (override).
  //  */
  // int FirmwareUpdateFinish() override;

  /* ----- Factory / Status helpers ----- */
  /**
   * @brief Thiết lập lại về trạng thái factory (xóa pairing, config, db...).
   */
  void ResetFactory();

  /**
   * @brief Kiểm tra online thread (helper để giám sát các module).
   */
  int CheckOnlineThread();

  /**
   * @brief Gửi trạng thái thiết bị (push) tới cloud (khi cần).
   */
  void SendStatusDev();

  /* ----- Getters (BLE / gateway info) ----- */
  uint16_t GetBleAddr();
  uint32_t GetBleIvIndex();
  std::string GetBleNetKey();
  std::string GetBleAppKey();
  std::string GetBleDeviceKey();
  std::string GetDormitory();
  std::string GetId();
  std::string GetVersion();
  std::string GetName();
  std::string GetRefreshToken();
  std::string GetData();
  std::string GetMac();

  /* ----- Setters (BLE / gateway info) ----- */
  void SetBleAddr(uint16_t addr);
  void SetBleIvIndex(uint32_t ivIndex);
  void SetBleNetkey(std::string netkey);
  void SetBleAppkey(std::string appkey);
  void SetBleDevicekey(std::string devicekey);
  void SetDormitory(std::string dormitory);
  void SetId(std::string id);
  void SetMac(std::string mac);
  void SetVersion(std::string version);
  void SetName(std::string name);
  void SetRefreshToken(std::string refresh_token);
  void SetData(std::string data);

  Rule *AddRule(Json::Value &ruleValue, bool addDatabase);
  /* ----- Helpers publish / control ----- */
  /**
   * @brief Xử lý JSON request chung (do cloud gửi) — route đến handlers tương
   * ứng.
   */
  int Do(Json::Value &dataValue);

  /**
   * @brief Thêm device vào response control (gửi trả cloud) — helper tạo
   * payload.
   */
  /**
   * @brief Push telemetry/attribute cập nhật device lên cloud.
   */
  int PushDeviceUpdateCloud(Json::Value &dataValue);

  int PushDeviceUpdateCloud(Device *device, Json::Value &telemetryValue);
  int PushDeviceOnlineState(Device *device, bool isOnline);
  int PushGatewayStatus();
  int PublishModbusTelemetry(const ModbusNodeInfo &node,
                             const uint16_t registers[12], bool online);
};
