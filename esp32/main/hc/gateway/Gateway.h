/**
 * @file Gateway.h
 * @brief Lớp Gateway — điều phối chính cho gateway: xử lý MQTT (CloudProtocol),
 *        quản lý thiết bị, nhóm, phòng, cảnh, rule và tích hợp BLE/UDP/OTA.
 *
 * File này khai báo lớp `Gateway` kế thừa từ `CloudProtocol` (giao tiếp MQTT)
 * và đóng vai trò trung tâm điều khiển của hệ thống:
 *  - Đăng ký/điều khiển các topic MQTT liên quan device/group/room/scene/rule.
 *  - Cầu nối giữa cloud và Device/Group/Scene/Room/Rule manager.
 *  - Quản lý quá trình OTA (FirmwareUpdate, chunk, finish).
 *  - Quản lý key/netkey/appkey cho BLE mesh (nếu enable).
 *  - Cung cấp API để đọc/ghi thông tin gateway (id, mac, version, tokens...).
 *
 * Ghi chú thiết kế:
 *  - Sử dụng singleton (static getInstance()) để truy cập toàn cục.
 *  - Các hàm `InitMqttMessage*` dùng để đăng ký callback MQTT.
 *  - Phần BLE chỉ hoạt động khi cấu hình `CONFIG_ENABLE_BLE` bật.
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
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include "Udp.h"

#if 1 // Commented out unused features
#include "GroupManager.h"
#include "RoomManager.h"
#include "RuleManager.h"
#include "SceneManager.h"

#endif

#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#include "DeviceBle.h"

#endif

/**
 * @class Gateway
 * @brief Lớp trung tâm điều khiển gateway, mở rộng CloudProtocol cho logic
 * gateway.
 *
 * Gateway chịu trách nhiệm:
 *  - Xử lý callback MQTT (RPC/attributes/telemetry) đặc thù của gateway.
 *  - Quản lý Device/Group/Room/Scene/Rule thông qua các Manager tương ứng.
 *  - Cung cấp API firmware update (OTA), reset factory, restart BLE, đồng bộ dữ
 * liệu.
 *
 * @warning Một số chức năng phụ thuộc macro cấu hình (ví dụ:
 * CONFIG_ENABLE_BLE).
 */
class Gateway : public CloudProtocol, public Udp
{
private:
  /* ----- Thông tin gateway (lưu trong DB / NVS) ----- */
  std::string id;            /**< Id gateway (ví dụ: gateway id trên cloud) */
  std::string mac;           /**< MAC vật lý của gateway */
  std::string version;       /**< Phiên bản firmware / version chuỗi */
  std::string ble_netkey;    /**< BLE mesh network key (base64/hex) */
  std::string ble_appkey;    /**< BLE mesh app key */
  std::string ble_devicekey; /**< Khóa thiết bị BLE (device key) */
  uint16_t ble_addr;         /**< Địa chỉ unicast BLE của gateway */
  uint32_t ble_iv_index;     /**< IV index cho BLE mesh */
  std::string dormitoryId;   /**< Id khu/nhà/chi nhánh nếu có */
  std::string refresh_token; /**< Token refresh (nếu có) */
  std::string data;          /**< Chuỗi data gốc (metadata) */

  /* ----- Mutex cho thao tác đồng bộ khi xóa/modify danh sách ----- */
#if 1                     // Commented out unused features
  std::mutex mtxDelGroup; /**< Khóa khi xóa group */
  std::mutex mtxDelScene; /**< Khóa khi xóa scene */
  std::mutex mtxDelRoom;  /**< Khóa khi xóa room */
  std::mutex mtxDelRule;  /**< Khóa khi xóa rule */
#endif

  /* ----- Kiểm tra BLE ----- */
  // Thời điểm lần cuối gateway ping module BLE (dùng để health-check)
  time_t lastTimePingGwBle;
  std::mutex broadcastHealthMtx;
  bool broadcastHealthPending;
  time_t broadcastHealthStartTime;
  std::set<std::string> broadcastHealthRespondedIds;
  std::atomic<bool> processingSceneGroup;

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

#if 1 // Commented out unused features: Group, Room, Rule, Scene
  /* =========================
   *  Room related handlers
   * ========================= */
  void InitMqttMessageRoom();
  int DelDeviceFromAllRoom(std::string deviceId);
  int OnGetRoomList(Json::Value &reqValue, Json::Value &respValue,
                    std::string &topicRsp);
  int OnGetDevListInRoom(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnCreateRoom(Json::Value &reqValue, Json::Value &respValue,
                   std::string &topicRsp);
  int OnAddDeviceToRoom(Json::Value &reqValue, Json::Value &respValue,
                        std::string &topicRsp);
  int OnDeleteDeviceFromRoom(Json::Value &reqValue, Json::Value &respValue,
                             std::string &topicRsp);
  int OnDeleteRoom(Json::Value &reqValue, Json::Value &respValue,
                   std::string &topicRsp);
  int OnGetGroupIntoRoom(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnGetSceneIntoRoom(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnUpdateRoomName(Json::Value &reqValue, Json::Value &respValue,
                       std::string &topicRsp);
  int OnCheckRoom(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnRoomsSync(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);

  /* =========================
   *  Rule related handlers
   * ========================= */
  void InitMqttMessageRule();
  int OnCreateRule(Json::Value &reqValue, Json::Value &respValue);
  int OnEditRule(Json::Value &reqValue, Json::Value &respValue);
  int OnDeleteRule(Json::Value &reqValue, Json::Value &respValue);
  int OnActiveRule(Json::Value &reqValue, Json::Value &respValue);
  int OnActionRule(Json::Value &reqValue, Json::Value &respValue);

  // Group
	void InitMqttMessageGroup();
	int OnControlGroup(Json::Value &reqValue, Json::Value &respValue);
	int OnCreateGroup(Json::Value &reqValue, Json::Value &respValue);
	int OnAddDeviceToGroup(Json::Value &reqValue, Json::Value &respValue);
	int OnDeleteDeviceFromGroup(Json::Value &reqValue, Json::Value &respValue);
	int OnDeleteGroup(Json::Value &reqValue, Json::Value &respValue);
  int OnCreateGroupLink(Json::Value &reqValue, Json::Value &respValue);
  int OnDeleteGroupLink(Json::Value &reqValue, Json::Value &respValue);
  int OnAddDeviceToGroupLink(Json::Value &reqValue, Json::Value &respValue);
  int OnDeleteDeviceFromGroupLink(Json::Value &reqValue, Json::Value &respValue);

  /* =========================
   *  Scene related handlers
   * ========================= */
  void InitMqttMessageScene();
  int OnControlScene(Json::Value &reqValue, Json::Value &respValue,
                     std::string &topicRsp);
  int OnGetSceneList(Json::Value &reqValue, Json::Value &respValue,
                     std::string &topicRsp);
  int OnGetDevListInScene(Json::Value &reqValue, Json::Value &respValue,
                          std::string &topicRsp);
  int OnCreateScene(Json::Value &reqValue, Json::Value &respValue,
                    std::string &topicRsp);
  int OnEditScene(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnDeleteScene(Json::Value &reqValue, Json::Value &respValue,
                    std::string &topicRsp);
  int OnAddDevToScene(Json::Value &reqValue, Json::Value &respValue,
                      std::string &topicRsp);
  int OnDelDevToScene(Json::Value &reqValue, Json::Value &respValue,
                      std::string &topicRsp);
  int ThreadRspCallScene(Scene *scene, int timeout);
  int ConfigSceneForPirSensor(Device *device, Json::Value &data,
                              Json::Value &scene, bool isAddScene);
  int ConfigSceneForSeftPowerRemote(Device *device, Json::Value &data,
                                    Json::Value &scene, bool isAddScene);
  int OnCreateSceneController(Json::Value &reqValue, Json::Value &respValue,
                              std::string &topicRsp);
  int OnDelSceneController(Json::Value &reqValue, Json::Value &respValue,
                           std::string &topicRsp);
  int OnAddFavoriteScene(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnDelFavoriteScene(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnGetFavoriteScene(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnUpdateSceneName(Json::Value &reqValue, Json::Value &respValue,
                        std::string &topicRsp);
  int OnScenesSync(Json::Value &reqValue, Json::Value &respValue,
                   std::string &topicRsp);
#endif // Commented out unused features

  /* =========================
   *  HC / Scan / Provision handlers
   * ========================= */
  int OnRegisterHc(Json::Value &reqValue, Json::Value &respValue,
                   std::string &topicRsp);
  int OnControlHc(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnGetHcInfo(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnStartScan(Json::Value &reqValue, Json::Value &respValue);
  int OnStopScan(Json::Value &reqValue, Json::Value &respValue);
  int OnStartScanPairDev(Json::Value &reqValue, Json::Value &respValue,
                         std::string &topicRsp);
  int OnStopScanPairDev(Json::Value &reqValue, Json::Value &respValue,
                        std::string &topicRsp);
  int OnResetHC(Json::Value &reqValue, Json::Value &respValue,
                std::string &topicRsp);
  int OnVersionHC(Json::Value &reqValue, Json::Value &respValue,
                  std::string &topicRsp);
  int OnOtaHc(Json::Value &reqValue, Json::Value &respValue, std::string &topicRsp);

  /* =========================
   *  UDP handlers
   * ========================= */
  void InitUdpMessage();
  int OnUdpScanHc(Json::Value &reqValue, Json::Value &respValue);
  int OnUdpHcScanWifi(Json::Value &reqValue, Json::Value &respValue);
  int OnUdpHcSetup(Json::Value &reqValue, Json::Value &respValue);
  int OnUdpHcInfo(Json::Value &reqValue, Json::Value &respValue);
  int OnScanIpHc(Json::Value &reqValue, Json::Value &respValue);

public:
  /* ----- Danh sách control object (đang chờ phản hồi) ----- */
  std::map<std::string, int> listControlObj;
  std::mutex listControlObjMutex;

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
  void StartUdpService();
  void StopUdpService();
  void InitMqttMessageHc();

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

  /* ----- Factory / BLE / Status helpers ----- */
  /**
   * @brief Thiết lập lại về trạng thái factory (xóa pairing, config, db...).
   */
  void ResetFactory();

  /**
   * @brief Khởi động lại BLE gateway (nếu có).
   */
  int RestartBleGw();

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
  int AddDeviceToControlResponse(Device *device, Json::Value &dataValue);

  /**
   * @brief Push telemetry/attribute cập nhật device lên cloud.
   */
  int PushDeviceUpdateCloud(Json::Value &dataValue);

  int PushDeviceUpdateCloud(Device *device, Json::Value &telemetryValue);
  int PushDeviceOnlineState(Device *device, bool isOnline);
  void SetProcessingSceneGroup(bool processing);
  bool IsProcessingSceneGroup() const;
  void MarkBleBroadcastResponse(Device *device);
  bool IsIrBleDevice(Device *device);

  /** Publish a locally provisioned BLE node after it has been persisted. */
  int PublishProvisionedDevice(Device *device);

  /* ----- BLE ping/health helpers ----- */
  time_t GetLastTimePingGwBle();
  void SetLastTimePingGwBle(time_t timeUpdate);
  int PushGatewayStatus();
};
