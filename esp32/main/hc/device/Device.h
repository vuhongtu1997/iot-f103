/**
 * @file Device.h
 * @brief Định nghĩa lớp cơ sở Device cho tất cả thiết bị BLE/Wi-Fi trong hệ
 * thống.
 *
 * File này chứa:
 * - Danh sách key thuộc tính của thiết bị (attribute keys)
 * - Các mã định danh thiết bị BLE (device type)
 * - Các nhóm thiết bị (group type)
 * - Định nghĩa lớp Device: quản lý thông tin, trạng thái, và xử lý dữ liệu của
 * thiết bị
 */

#pragma once

#include <string>
#include <vector>
#ifndef bswap_16
#define bswap_16(x) __builtin_bswap16(x)
#endif
#ifndef bswap_32
#define bswap_32(x) __builtin_bswap32(x)
#endif
#include "ErrorCode.h"
#include "Object.h"
#if 1 // Rule feature disabled
#include "RuleInputDevice.h"
#endif
#include "json.h"

/* ========================================================================== */
/*                             ATTRIBUTE KEYS                                 */
/* ========================================================================== */
/**
 * @brief Các định nghĩa key thuộc tính (attribute keys) dùng trong JSON giữa
 * gateway và cloud.
 */
#define KEY_ATTRIBUTE_ONOFF "onoff"
#define KEY_ATTRIBUTE_DOOR_DETECT "door_sensor"
#define KEY_ATTRIBUTE_DOOR_BATTERY "bt"
#define KEY_ATTRIBUTE_DIM "dim"
#define KEY_ATTRIBUTE_CCT "cct"
#define KEY_ATTRIBUTE_HUE "h"
#define KEY_ATTRIBUTE_SATURATION "s"
#define KEY_ATTRIBUTE_LUMINANCE "l"
#define KEY_ATTRIBUTE_LUX "lux"
#define KEY_ATTRIBUTE_PIR "pir"
#define KEY_ATTRIBUTE_SMOKE "smoke_alarm"
#define KEY_ATTRIBUTE_PRESENCE_ACTIVE "1"
#define KEY_ATTRIBUTE_PRESENCE_TYPE "2"
#define KEY_ATTRIBUTE_PRESENCE_LUX "3"
#define KEY_ATTRIBUTE_DOOR_ALARM "2"
#define KEY_ATTRIBUTE_BUTTON "bt"
#define KEY_ATTRIBUTE_MODE_PRESS "modePress"
#define KEY_ATTRIBUTE_ACTIME "actime"
#define KEY_ATTRIBUTE_ACMODE "mode"
#define KEY_ATTRIBUTE_SENSI "sensi"
#define KEY_ATTRIBUTE_MODE_RGB "sceneRGB"
#define KEY_ATTRIBUTE_COUNTDOWN "countdown"
#define KEY_ATTRIBUTE_COUNTDOWN_TIMER "countDownTimer"
#define KEY_ATTRIBUTE_SETUP_DIMS "setup_dims"
#define KEY_ATTRIBUTE_CURTAINS_TIME "curtainsTime"
#define KEY_ATTRIBUTE_ONLINE_OFFLINE "stt"
#define KEY_ATTRIBUTE_R "r"
#define KEY_ATTRIBUTE_G "g"
#define KEY_ATTRIBUTE_B "b"
#define KEY_ATTRIBUTE_DIM_ON "dimOn"
#define KEY_ATTRIBUTE_DIM_OFF "dimOff"
#define KEY_ATTRIBUTE_DISTANCE "distance"
#define KEY_ATTRIBUTE_STATUS_STARTUP "statusStartup"

/**
 * @brief Giao thức thiết bị.
 */
typedef enum
{
  OTHER_DEVICE = -1, /**< Thiết bị không xác định */
  BLE_DEVICE = 2     /**< Thiết bị giao tiếp BLE */
} protocol_e;

/**
 * @brief Mã định danh loại thiết bị BLE.
 *
 * Dùng để nhận diện model BLE cụ thể (đèn, công tắc, cảm biến,...)
 */
enum
{
  BLE_DOWNLIGHT_SMT = 0x00010201,
  BLE_DOWNLIGHT_RGBCW = 0x00010403,

  BLE_SWITCH_RGB_1_V2 = 0x0002021A,
  BLE_SWITCH_RGB_2_V2 = 0x0002021C,
  BLE_SWITCH_RGB_3_V2 = 0x0002021E,
  BLE_SWITCH_RGB_4_V2 = 0x00020220,

  BLE_SEFTPOWER_REMOTE_1 = 0x00020701,
  BLE_SEFTPOWER_REMOTE_2 = 0x00020702,
  BLE_SEFTPOWER_REMOTE_3 = 0x00020703,

  BLE_RADA_LIGHT_SENSOR_AC_CB15 = 0x00030208,

  DEVICE_UNKNOWN_TYPE = 0xFFFFFFFF
};

#define TUYA_DEVICE "WIFI"
#define HG_BLE "BLEHGAA0101,BLEHGAA0102,BLEHGAA0103,BLEHGAA0104,BLEHGAA0108,BLEHGAA0601,BLEHGAA0602,BLEHGAA0603,BLEHGAA010A,BLEHGAA010B,BLEHGAA010C,BLEHGAA010D,BLEHGAA0701,BLEHGAA0702,BLEHGAA0703,BLEHGAA0704,BLEHGAA0105,BLEHGAA0106,BLEHGAA0107,BLEHGAA0201,BLEHGAA0401,BLEHGAA0111,BLEHGAA0112,BHGRDNS0110,BHGRDMS0113,BHGMCS0112,BHGMCS0111,BHGCS0114,BHGMDC0115,BLEHGAA0801,BLEHGAA0802,BLEHGAA0803,BLEHGAA0804,BLEHGAA0901,BLEHGAA0902,BLEHGAA0903,BLEHGAA0904,BLEHGAA010E,BHGMCS0116,BHGMCS0117,BHGRDNS0118,BHGRDMS8000,BLEHGAA0406"

#define HG_BLE_SWITCH "BLEHGAA0101,BLEHGAA0102,BLEHGAA0103,BLEHGAA0104,BLEHGAA0601,BLEHGAA0602,BLEHGAA0603,BLEHGAA0701,BLEHGAA0702,BLEHGAA0703,BLEHGAA0704,BLEHGAA0801,BLEHGAA0802,BLEHGAA0803,BLEHGAA0804,BLEHGAA0901,BLEHGAA0902,BLEHGAA0903,BLEHGAA0904"
#define HG_BLE_SWITCH_1 "BLEHGAA0101"
#define HG_BLE_SWITCH_2 "BLEHGAA0102"
#define HG_BLE_SWITCH_3 "BLEHGAA0103"
#define HG_BLE_SWITCH_4 "BLEHGAA0104"
#define HG_BLE_MODULE_SWITCH_1 "BLEHGAA0601"
#define HG_BLE_MODULE_SWITCH_2 "BLEHGAA0602"
#define HG_BLE_MODULE_SWITCH_3 "BLEHGAA0603"
#define HG_BLE_MECHANICAL_SWITCH_1 "BLEHGAA0701"
#define HG_BLE_MECHANICAL_SWITCH_2 "BLEHGAA0702"
#define HG_BLE_MECHANICAL_SWITCH_3 "BLEHGAA0703"
#define HG_BLE_MECHANICAL_SWITCH_4 "BLEHGAA0704"

#define HG_BLE_VIP_MECHANICAL_SWITCH_1 "BLEHGAA0801"
#define HG_BLE_VIP_MECHANICAL_SWITCH_2 "BLEHGAA0802"
#define HG_BLE_VIP_MECHANICAL_SWITCH_3 "BLEHGAA0803"
#define HG_BLE_VIP_MECHANICAL_SWITCH_4 "BLEHGAA0804"

#define HG_BLE_LUXURY_MECHANICAL_SWITCH_1 "BLEHGAA0901"
#define HG_BLE_LUXURY_MECHANICAL_SWITCH_2 "BLEHGAA0902"
#define HG_BLE_LUXURY_MECHANICAL_SWITCH_3 "BLEHGAA0903"
#define HG_BLE_LUXURY_MECHANICAL_SWITCH_4 "BLEHGAA0904"

////////////////////DEFINE IR////////////////////////////////
#define HG_BLE_SWITCH_SCENE "BLEHGAA010B"

////////////////////DEFINE IR////////////////////////////////
#define HG_BLE_IR "BLEHGAA0301"
#define HG_BLE_IR_TV "BLEHGAA0302"
#define HG_BLE_IR_FAN "BLEHGAA0303"
#define HG_BLE_IR_AC "BLEHGAA0304"
#define HG_BLE_IR_REMOTE "BLEHGAA0305"
#define HG_BLE_IR_V2 "BLEHGAA0316"
#define HG_BLE_IR_TV_V2 "BLEHGAA0317"
#define HG_BLE_IR_FAN_V2 "BLEHGAA0318"
#define HG_BLE_IR_AC_V2 "BLEHGAA0319"
#define HG_BLE_IR_REMOTE_V2 "BLEHGAA0320"
#define HG_BLE_IR_BLE "BLEHGAA0321"
#define HG_BLE_IR_TV_V2_LAO "BLEHGAA0322"
#define HG_BLE_IR_FAN_V2_LAO "BLEHGAA0323"
#define HG_BLE_IR_AC_V2_LAO "BLEHGAA0324"
#define HG_BLE_IR_REMOTE_V2_LAO "BLEHGAA0325"
#define HG_BLE_IR_FULL "BLEHGAA0301,BLEHGAA0302,BLEHGAA0303,BLEHGAA0304,BLEHGAA0305,BLEHGAA0316,BLEHGAA0317,BLEHGAA0318,BLEHGAA0319,BLEHGAA0320,BLEHGAA0321,BLEHGAA0322,BLEHGAA0323,BLEHGAA0324,BLEHGAA0325"

#define DPID_IR_BRAND_ID "1"
#define DPID_IR_REMOTE_ID "2"
#define DPID_IR_COMMAND_TYPE "3"
#define DPID_IR_VOICE "4"
#define DPID_IR_ONOFF "101"
#define DPID_IR_MODE "102"
#define DPID_IR_TEMP "103"
#define DPID_IR_FAN "104"
#define DPID_IR_SWING "105"
#define DPID_IR_COMMAND "106"

////////////////////DEFINE SENSOR ////////////////////////////////
#define HG_BLE_SENSOR_PRESENCE "BLEHGAA0606"
#define RD_BLE_SENSOR_SMOKE "BLEHGAA0406"
#define RD_BLE_SENSOR_DOOR "BLEHG030601,BLEHGAA0404" // Pin theo thứ tự danh sách là loại Nhỏ và Lớn
#define RD_BLE_SENSOR_DOOR_SMALL "BLEHG030601"       // Pin theo thứ tự danh sách là loại Nhỏ
#define RD_BLE_SENSOR_DOOR_LARGE "BLEHGAA0404"       // Pin theo thứ tự danh sách là loại Lớn
////////////////////DEFINE LIGHT ////////////////////////////////
#define BLE_LIGHT "BLEHGAA0201,BLEHGAA0202,BLEHGAA010A"
#define HG_BLE_LIGHT_WHITE "BLEHGAA0201"
#define HG_BLE_LIGHT_RGB "BLEHGAA0202"
#define HG_BLE_LIGHT_DIMMING "BLEHGAA010A"
////////////////////DEFINE PID FOR CURTAIN ////////////////////////////////
#define HG_BLE_CURTAIN "BLEHGAA0105,BLEHGAA0107,BLEHGAA0608,BLEHGAA0609,BHGMCS0112,BHGMCS0111,BHGCS0114,BHGMDC0115,BLEHGAA0106,BLEHGAA0109,BLEHGAA0605,BHGRDNS0110,BHGRDMS0113,BHGMCS0116,BHGMCS0117,BLEHGAA020C,BLEHGAA020F"
#define HG_BLE_CURTAIN_2_LAYER "BLEHGAA0107" // chua co phan hoi vi tri
#define HG_BLE_CURTAIN_IH35 "BLEHGAA0608"    // Phan hoi vi tri
#define HG_BLE_CURTAIN_IH68 "BLEHGAA0609"    // Phan hoi vi tri
#define HG_BLE_CURTAIN_NEW "BLEHGAA0105,BHGMCS0112,BHGCS0114,BHGMDC0115,BHGMCS0117,BLEHGAA0608,BLEHGAA0609,BLEHGAA020C,BLEHGAA020F"
#define HG_BLE_CURTAIN_1_LAYER_MECHANICAL "BHGMCS0112"         // Phan hoi vi tri
#define HG_BLE_CURTAIN_2_LAYER_MECHANICAL "BHGMCS0111"         // chua co phan hoi vi tri
#define HG_BLE_CURTAIN_NORMAL_NEW "BHGCS0114"                  // Phan hoi vi tri
#define HG_BLE_CURTAIN_MODULE_NEW "BHGMDC0115"                 // Phan hoi vi tri
#define HG_BLE_VIP_CURTAIN_1_LAYER_MECHANICAL "BHGMCS0117"     // Phan hoi vi tri
#define HG_BLE_VIP_CURTAIN_2_LAYER_MECHANICAL "BHGMCS0116"     // chua co phan hoi vi tri
#define HG_BLE_LUXURY_CURTAIN_1_LAYER_MECHANICAL "BLEHGAA020C" // Phan hoi vi tri
#define HG_BLE_LUXURY_CURTAIN_2_LAYER_MECHANICAL "BLEHGAA020F" // chua co phan hoi vi tri

#define HG_BLE_CURTAIN_OLD "BLEHGAA0107,BHGMCS0111,BHGMCS0116"
////////////////////DEFINE PID FOR ROLLING_DOOR ////////////////////////////////
#define HG_BLE_ROLLING "BLEHGAA0106,BLEHGAA0109,BLEHGAA0605,BHGRDNS0110,BHGRDMS0113,BHGRDNS0118,BHGRDMS8000,BLEHGAA020E"
#define HG_BLE_ROLLING_DOOR "BLEHGAA0106"
#define HG_BLE_ROLLING_DOOR_SWITCH "BLEHGAA0109"
#define HG_BLE_ROLLING_DOOR_MODULE "BLEHGAA0605"
#define HG_BLE_ROLLING_DOOR_SWITCH_MECHANICAL_3 "BHGRDNS0110"
#define HG_BLE_ROLLING_DOOR_SWITCH_MECHANICAL_1 "BHGRDMS0113"
#define HG_BLE_VIP_ROLLING_DOOR_SWITCH_MECHANICAL_3 "BHGRDNS0118"
#define HG_BLE_VIP_ROLLING_DOOR_SWITCH_MECHANICAL_1 "BHGRDMS8000"
#define HG_BLE_LUXURY_ROLLING_DOOR_SWITCH_MECHANICAL_3 "BLEHGAA020E"
/////////////////////////////////STAIR////////////////////////////////
#define HG_BLE_STAIR "BLEHGAA0108,BLEHGAA0208,BLEHGAA0209"
#define HG_BLE_NORMAL_STAIR "BLEHGAA0108"
#define HG_BLE_LUXURY_MECHANICAL_STAIR "BLEHGAA0208"
#define HG_BLE_VIP_MECHANICAL_STAIR "BLEHGAA0209"

////////////////////////////////DIMMING ////////////////////////////////
#define HG_BLE_SWITCHES_DIMMING "BLEHGAA010C,BLEHGAA010D,BLEHGAA010E"
#define HG_BLE_SWITCH_DIMMING "BLEHGAA010C"
#define HG_BLE_MECHANICAL_SWITCH_DIMMING "BLEHGAA010D"
#define HG_BLE_VIP_MECHANICAL_SWITCH_DIMMING "BLEHGAA010E"
///////////////////////////////////////////////////////////////////////

/**
 * @brief Nhóm (group) của thiết bị BLE, phục vụ điều khiển theo nhóm.
 */
enum
{
  BLE_DOWNLIGHT_SMT_GROUP = 1,
  BLE_DOWNLIGHT_RGBCW_GROUP = 20,
  BLE_SWITCH_TOUCH_GROUP = 31,
};

/**
 * @brief Nguồn điện của thiết bị.
 */
enum
{
  POWER_UNKNOWN = 0, /**< Không xác định */
  POWER_BATTERY,     /**< Pin */
  POWER_AC           /**< Điện xoay chiều */
};

/**
 * @brief Loại group trong hệ thống.
 */
enum
{
  TYPE_GROUP_ROOM = 0,  /**< Nhóm phòng */
  TYPE_GROUP_SWITCH = 1 /**< Nhóm công tắc */
};

/* ========================================================================== */
/*                               CLASS DEVICE                                */
/* ========================================================================== */

/**
 * @class Device
 * @brief Lớp cơ sở cho tất cả các thiết bị trong hệ thống.
 *
 * Lớp này lưu thông tin nhận dạng (MAC, type, version, data JSON),
 * quản lý thuộc tính, trạng thái online/offline và xử lý dữ liệu gửi/nhận.
 */
class Device : public Object
{
protected:
  std::string mac;      /**< Địa chỉ MAC của thiết bị */
  std::string type;     /**< Loại thiết bị (BLE type ID) */
  Json::Value dataJson; /**< Dữ liệu cấu hình thiết bị */
  uint16_t version;     /**< Phiên bản firmware */
  int rssi;             /**< Giá trị RSSI gần nhất */
  protocol_e protocol;  /**< Giao thức kết nối (BLE/Wi-Fi/khác) */
  bool isFavorite;      /**< Đánh dấu thiết bị được yêu thích */
  Json::Value values;   /**< Các giá trị đo lường hoặc trạng thái hiện tại */
  Json::Value
      propertyJsonUpdate;    /**< JSON chứa thông tin cập nhật thuộc tính */
  int lastBatteryLevel = 10; /**< Mức pin gần nhất (nếu có) */
  bool sensitive = false;    /**< Cờ thể hiện thiết bị nhạy cảm (sensitive) */

public:
#if 1 // Rule feature disabled
  vector<RuleInputDevice *>
      deviceRuleInputList; /**< Danh sách các rule liên quan đến thiết bị */
#endif

  int powerSource;            /**< Nguồn điện của thiết bị (AC/Battery) */
  bool lastOnlineState;       /**< Trạng thái online gần nhất */
  time_t lastTimeActive;      /**< Lần cuối thiết bị hoạt động */
  time_t lastTimeCheckActive; /**< Lần cuối hệ thống kiểm tra trạng thái hoạt
                                 động */

public:
  /** @name Constructor & Destructor */
  /** @{ */
  Device(std::string id, std::string name, std::string mac, string type,
         uint16_t addr, uint16_t version, Json::Value *dataJson);
  virtual ~Device();
  /** @} */

  /** @name Getter / Setter cơ bản */
  /** @{ */
  std::string GetMac();
  string GetType();
  uint16_t GetVersion();
  std::string GetVersionStr();
  Json::Value GetData();
  void SetData(Json::Value &dataValue);

  int GetRSSI();
  void SetRSSI(int rssi);

  bool IsSensitive();
  void SetSensitive(bool isSensitive);

  protocol_e GetProtocol();
  /** @} */

  /** @name Trạng thái thiết bị */
  /** @{ */
  bool IsOnline();
  bool IsNeedCheckOnline();
  void UpdateLastTimeActive();

  bool GetIsFavorite();
  bool SetIsFavorite(bool isFavorite);
  /** @} */

  /** @name Quản lý Rule Trigger */
  /** @{ */
#if 1 // Rule feature disabled
  void RegisterTrigger(RuleInputDevice *ruleInputDevice);
  void UnregisterTrigger(RuleInputDevice *ruleInputDevice);
#endif
  virtual void CheckTrigger();
  /** @} */

  /** @name Quản lý dữ liệu thuộc tính và telemetry */
  /** @{ */
  virtual int BuildAttributeValue(Json::Value &pushDataValue);
  virtual int BuildTelemetryValue(Json::Value &pushDataValue)
  {
    return CODE_ERROR;
  }

  void DeviceInputData(uint8_t *data, int len, uint16_t addr);

  virtual int InitAttribute(std::string attribute, double value)
  {
    return CODE_ERROR;
  }
  virtual int DoJsonArray(Json::Value &dataValue);

  virtual void SetPropertyJsonUpdate(Json::Value property);
  virtual Json::Value GetPropertyJsonUpdate();
  virtual void UpdatePropertyJsonUpdate(Json::Value &property);

  int PushAttribute();
  int PushAttribute(Json::Value &jsonValue);
  int PushTelemetry();
  int PushTelemetry(Json::Value &jsonValue);
  int PushOnlineState(bool isOnline);
  /** @} */

  /** @name Xử lý dữ liệu đầu vào (input/output) */
  /** @{ */
  virtual void InputData(Json::Value &dataValue) {}
  virtual void InputData(uint8_t *data, int len, uint16_t epId = 0) {};
  virtual bool CheckData(Json::Value &dataValue, bool &rs) { return false; }
  virtual int GetNumElement() { return 1; }
  /** @} */

  /** @name Các hành động thêm/xóa khỏi nhóm, phòng, scene */
  /** @{ */
  virtual int AddToGroup(uint16_t groupAddr, uint16_t epId)
  {
    return CODE_ERROR;
  }
  virtual int RemoveFromGroup(uint16_t groupAddr, uint16_t epId)
  {
    return CODE_ERROR;
  }
  virtual int AddToScene(uint16_t sceneAddr, Json::Value &dataValue,
                         uint16_t groupAddr = 0)
  {
    return CODE_ERROR;
  }
  virtual int RemoveFromScene(uint16_t sceneAddr, Json::Value &dataValue,
                              uint16_t groupAddr = 0)
  {
    return CODE_ERROR;
  }
  virtual int EditInScene(uint16_t sceneAddr, Json::Value &dataValue)
  {
    return CODE_ERROR;
  }

  virtual int AddToRoom(uint16_t roomAddr) { return CODE_ERROR; }
  virtual int RemoveFromRoom(uint16_t roomAddr) { return CODE_ERROR; }

  virtual int AddToGroupInRoom(uint16_t groupAddr, uint32_t groupType)
  {
    return CODE_ERROR;
  }
  virtual int RemoveFromGroupInRoom(uint16_t groupAddr) { return CODE_ERROR; }

  virtual int AddToSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue,
                               uint16_t roomAddr = 0)
  {
    return CODE_ERROR;
  }
  virtual int RemoveFromSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue,
                                    uint16_t roomAddr = 0)
  {
    return CODE_ERROR;
  }

  virtual int Do(Json::Value &dataValue) { return CODE_ERROR; }
  /** @} */

  /** @name Phương thức static và tiện ích */
  /** @{ */
  static void InitDeviceModelList();
  static void RegisterDeviceModel(const char *type, const char *model);

  static uint32_t BleTypeToGroupId(uint32_t deviceType);
  static uint32_t BleGroupIntByString(const char *groupStr);

  static const char *ConvertDeviceTypeToName(uint32_t type);
  static const char *ConvertDeviceTypeToModel(uint32_t type);
  static uint32_t ConvertModelToDeviceType(const char *model);

  virtual void Isolate() {}
  virtual bool CheckAddr(uint16_t addr) { return this->addr == addr; }
  virtual std::string GetCloudId();
  virtual int GetPageIndex();
  virtual std::string GetDeviceKey();
  /** @} */
};
