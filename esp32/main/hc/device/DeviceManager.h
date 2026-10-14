/**
 * @file DeviceManager.h
 * @brief Định nghĩa lớp DeviceManager để quản lý danh sách các thiết bị trong hệ thống.
 *
 * Lớp này sử dụng mẫu Singleton để đảm bảo chỉ có một đối tượng quản lý duy nhất,
 * chịu trách nhiệm thêm, xóa, truy xuất và duyệt qua các thiết bị (Device).
 * Nếu cấu hình `CONFIG_ENABLE_BLE` được bật, lớp cũng hỗ trợ quản lý các thiết bị BLE.
 */

#pragma once

#include <map>
#include <mutex>
#include "Device.h"

#ifdef CONFIG_ENABLE_BLE
#include "DeviceBle.h"
#endif

/**
 * @class DeviceManager
 * @brief Lớp quản lý tất cả các thiết bị (Device) trong hệ thống.
 *
 * Lớp này cung cấp các phương thức để:
 * - Thêm thiết bị mới
 * - Xóa thiết bị
 * - Lấy thiết bị theo ID hoặc MAC
 * - Duyệt qua danh sách thiết bị
 * - (Nếu bật BLE) Lấy thiết bị BLE theo địa chỉ, và quản lý địa chỉ BLE.
 *
 * Thiết kế theo mẫu Singleton — chỉ có một thể hiện duy nhất trong chương trình.
 */
class DeviceManager
{
private:
  map<std::string, Device *> deviceList; /**< Danh sách các thiết bị, key là ID hoặc MAC */
  mutex deviceListMtx;                   /**< Mutex bảo vệ truy cập đồng thời đến deviceList */

  /**
   * @brief Hàm khởi tạo riêng tư theo mẫu Singleton.
   */
  DeviceManager();

public:
  /**
   * @brief Lấy thể hiện duy nhất của DeviceManager.
   *
   * @return Con trỏ đến đối tượng DeviceManager duy nhất.
   */
  static DeviceManager *getInstance();

  /**
   * @brief Duyệt qua tất cả các thiết bị và thực thi hàm callback.
   *
   * @param func Hàm callback nhận đối tượng `Device*` làm tham số.
   */
  void ForEach(function<void(Device *)> func);

  /**
   * @brief Thêm thiết bị mới vào danh sách.
   *
   * @param mac Địa chỉ MAC của thiết bị.
   * @param type Loại thiết bị (device type).
   * @param addr Địa chỉ (nếu có, mặc định 0).
   * @param version Phiên bản firmware/hardware (mặc định 0).
   * @param dataJson Dữ liệu JSON bổ sung cho thiết bị (có thể NULL).
   * @return Con trỏ đến đối tượng `Device` vừa được tạo hoặc NULL nếu thất bại.
   */
  Device *AddDevice(std::string &mac, std::string &type, string &name, uint16_t addr = 0, uint16_t version = 0, Json::Value *dataJson = NULL);
  Device *AddDeviceNoReset(std::string &mac, std::string &type, string &name,
                           uint16_t addr = 0, uint16_t version = 0,
                           Json::Value *dataJson = NULL);
  Device *AddDeviceNoResetWithId(std::string id, std::string mac,
                                 std::string &type, string &name,
                                 uint16_t addr = 0, uint16_t version = 0,
                                 Json::Value *dataJson = NULL);

  /**
   * @brief Lấy thiết bị theo ID.
   *
   * @param id ID của thiết bị.
   * @return Con trỏ đến `Device` nếu tìm thấy, NULL nếu không có.
   */
  Device *GetDeviceFromId(std::string id);

  /** Resolve the ID published to the server in deviceUpdate. */
  Device *GetDeviceFromCloudId(std::string cloudId);

  /**
   * @brief Lấy thiết bị theo địa chỉ MAC.
   *
   * @param mac Địa chỉ MAC của thiết bị.
   * @return Con trỏ đến `Device` nếu tồn tại, NULL nếu không có.
   */
  Device *GetDeviceFromMac(std::string mac);

#ifdef CONFIG_ENABLE_BLE
  /**
   * @brief Lấy thiết bị BLE theo địa chỉ.
   *
   * @param addr Địa chỉ BLE của thiết bị.
   * @return Con trỏ đến `DeviceBle` nếu tìm thấy, NULL nếu không.
   */
  DeviceBle *GetDeviceBleFromAddr(uint16_t addr);

  /**
   * @brief Lấy địa chỉ BLE kế tiếp dùng cho thiết bị được Android Provision.
   *
   * @return Địa chỉ BLE mới có thể sử dụng.
   */
  uint32_t GetNextAddrBleAndroidProvision();

  /**
   * @brief Lấy địa chỉ BLE lớn nhất hiện đang được sử dụng.
   *
   * @return Địa chỉ BLE lớn nhất.
   */
  uint32_t GetMaxAddrBle();

  /**
   * @brief Lấy danh sách tất cả thiết bị BLE hiện có.
   *
   * @return Vector chứa con trỏ đến các đối tượng `Device`.
   */
  vector<Device *> GetListDeviceBle();
#endif

  /**
   * @brief Xóa một thiết bị khỏi danh sách.
   *
   * @param device Con trỏ đến thiết bị cần xóa.
   */
  void DelDevice(Device *device);
  void DetachDevice(Device *device);

  /**
   * @brief Xóa toàn bộ thiết bị khỏi danh sách.
   */
  void DelAllDevice();

  int GetDeviceCount();
};
