/**
 * @file Device.h
 * @brief Định nghĩa lớp cơ sở Device cho các thiết bị gateway.
 *
 * File này chứa:
 * - Danh sách key thuộc tính của thiết bị (attribute keys)
 * - Các thuộc tính và giao thức thiết bị
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
  MODBUS_DEVICE = 3  /**< Thiết bị giao tiếp Modbus RTU qua RS485 */
} protocol_e;

/**
 * @brief Nguồn điện của thiết bị.
 */
enum
{
  POWER_UNKNOWN = 0, /**< Không xác định */
  POWER_BATTERY,     /**< Pin */
  POWER_AC           /**< Điện xoay chiều */
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
  std::string type;     /**< Loại thiết bị */
  Json::Value dataJson; /**< Dữ liệu cấu hình thiết bị */
  uint16_t version;     /**< Phiên bản firmware */
  int rssi;             /**< Giá trị RSSI gần nhất */
  protocol_e protocol;  /**< Giao thức kết nối */
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

  virtual int Do(Json::Value &dataValue) { return CODE_ERROR; }
  /** @} */

  /** @name Phương thức static và tiện ích */
  /** @{ */
  virtual void Isolate() {}
  virtual bool CheckAddr(uint16_t addr) { return this->addr == addr; }
  virtual std::string GetCloudId();
  virtual int GetPageIndex();
  virtual std::string GetDeviceKey();
  /** @} */
};
