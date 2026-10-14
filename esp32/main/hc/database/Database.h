/**
 * @file Database.h
 * @brief Lớp quản lý cơ sở dữ liệu SQLite cho hệ thống Smart Gateway.
 *
 * File này định nghĩa lớp `Database`, chịu trách nhiệm thao tác với
 * cơ sở dữ liệu SQLite: đọc, ghi, cập nhật và xóa thông tin về các
 * thực thể như thiết bị (Device), nhóm (Group), phòng (Room), cảnh (Scene),
 * luật (Rule) và gateway.
 *
 * Lớp được triển khai theo mẫu Singleton nhằm đảm bảo chỉ có một kết nối
 * cơ sở dữ liệu duy nhất trong toàn hệ thống, giúp tránh xung đột truy cập.
 *
 */

#pragma once

#include "Gateway.h"
#include <mutex>
#include <sqlite3.h>
#include <string.h>
#include <string>
#include <vector>


/**
 * @class Database
 * @brief Lớp quản lý thao tác với cơ sở dữ liệu SQLite.
 *
 * Lớp này cung cấp giao diện thống nhất để khởi tạo, kiểm tra, đọc và cập nhật
 * dữ liệu trong SQLite. Các thao tác được đồng bộ hóa bằng mutex để đảm bảo an
 * toàn luồng.
 */
class Database {
private:
  sqlite3 *db; /**< Con trỏ tới cơ sở dữ liệu SQLite hiện tại */
  std::mutex mtx; /**< Mutex dùng để bảo vệ truy cập đồng thời */

  /**
   * @brief Hàm khởi tạo riêng tư (theo mẫu Singleton).
   */
  Database();

  /**
   * @brief Hàm hủy riêng tư, đóng kết nối CSDL khi không còn sử dụng.
   */
  ~Database();

  /**
   * @brief Thực thi câu lệnh SQL không trả về dữ liệu.
   * @param sql Chuỗi lệnh SQL cần thực thi.
   * @return Mã kết quả từ SQLite (0 nếu thành công).
   */
  int Sqlite_Exec(std::string &sql);

  /**
   * @brief Đọc toàn bộ dữ liệu từ bảng và parse qua hàm callback.
   * @param table Tên bảng cần đọc.
   * @param listPtr Con trỏ tới danh sách lưu kết quả.
   * @param Parse Hàm callback xử lý từng bản ghi.
   * @return Số lượng bản ghi đọc được hoặc mã lỗi.
   */
  int ReadAll(std::string table, void *listPtr,
              int (*Parse)(sqlite3_stmt *, void *));

public:
  /**
   * @brief Lấy thể hiện duy nhất của lớp Database.
   * @return Con trỏ tới đối tượng Database duy nhất.
   */
  static Database *getInstance();

  /**
   * @brief Khởi tạo kết nối cơ sở dữ liệu.
   */
  void Init(void);
  bool EnsureReady();

  /**
   * @brief Kiểm tra sự tồn tại của cơ sở dữ liệu.
   * @param dbName Tên file cơ sở dữ liệu.
   * @return true nếu tồn tại, false nếu không.
   */
  bool IsHaveDb(const char *dbName);

  /**
   * @brief Tạo các bảng nếu chưa tồn tại.
   * @return 0 nếu thành công.
   */
  int CreateTableIfNotExists();

  /**
   * @brief Xóa toàn bộ cơ sở dữ liệu.
   */
  void DelDatabase();

  /**
   * @brief Bắt đầu transaction SQLite.
   */
  int SqliteBenginTransaction();

  /**
   * @brief Kết thúc transaction SQLite.
   */
  int SqliteEndTransaction();

  //==================================================================
  // Các nhóm thao tác dữ liệu
  //==================================================================

  /** @name Thiết bị (Device)
   *  @{
   */
  int DeviceRead();
  int DeviceAdd(Device *device);
  int DeviceUpdate(Device *device);
  int DeviceUpdateData(Device *device);
  int DeviceDel(Device *device);
  int DeviceDel(std::string mac);
  int DeviceDelAll();
  int DelDevExist(Device *device);
  int DeviceUpdateFavorite(Device *device);
  /** @} */

  /** @name Thuộc tính thiết bị (Device Attribute)
   *  @{
   */
  int DeviceAttributeRead();
  int DeviceAttributeAdd(Device *device, std::string attribute, double value);
  int DeviceAttributeUpdate(Device *device, std::string attribute,
                            double value);
  int DeviceAttributeDel(Device *device, std::string attribute);
  int DeviceAttributeDelAll();
  /** @} */

  /** @name Lịch sử datapoint thiết bị
   *  @{
   */
  int DeviceHistoryAdd(const std::string &deviceId, int dpId,
                       const std::string &dpValue, int causeType = 0,
                       const std::string &causeId = "", int eventType = 1);
  int DeviceHistoryFind(const std::string &deviceId, uint64_t startTime,
                        uint64_t endTime, const std::vector<int> &dpIds,
                        int causeType, int eventType, int limit,
                        Json::Value &rows);
  /** @} */

  /** @name Thiết bị con BLE (Device BLE Child)
   *  @{
   */
  int DeviceBleChildRead();
  int DeviceBleChildAdd(Device *child, Device *parent, std::string data);
  int DeviceBleChildAdd(std::string childId, std::string parentId, std::string data);
  int DeviceBleChildUpdateData(Device *child, Device *parent, std::string data);
  int DeviceBleChildDel(Device *child, Device *parent);
  int DeviceBleChildDelAll();
  /** @} */

#if 1 // Group feature disabled
	/** @name Liên kết thiết bị - nhóm (DeviceInGroup)
	 *  @{
	 */
	int DeviceInGroupRead();
	int DeviceInGroupAdd(Group *group, Device *device, uint16_t epId);
	int DeviceInGroupDel(Group *group, Device *device, uint16_t epId);
	int DeviceInGroupDelDev(std::string deviceId);
	int DeviceInGroupDelDev(Device *device);
	int DeviceInGroupDelAll();
	/** @} */
#endif

#if 1 // Room feature disabled
	/** @name Liên kết thiết bị - phòng (DeviceInRoom)
	 *  @{
	 */
	int DeviceInRoomRead();
	int DeviceInRoomAdd(Room *room, Device *device, uint16_t epId);
	int DeviceInRoomDel(Room *room, Device *device, uint16_t epId);
	int DeviceInRoomDelDev(std::string deviceId);
	int DeviceInRoomDelDev(Device *device);
	int DeviceInRoomDelAll();
	/** @} */
#endif

#if 1 // Scene feature disabled
	/** @name Liên kết thiết bị - cảnh (DeviceInScene)
	 *  @{
	 */
	int DeviceInSceneRead();
	int DeviceInSceneAdd(Scene *scene, Device *device, std::string data);
	int DeviceInSceneDel(Scene *scene, Device *device);
	int DeviceInSceneDelDev(std::string deviceId);
	int DeviceInSceneDelDev(Device *device);
	int DeviceInSceneDelAll();
	/** @} */
#endif

  /** @name Gateway
   *  @{
   */
  int GatewayRead();
  int GatewayAdd();
  int GatewayUpdateId(std::string id);
  int GatewayUpdateNetKey(std::string netkey);
  int GatewayUpdateAppKey(std::string appkey);
  int GatewayUpdateDeviceKey(std::string devicekey);
  int GatewayUpdateUnicast(uint16_t unicast);
  int GatewayUpdateIvIndex(uint32_t iv_index);
  int GatewayUpdateDormitory(std::string dormitory);
  int GatewayUpdateRefreshToken(std::string refreshToken);
  int GatewayUpdateData(std::string data);
  int GatewayUpdateVersion(std::string version);
  int GatewayDel();
  int GatewayDel(std::string id);
  int GatewayDelAll();
  /** @} */

#if 1 // Group feature disabled
	/** @name Nhóm (Group)
	 *  @{
	 */
	int GroupRead();
	int GroupAdd(Group *group);
	int GroupUpdate(Group *group);
	int GroupUpdateRoom(Group *group, std::string roomId);
	int GroupDel(Group *group);
	int GroupDel(std::string id);
	int GroupDelAll();
	/** @} */
#endif

#if 1 // Room feature disabled
	/** @name Phòng (Room)
	 *  @{
	 */
	int RoomRead();
	int RoomAdd(Room *room);
	int RoomUpdateName(Room *room);
	int RoomUpdateData(Room *room);
	int RoomDel(Room *room);
	int RoomDel(std::string id);
	int RoomDelAll();
	/** @} */
#endif

#if 1 // Rule feature disabled
	/** @name Kịch bản (Rule)
	 *  @{
	 */
	int RuleRead();
	int RuleAdd(Rule *rule, std::string data, int type);
	int RuleUpdateData(Rule *rule, std::string data);
	int RuleUpdateStatus(Rule *rule);
	int RuleUpdateAddr(Rule *rule);
	int RuleUpdateType(Rule *rule, int type);
	int RuleUpdateFavorite(Rule *rule, bool isFavorite);
	int RuleUpdateFirstRun(Rule *rule, bool isFirstRun);
	int RuleDel(Rule *rule);
	int RuleDel(std::string id);
	int RuleDelAll();
	/** @} */
#endif

#if 1 // Scene feature disabled
	/** @name Cảnh (Scene)
	 *  @{
	 */
	int SceneRead();
	int SceneAdd(Scene *scene);
	int SceneUpdate(Scene *scene);
	int SceneUpdateRoom(Scene *scene, std::string roomId);
	int SceneDel(Scene *scene);
	int SceneDel(std::string id);
	int SceneDelAll();
	int SceneUpdateFavorite(Scene *scene);
	/** @} */
#endif
};
