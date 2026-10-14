/**
 * @file SceneManager.h
 * @brief Quản lý tất cả các Scene trong hệ thống smarthome.
 *
 * SceneManager cung cấp các phương thức thêm, xóa, truy vấn và duyệt các Scene.
 * Sử dụng singleton để đảm bảo chỉ có một thể hiện duy nhất.
 */

#pragma once

#include <map>
#include <mutex>
#include "Scene.h"

/**
 * @class SceneManager
 * @brief Lớp quản lý tất cả các Scene.
 *
 * SceneManager lưu trữ danh sách Scene, hỗ trợ truy vấn theo ID, địa chỉ,
 * thêm/xóa Scene và duyệt danh sách Scene.
 */
class SceneManager
{
private:
  std::map<std::string, Scene *> sceneList; /**< Bản đồ ID -> Scene */
  std::mutex sceneListMtx;                  /**< Mutex bảo vệ sceneList */

  /**
   * @brief Constructor private (singleton)
   */
  SceneManager();

public:
  /**
   * @brief Lấy thể hiện duy nhất của SceneManager.
   * @return Con trỏ tới SceneManager.
   */
  static SceneManager *getInstance();

  /**
   * @brief Duyệt tất cả Scene và thực hiện callback.
   * @param func Hàm callback nhận Scene*.
   */
  void ForEach(std::function<void(Scene *)> func);

  /**
	 * @brief Thêm một Scene mới từ dữ liệu JSON.
	 * @param sceneValue Dữ liệu JSON mô tả Scene.
	 * @return Con trỏ tới Scene vừa thêm.
	 */
	Scene *AddScene(Json::Value &sceneValue);

  /**
   * @brief Lấy Scene theo ID.
   * @param id ID của Scene.
   * @return Con trỏ Scene nếu tồn tại, nullptr nếu không.
   */
  Scene *GetSceneFromId(std::string id);

  /**
   * @brief Lấy Scene theo địa chỉ.
   * @param addr Địa chỉ Scene.
   * @return Con trỏ Scene nếu tồn tại, nullptr nếu không.
   */
  Scene *GetSceneFromAddr(uint16_t addr);

  /**
   * @brief Lấy địa chỉ tiếp theo khả dụng cho Scene mới.
   * @return Địa chỉ Scene tiếp theo.
   */
  uint16_t GetNextSceneAddr();

  /**
   * @brief Thêm Scene vào quản lý.
   * @param scene Con trỏ Scene cần thêm.
   */
  void AddScene(Scene *scene);

  /**
   * @brief Xóa Scene khỏi quản lý.
   * @param scene Con trỏ Scene cần xóa.
   */
  void DelScene(Scene *scene);

  /**
   * @brief Xóa tất cả Scene và giải phóng bộ nhớ.
   */
  void DelAllScene();

  /**
   * @brief In thông tin tất cả Scene (debug/log).
   */
  void PrintScene();
};
