/**
 * @file GroupManager.h
 * @brief Định nghĩa lớp GroupManager — quản lý danh sách các nhóm (Group) trong hệ thống.
 *
 * @details
 * Lớp GroupManager chịu trách nhiệm:
 *  - Lưu trữ, thêm, xóa và truy xuất các nhóm (Group).
 *  - Cung cấp phương thức duyệt qua toàn bộ nhóm với callback.
 *  - Đảm bảo thread-safe bằng cơ chế mutex.
 *  - Triển khai mô hình Singleton: chỉ tồn tại một thể hiện duy nhất trong toàn hệ thống.
 *
 * @note
 *  - Các thao tác xóa (`DelGroup`, `DelAllGroup`) cần đảm bảo không có thread khác
 *    đang sử dụng cùng một đối tượng Group để tránh lỗi truy cập bộ nhớ.
 *  - Khi gọi `ForEach()`, tránh gọi lại các hàm khác của GroupManager để tránh deadlock.
 */

#pragma once

#include <map>
#include <mutex>
#include "Group.h"

/**
 * @class GroupManager
 * @brief Lớp quản lý toàn bộ các đối tượng Group trong hệ thống.
 *
 * GroupManager lưu trữ danh sách tất cả Group (nhóm thiết bị), hỗ trợ thêm, xóa, 
 * duyệt và truy xuất nhóm theo ID hoặc địa chỉ (addr). 
 * Toàn bộ các thao tác đều được bảo vệ bằng mutex để đảm bảo thread-safe.
 *
 * Lớp này được triển khai theo mô hình Singleton — chỉ tồn tại duy nhất một thể hiện.
 */
class GroupManager
{
private:
  /**
   * @brief Danh sách các nhóm trong hệ thống, ánh xạ theo groupId.
   *        Key: groupId (std::string)
   *        Value: con trỏ đến đối tượng Group tương ứng.
   */
  std::map<std::string, Group *> groupList;

  /**
   * @brief Mutex bảo vệ việc truy cập và thao tác với groupList.
   */
  std::mutex groupListMtx;

  /**
   * @brief Constructor riêng tư (Private Constructor) 
   *        để đảm bảo chỉ có thể khởi tạo qua getInstance().
   */
  GroupManager();

public:
  /**
   * @brief Lấy thể hiện duy nhất của GroupManager (Singleton pattern).
   * @return Con trỏ đến đối tượng GroupManager.
   */
  static GroupManager *getInstance();

  /**
   * @brief Duyệt qua tất cả các nhóm hiện có và thực thi hàm callback cho từng nhóm.
   * @param func Hàm callback nhận tham số là con trỏ đến Group.
   * 
   * Lưu ý: Không nên gọi các hàm khác của GroupManager từ trong callback,
   * để tránh deadlock do mutex đang bị giữ.
   */
  void ForEach(std::function<void(Group *)> func);

  /**
   * @brief Lấy con trỏ đến Group theo groupId.
   * @param id Chuỗi định danh của nhóm.
   * @return Con trỏ Group nếu tìm thấy, NULL nếu không tồn tại.
   */
  Group *GetGroupFromId(std::string id);

  /**
   * @brief Lấy con trỏ đến Group theo địa chỉ nhóm (addr).
   * @param addr Địa chỉ nhóm (BLE group address).
   * @return Con trỏ Group nếu tìm thấy, NULL nếu không tồn tại.
   */
  Group *GetGroupFromAddr(uint16_t addr);

  /**
   * @brief Tính toán địa chỉ nhóm kế tiếp có thể sử dụng.
   * @return Địa chỉ nhóm kế tiếp (từ 1 đến < 4096).
   */
  uint16_t GetNextGroupAddr();

  /**
   * @brief Thêm một nhóm mới vào danh sách quản lý.
   * @param group Con trỏ đến đối tượng Group cần thêm.
   */
  void AddGroup(Group *group);

  /**
   * @brief Xóa một nhóm cụ thể khỏi danh sách quản lý.
   * @param group Con trỏ đến nhóm cần xóa.
   * 
   * Hàm này sẽ gọi `delete group` sau khi xóa khỏi map.
   * Cần đảm bảo không có thread nào đang sử dụng group đó.
   */
  void DelGroup(Group *group);

  /**
   * @brief Xóa toàn bộ nhóm trong danh sách và giải phóng bộ nhớ.
   * 
   * Phiên bản mặc định thực hiện delete trong khi đang giữ mutex → 
   * có thể gây deadlock nếu được gọi trong callback hoặc từ thread khác.
   * Nên xem xét triển khai phiên bản an toàn hơn bằng snapshot (xóa ngoài lock).
   */
  void DelAllGroup();

  /**
   * @brief In thông tin toàn bộ group và danh sách thiết bị bên trong ra log.
   */
  void PrintGroup();
};
