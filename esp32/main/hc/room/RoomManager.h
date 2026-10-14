/**
 * @file RoomManager.h
 * @brief Định nghĩa lớp RoomManager — chịu trách nhiệm quản lý toàn bộ danh sách Room trong hệ thống.
 *
 * Lớp này sử dụng mô hình Singleton để đảm bảo chỉ tồn tại một thực thể duy nhất quản lý các phòng.
 * RoomManager cung cấp các hàm để thêm, xóa, duyệt, tìm kiếm Room theo ID hoặc địa chỉ,
 * cũng như xử lý quan hệ giữa Room với Device, Group và Scene.
 *
 * @note Các thao tác trên danh sách phòng (roomList) đều được bảo vệ bằng mutex để tránh xung đột truy cập đa luồng.
 */

#pragma once

#include <map>
#include <mutex>
#include "Room.h"

#define ROOM_ADDR_START 4096    ///< Địa chỉ bắt đầu dành cho các Room trong hệ thống.
#define ROOM_NUM_GROUPS_MAX 256 ///< Số lượng Group tối đa trong một Room.
#define NUM_ROOMS_MAX 45        ///< Giới hạn số lượng Room tối đa trong toàn hệ thống.

/**
 * @class RoomManager
 * @brief Lớp quản lý danh sách các Room trong hệ thống.
 *
 * RoomManager chịu trách nhiệm tạo, xóa, và tra cứu các Room theo ID hoặc địa chỉ.
 * Đồng thời cung cấp các hàm hỗ trợ xử lý quan hệ giữa Room và các đối tượng Device, Group, Scene.
 */
class RoomManager
{
private:
  std::map<std::string, Room *> roomList; ///< Danh sách các phòng (Room) được quản lý, ánh xạ theo ID.
  std::mutex roomListMtx;                 ///< Mutex bảo vệ roomList trong môi trường đa luồng.

  /**
   * @brief Constructor riêng tư để đảm bảo Singleton pattern.
   */
  RoomManager();

public:
  /**
   * @brief Lấy thể hiện duy nhất của RoomManager (Singleton).
   * @return Con trỏ đến thể hiện duy nhất của RoomManager.
   */
  static RoomManager *getInstance();

  /**
   * @brief Duyệt qua tất cả các Room và thực thi hàm callback chỉ định.
   * @param func Hàm callback nhận con trỏ đến từng Room.
   */
  void ForEach(std::function<void(Room *)> func);

  /**
   * @brief Tìm Room theo ID.
   * @param id Mã định danh duy nhất của Room.
   * @return Con trỏ đến Room tương ứng hoặc nullptr nếu không tìm thấy.
   */
  Room *GetRoomFromId(std::string id);

  /**
   * @brief Tìm Room theo địa chỉ.
   * @param addr Địa chỉ Room trong hệ thống.
   * @return Con trỏ đến Room tương ứng hoặc nullptr nếu không tìm thấy.
   */
  Room *GetRoomFromAddr(uint16_t addr);

  /**
   * @brief Lấy địa chỉ khả dụng tiếp theo cho một Room mới.
   * @return Địa chỉ Room mới.
   */
  uint16_t GetNextRoomAddr();

  /**
   * @brief Thêm một Room vào danh sách quản lý.
   * @param room Con trỏ đến Room cần thêm.
   * @return Con trỏ đến Room đã được thêm hoặc nullptr nếu danh sách đầy.
   */
  Room *AddRoom(Room *room);

  /**
   * @brief Xóa một Room khỏi danh sách.
   * @param room Con trỏ đến Room cần xóa.
   */
  void DelRoom(Room *room);

  /**
   * @brief Xóa toàn bộ các Room khỏi danh sách.
   */
  void DelAllRoom();

  /**
   * @brief Xóa một thiết bị khỏi tất cả các Room có chứa thiết bị đó.
   * @param device Con trỏ đến thiết bị cần xóa.
   */
  void DelDevInAllRoom(Device *device);

  /**
   * @brief Lấy Room chứa một thiết bị cụ thể.
   * @param device Con trỏ đến thiết bị cần tìm.
   * @return Con trỏ đến Room chứa thiết bị hoặc nullptr nếu không tìm thấy.
   */
  Room *GetRoomDeviceIn(Device *device);

  /**
   * @brief Lấy Room chứa một Group cụ thể.
   * @param group Con trỏ đến Group cần tìm.
   * @return Con trỏ đến Room chứa Group hoặc nullptr nếu không tìm thấy.
   */
  Room *GetRoomGroupIn(Group *group);

  /**
   * @brief Lấy Room chứa một Scene cụ thể.
   * @param scene Con trỏ đến Scene cần tìm.
   * @return Con trỏ đến Room chứa Scene hoặc nullptr nếu không tìm thấy.
   */
  Room *GetRoomSceneIn(Scene *scene);

  /**
   * @brief In thông tin toàn bộ các Room ra log (hoặc console).
   */
  void PrintRoom();
};
