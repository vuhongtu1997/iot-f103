/**
 * @file BleProtocol.h
 * @brief Định nghĩa các hằng số, cấu trúc dữ liệu và lớp `BleProtocol` dùng để
 * xử lý giao tiếp BLE Mesh qua UART.
 *
 * @details
 * Lớp `BleProtocol` đóng vai trò trung gian giữa Gateway và thiết bị BLE Mesh.
 * Nó thực hiện gửi/nhận gói tin UART, phân tích opcode, xử lý provisioning,
 * binding, điều khiển nhóm, cảnh, sensor,...
 *
 * Thành phần của file:
 * - Các định nghĩa hằng số (opcode, header, mã lệnh BLE...).
 * - Các cấu trúc dữ liệu đóng gói gói tin BLE (`scan_device_message_t`,
 * `message_rsp_st`,...).
 * - Lớp `BleProtocol` kế thừa từ `Uart`, quản lý gửi/nhận lệnh BLE và phản hồi.
 *
 * @note
 * File này có thể được build trên nhiều nền tảng (OpenWRT, ESP32) — tuỳ thuộc
 * macro `ESP_PLATFORM`.
 */

#pragma once

#include "Uart.h"
#include <atomic>
#include <string>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

namespace Json { class Value; }

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#endif

// ==========================
// BLE Protocol Definitions
// ==========================

#define BLE_GROUP_OFFSET 0xC000
#define SYSTEM_REQ 0xFFE9
#define APP_REQ 0xFFE8
#define RAL_MAGIC 0x0428
#define RD_VENDOR_ID 0x0211

// ---------- Opcode Definitions ----------
#define RD_OPCODE_PROVISION 0xF0
#define RD_OPCODE_PROVISION_RSP 0xF1
#define RD_OPCODE_CONFIG 0xF2
#define RD_OPCODE_CONFIG_RSP 0xF3
#define RD_OPCODE_SENSOR_RSP 0x52
#define HG_OPCODE_CONFIG 0xE0
#define HG_OPCODE_CONFIG_RSP 0xE1

// (Các RD_HEADER_* định nghĩa chi tiết header lệnh BLE)
#define RD_HEADER_PROVISION_SET_GW_ADDR 0x0001
#define RD_HEADER_PROVISION_GET_DEV_TYPE 0x0002

#define RD_HEADER_LIGHT_SENSOR_MODULE_TYPE 0x0003
#define RD_HEADER_HG_HEADER_0005 0x0004
#define RD_HEADER_RSP_LIGHT_SENSOR 0x0103

#define RD_HEADER_CONFIG_SET_SCENE_PIR_LIGHT_SENSOR 0x0134
#define RD_HEADER_CONFIG_DEL_SCENE_PIR_LIGHT_SENSOR 0x0234
#define RD_HEADER_CONFIG_SET_TIME_ACTION_PIR_LIGHT_SENSOR 0x0334
#define RD_HEADER_CONFIG_SET_MODE_ACTION_PIR_LIGHT_SENSOR 0x0434
#define RD_HEADER_CONFIG_SET_SENSI_PIR_LIGHT_SENSOR 0x0534
#define RD_HEADER_RSP_PIR_LIGHT_SENSOR_STARTUP 0x0634
#define RD_HEADER_CONFIG_SET_DISTANCE_RADA_SENSOR 0x0734
#define RD_HEADER_CONFIG_TIME_RSP_SENSOR 0x0834

#define RD_HEADER_CONFIG_CONTROL_RELAY_SWITCH_4 0x000A
#define RD_HEADER_CONFIG_STATUS_STARTUP_SWITCH 0x100A
#define RD_HEADER_CONFIG_STATUS_ALL_RELAY_SWITCH 0x110A

#define RD_HEADER_CONFIG_CONTROL_RGB_SWITCH 0x050A
#define RD_HEADER_CONFIG_SET_ID_COMBINE 0x060A
#define RD_HEADER_CONFIG_SET_TIMER 0x070A
#define RD_HEADER_REQUEST_STATUS_SWITCH 0x090A
#define HG_HEADER_000C 0x00C0

#define RD_HEADER_SEFTPOWER_REMOTE_SCAN 0x0A0A
#define RD_HEADER_SEFTPOWER_REMOTE_SAVE 0x0D0A
#define RD_HEADER_SEFTPOWER_REMOTE_RESET 0x0E0A
#define RD_HEADER_SEFTPOWER_REMOTE_PRESS 0x0B0A
#define RD_HEADER_SEFTPOWER_REMOTE_SET_SCENE 0x0C0A
#define RD_HEADER_SEFTPOWER_REMOTE_DEL_SCENE 0x0F0A

#define TRANSITION_DEFAULT 5

// ---------- Enum cho trạng thái thiết bị ----------
enum
{
  CLOSE,
  OPEN,
  PAUSE,
  PERCENT,
};

// ---------- Enum cho lệnh Gateway BLE ----------
#include "BleOpCode.h"

// ---------- Enum cho lệnh Gateway BLE ----------
enum
{
  // send cmd part
  // HCI_GATEWAY_CMD_START = 0x00,
  // HCI_GATEWAY_CMD_STOP = 0x01,
  // HCI_GATEWAY_CMD_RESET = 0x02,
  // HCI_GATEWAY_CMD_CLEAR_NODE_INFO = 0x06,
  // HCI_GATEWAY_CMD_SET_ADV_FILTER = 0x08,
  // HCI_GATEWAY_CMD_SET_PRO_PARA = 0x09,
  // HCI_GATEWAY_CMD_SET_NODE_PARA = 0x0a,
  // HCI_GATEWAY_CMD_START_KEYBIND = 0x0b,
  // HCI_GATEWAY_CMD_GET_PRO_SELF_STS = 0x0c,
  // HCI_GATEWAY_CMD_SET_DEV_KEY = 0x0d,
  // HCI_GATEWAY_CMD_GET_SNO = 0x0e,
  // HCI_GATEWAY_CMD_SET_SNO = 0x0f,
  // HCI_GATEWAY_CMD_GET_UUID_MAC = 0x10,
  // HCI_GATEWAY_CMD_DEL_VC_NODE_INFO = 0x11,
  // HCI_GATEWAY_CMD_SEND_VC_NODE_INFO = 0x12,

  // rsp cmd part
  // HCI_GATEWAY_RSP_UNICAST = 0x80,
  // HCI_GATEWAY_RSP_OP_CODE = 0X81,
  // HCI_GATEWAY_KEY_BIND_RSP = 0x82,
  // HCI_GATEWAY_CMD_STATIC_OOB_RSP = 0x87, // HCI send back the static oob
  // information HCI_GATEWAY_CMD_UPDATE_MAC = 0x88,
  // HCI_GATEWAY_CMD_PROVISION_EVT = 0x89,
  // HCI_GATEWAY_CMD_KEY_BIND_EVT = 0x8a,
  // HCI_GATEWAY_CMD_PRO_STS_RSP = 0x8b,
  // HCI_GATEWAY_CMD_SEND_ELE_CNT = 0x8c,
  // HCI_GATEWAY_CMD_SEND_NODE_INFO = 0x8d,
  HCI_GATEWAY_CMD_SEND_CPS_INFO = 0x8e,
  HCI_GATEWAY_CMD_HEARTBEAT = 0x8f,
  HCI_GATEWAY_CMD_SEND_MESH_OTA_STS = 0x98,
  HCI_GATEWAY_CMD_SEND_UUID = 0x99,
  HCI_GATEWAY_CMD_SEND_IVI = 0x9a,
  HCI_GATEWAY_CMD_SEND_SRC_CMD = 0x9c,
  HCI_GATEWAY_CMD_SEND_SNO_RSP = 0xa0,
  HCI_GATEWAY_CMD_SEND = 0xb1,
  HCI_GATEWAY_DEV_RSP = 0xb2,
  HCI_GATEWAY_CMD_LINK_OPEN = 0xb3,
  HCI_GATEWAY_CMD_LINK_CLS = 0xb4,
  HCI_GATEWAY_CMD_SEND_BACK_VC = 0xb5,
  HCI_GATEWAY_CMD_LOG_BUF = 0xb7,
  HCI_GATEWAY_CMD_SET_NODE_REPONS_LIVE = 0xEC,
};

#define CONNECT_DEVICE_TIMEOUT 40 ///< Timeout khi kết nối thiết bị BLE (giây)
#define BLE_MAX_ELEMENT 4         ///< Số element BLE tối đa trong 1 thiết bị

// ==========================
// Struct định nghĩa gói tin BLE
// ==========================

/**
 * @struct scan_device_message_t
 * @brief Dữ liệu quét BLE trả về từ thiết bị.
 */
typedef struct __attribute__((packed))
{
  uint8_t mac[6];
  uint8_t len;
  uint8_t header_type;
  uint8_t beacon_type;
  uint8_t uuid[16];
  uint32_t uri_hash;
  uint16_t obb_info;
  int8_t rssi;
  uint16_t dc;
} scan_device_message_t;

/**
 * @struct scan_device_pair_message_t
 * @brief Dữ liệu quét cặp thiết bị trong quá trình provisioning.
 */
typedef struct __attribute__((packed))
{
  uint16_t parentAddr;
  uint16_t gwAddr;
  uint8_t opcodeRsp;
  uint16_t vendorId;
  uint16_t header;
  uint8_t mac[4];
  uint8_t type;
  uint8_t rssi;
} scan_device_pair_message_t;

/**
 * @struct message_rsp_st
 * @brief Cấu trúc phản hồi tin nhắn BLE.
 */
typedef struct __attribute__((packed))
{
  uint16_t len;
  uint8_t magic;
  uint8_t opcode;
  uint8_t data[];
} message_rsp_st;

// ==========================
// Lớp chính xử lý giao tiếp BLE
// ==========================

/**
 * @class BleProtocol
 * @brief Lớp xử lý toàn bộ logic giao tiếp BLE Mesh qua UART.
 *
 * @details
 * - Kế thừa từ lớp `Uart`.
 * - Quản lý các yêu cầu gửi/nhận dữ liệu BLE.
 * - Hỗ trợ quét, provisioning, binding, điều khiển thiết bị, nhóm, cảnh,...
 * - Đảm bảo thread-safe thông qua nhiều mutex bảo vệ danh sách phản hồi.
 */
class BleProtocol : public Uart
{
private:
  // ---------------------- Internal Structs ----------------------
  typedef struct __attribute__((packed))
  {
    uint16_t opcode;
    uint8_t data[100];
  } message_req_st;

  typedef struct __attribute__((packed))
  {
    bool status;
    uint8_t opcode;
    int *len;
    uint8_t *data;
    uint8_t *compare_data;
    int compare_position;
    int compare_len;
  } message_rsp_list_st;

  typedef struct __attribute__((packed))
  {
    uint8_t uuid[8];
    uint8_t deviceType[4];
    uint16_t fwVersion;
    uint16_t magic;
  } uuid_t;

  typedef struct __attribute__((packed))
  {
    uint8_t netKey[16];
    uint16_t key_index;
    uint8_t flag;
    uint32_t iv_index;
    uint16_t unicast_address;
  } pro_net_info_t;

  typedef struct __attribute__((packed))
  {
    uint16_t nkIdx;
    uint16_t akIdx;
    uint8_t retryCnt;
    uint8_t rspMax;
    uint16_t devAddr;
  } ble_message_header_t;

  // ---------------------- Private Members ----------------------
  std::vector<message_rsp_list_st *>
      messageRespList;           ///< Danh sách phản hồi BLE đang chờ
  std::mutex mtxWaitSendUart;    ///< Mutex bảo vệ gửi UART
  std::mutex messageRespListMtx; ///< Mutex bảo vệ danh sách phản hồi
  std::mutex mtxOnOffCommand;    ///< Chỉ cho phép một lệnh OnOff ACK đang chờ
  std::atomic<uint32_t> onOffStatusSequence{0};
  std::atomic<uint16_t> lastOnOffStatusAddr{0};
  std::atomic<uint8_t> lastOnOffStatusValue{0xFF};

#define BLE_CHECK_OPCODE_BUFFER_MAX_SIZE 500
  std::mutex vectorCheckOpcodeMtx;
  std::vector<message_rsp_st *> messageCheckOpcodeList;

  // TODO: Add init state
  pro_net_info_t pro_net_info;
  uint8_t netKey[16];
  uint8_t appKey[16];
  uint8_t gwKey[16];
  uint8_t deviceKey[16];
  uint16_t nextAddr;
  bool chipHasValidKeys = false;

#ifdef ESP_PLATFORM
  BleProtocol(uart_port_t num, int txPin, int rxPin, int baudrate);
#else
  BleProtocol(char *uartPort, int uartBaudrate);
#endif
  virtual ~BleProtocol();

  int OnMessage(unsigned char *data, int len);
  int SendMessage(uint16_t opReq, uint8_t *dataReq, int lenReq, uint8_t opRsp,
                  uint8_t *dataRsp, int *lenRsp, uint32_t timeout,
                  uint8_t *compare_data = 0, int compare_position = 0,
                  int compare_len = 0);
  int SendCurtainConfig(uint16_t devAddr, uint8_t command, uint8_t value);

public:
  std::atomic<bool> haveNewMac;
  std::atomic<bool> haveGetMacRsp;
  std::atomic<bool> isProvisioning;
  std::atomic<bool> isInitKey;
  scan_device_message_t scanDeviceMessage;
  scan_device_pair_message_t scanDevicePairMessage;
  uint8_t tid;                   ///< Transaction ID for mesh commands
  std::atomic<bool> isCongested; ///< Flag to pause polling during critical operations

  static BleProtocol *getInstance();

  void Init();

  int GetOpcodeExceptionMessage(message_rsp_st **data);
  void CheckOpcodeException(message_rsp_st *message);

  void InitKey();
  bool HasProvisionedMeshConfig();
  bool NeedsMeshRecovery();
  int ConfigureProvisionedMesh(const Json::Value &config);
  void CheckKeyBle();
  int GetNetKey();
  int SetNodeResponsLive();

  // SeftPower Remote
  int ScanStopSeftPowerRemote(uint16_t devAddr, uint8_t status);
  int SaveSeftPowerRemote(scan_device_pair_message_t scanMessage,
                          uint16_t childDev);
  int SetSceneSeftPowerRemote(uint16_t devAddr, uint16_t seftPowerAddr,
                              uint8_t button, uint8_t mode, uint16_t scene);
  int DelSceneSeftPowerRemote(uint16_t devAddr, uint16_t seftPowerAddr,
                              uint8_t button, uint8_t mode);
  int ResetSeftPowerRemote(uint16_t devAddr, uint16_t seftPowerAddr);

  int StartScan();
  int StopScan();
  int ResetBle();
  int ResetFactory();

  bool IsProvision();
  void SetProvisioning(bool isProvision);
  int AddDevice(scan_device_message_t *scan_device_message);
  int AddPairDevice(uint32_t parentAddr, uint32_t childAddr);
  int SelectMac(uint8_t *mac);
  int Provision(uint16_t deviceAddr);
  int BindingAll();
  int SetGwAddr(uint16_t devAddr, uint16_t gwAddrSet, uint16_t indexDevice);
  int GetDeviceType(uint8_t *mac, uint16_t devAddr, uint32_t &deviceType, uint16_t &deviceVersion);

  int ResetDev(uint16_t devAddr);
  int ResetDelAll();

  int SendOnlineCheck(uint16_t devAddr, uint32_t typeDev, uint16_t version);
  int GetDevicesOnOffBroadcast();
  int GetTTL(uint16_t devAddr);

  int SetOnOffLight(uint16_t devAddr, uint8_t onoff, uint16_t transition,
                    bool ack);
  int ControlCurtain(uint16_t devAddr, uint8_t openClose, uint8_t position);
  int ControlLegacyCurtain(uint16_t devAddr, uint8_t openClose);
  int SetCurtainTime(uint16_t devAddr, uint16_t seconds);
  int SetCurtainTravelTime(uint16_t devAddr, uint8_t seconds);
  int SetCurtainType(uint16_t devAddr, uint8_t curtainType);
  int SetCurtainDirection(uint16_t devAddr, uint8_t direction);
  int GetOnOffLight(uint16_t devAddr);
  int SetDimmingLight(uint16_t devAddr, uint16_t dim, uint16_t transition,
                      bool ack);
  int SetCctLight(uint16_t devAddr, uint16_t cct, uint16_t transition,
                  bool ack);
  int SetHSLLight(uint16_t devAddr, uint16_t H, uint16_t S, uint16_t L,
                  uint16_t transition, bool ack);
  int SetCctDimLight(uint16_t devAddr, uint16_t cct, uint16_t dim,
                     uint16_t transition, bool ack);
  int SetGroupDimmingLight(uint16_t groupAddr, uint16_t dim);
  int SetGroupCctLight(uint16_t groupAddr, uint16_t cct);
  int SetGroupCctDimLight(uint16_t groupAddr, uint16_t dim, uint16_t cct);
  int SetGroupHslLight(uint16_t groupAddr, const std::string &hsl);
  int SetGroupRgbMode(uint16_t groupAddr, uint8_t modeRgb);
  int SetLevelDim(uint16_t devAddr, uint8_t dimMax, uint8_t dimMin);
  int SetupDimLevels(uint16_t devAddr, const uint8_t levels[5]);

  // group light
  int AddDev2Group(uint16_t devAddr, uint16_t element, uint16_t group);
  int DelDev2Group(uint16_t devAddr, uint16_t element, uint16_t group);

  // Scene light
  int SetScene(uint16_t devAddr, uint16_t scene, uint8_t modeRgb);
  int DelScene(uint16_t devAddr, uint16_t scene);
  int CallScene(uint16_t devAddr, uint16_t scene, uint16_t transition,
                bool ack);
  int CallModeRgb(uint16_t devAddr, uint8_t modeRgb);

  // update status lights
  int UpdateLights(uint16_t devAddr);
  int UpdateStatusSensorsPm(uint16_t devAddr);

  // PirLightSensor
  int SetScenePirLightSensor(uint16_t devAddr, uint8_t condition, uint8_t pir,
                             uint16_t lowLux, uint16_t highLux, uint16_t scene,
                             uint8_t type);
  int DelScenePirLightSensor(uint16_t devAddr, uint16_t scene);
  int TimeActionPirLightSensor(uint16_t devAddr, uint16_t time);
  int SetModeActionPirLightSensor(uint16_t devAddr, uint8_t mode);
  int SetSensiPirLightSensor(uint16_t devAddr, uint8_t sensi);
  int SetDistanceSensor(uint16_t devAddr, uint8_t distance);
  int SetTimeRspSensor(uint16_t devAddr, uint16_t time);

  int ControlIR(uint16_t devAddr, int commandType, int brandId, int remoteId,
                int temp, int mode, int fan, int swing);
  int ControlIRCmd(const std::string &command);
  int LearnIRV3Code(uint16_t devAddr, uint8_t department, uint16_t code);
  int DeleteIRV3Code(uint16_t devAddr, uint16_t id, uint8_t type);
  int GetIRV3Codes(uint16_t devAddr);

  // switch
  int ControlRgbSwitch(uint16_t devAddr, uint8_t button, uint8_t b, uint8_t g,
                       uint8_t r, uint8_t dimOn, uint8_t dimOff);
  int ControlRelayOfSwitch(uint16_t devAddr, uint32_t type, uint8_t relay,
                           uint8_t value);
  int SetIdCombine(uint16_t devAddr, uint16_t id);
  int CountDownSwitch(uint16_t devAddr, uint16_t timer, uint8_t status);
  int UpdateStatusRelaySwitch(uint16_t devAddr, uint32_t type = 0);
  int ConfigStatusStartupSwitch(uint16_t devAddr, uint8_t status,
                                uint32_t type = 0);
  int UpdateTypeSceneSwitch(uint16_t devAddr, uint8_t regType);

  // Backup
  int GetInfogw();
  int GetInfoMesh();
  int UpdateDeviceKeyDev(uint16_t devAddr, uint16_t elementCount, std::string devKeyDev);
  int UpdateDeviceKeyGateway(uint16_t gwAddr, std::string devKeyDev);
  int UpdateNetKey(uint16_t gwAddr, std::string netKey, uint32_t ivIndex = 0, uint16_t netKeyIdx = 0);
  int UpdateDevKey(uint16_t gwAddr, std::string devKey);
  int UpdateAppKey(std::string appKey, uint16_t appKeyIdx);
  int UpdateMaxAddr(uint16_t addr);
  int EnableReporting();
  int ResetGateway();
  int SetNodeParams();

  // Set device action scene
  int SetConditionDeviceScene(int mode, uint16_t devAddr, uint16_t sceneId, std::string type, uint8_t data1, uint8_t data2);
  int SetDeviceActionScene(int mode, uint16_t devAddr, uint16_t sceneId, std::string type, uint8_t data1, uint8_t data2);
  int SetIrDeviceActionScene(int mode, uint16_t devAddr, uint16_t sceneId,
                             int commandType, int brandId, int remoteId,
                             int temp, int irMode, int fan, int swing);
  int SetLocalSceneState(uint16_t sceneId, bool enabled);

  int SendRawData(uint8_t *data, int len);

  int SetLockKids(uint16_t devAddr, bool lock);
  int SetLockDevice(uint16_t devAddr, bool lock);
};
