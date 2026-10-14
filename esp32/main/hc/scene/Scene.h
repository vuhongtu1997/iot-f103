/**
 * @file Scene.h
 * @brief Định nghĩa lớp Scene và DeviceInScene trong hệ thống smarthome.
 *
 * Lớp Scene đại diện cho một cảnh (scene) gồm nhiều thiết bị.
 * DeviceInScene lưu thông tin thiết bị và dữ liệu liên quan trong Scene.
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include "json.h"
#include "Object.h"
#include "Device.h"

using std::string;

/**
 * @class DeviceInScene
 * @brief Lưu thông tin thiết bị và dữ liệu của thiết bị trong Scene.
 */
class DeviceInScene
{
public:
	Device *device;	  /**< Con trỏ tới thiết bị */
	Json::Value data; /**< Dữ liệu liên quan tới thiết bị trong Scene */

	/**
	 * @brief Constructor khởi tạo DeviceInScene.
	 * @param device Con trỏ tới Device.
	 * @param data Dữ liệu Json của thiết bị.
	 */
	DeviceInScene(Device *device, Json::Value data);
};

/**
 * @class Scene
 * @brief Đại diện cho một cảnh (scene) gồm nhiều thiết bị.
 *
 * Scene có thể thêm/xóa thiết bị, thực thi hành động,
 * và lưu trạng thái yêu thích, dữ liệu phản hồi từ thiết bị.
 */
class Scene : public Object
{
private:
	bool isFavorite; /**< Flag đánh dấu cảnh có được đánh dấu yêu thích hay không */
	Json::Value data; /**< Metadata của scene, dùng để quản lý và khôi phục sau reboot */
	std::mutex mtx;	 /**< Mutex bảo vệ dữ liệu bên trong Scene */

public:
	std::vector<DeviceInScene *> deviceList; /**< Danh sách các thiết bị trong Scene */

	/**
	 * @brief Constructor khởi tạo Scene.
	 * @param id ID của Scene.
	 * @param addr Địa chỉ Scene.
	 * @param name Tên Scene.
	 */
	Scene(std::string id, uint16_t addr, std::string name);

	/**
	 * @brief Destructor hủy Scene và giải phóng bộ nhớ liên quan.
	 */
	~Scene();

	/**
	 * @brief Lấy trạng thái yêu thích của Scene.
	 * @return true nếu là favorite, false nếu không.
	 */
	bool GetIsFavorite();

	/**
	 * @brief Đặt trạng thái yêu thích của Scene.
	 * @param isFavorite true nếu muốn đánh dấu favorite, false nếu không.
	 * @return Giá trị đã đặt.
	 */
	bool SetIsFavorite(bool isFavorite);

	/**
	 * @brief Lấy metadata đã lưu của Scene.
	 */
	Json::Value GetData();

	/**
	 * @brief Cập nhật metadata của Scene.
	 */
	void SetData(const Json::Value &data);

	/**
	 * @brief Lấy vị trí thiết bị trong danh sách deviceList.
	 * @param device Con trỏ tới thiết bị cần tìm.
	 * @return Chỉ số trong danh sách, -1 nếu không tồn tại.
	 */
	int GetPositionDevice(Device *device);

	/**
	 * @brief Lấy dữ liệu Json của thiết bị trong Scene.
	 * @param device Con trỏ tới thiết bị cần lấy dữ liệu.
	 * @return Json::Value dữ liệu thiết bị.
	 */
	Json::Value GetDataDeviceInScene(Device *device);

	/**
	 * @brief Thêm thiết bị vào Scene.
	 * @param device Con trỏ tới Device.
	 * @param data Dữ liệu Json liên quan đến thiết bị.
	 * @return 0 nếu thành công, giá trị khác nếu lỗi.
	 */
	int AddDevice(Device *device, Json::Value data);

	/**
	 * @brief Xóa thiết bị khỏi Scene.
	 * @param device Con trỏ tới Device cần xóa.
	 * @return 0 nếu thành công, giá trị khác nếu lỗi.
	 */
	int DelDevice(Device *device);

	/**
	 * @brief Thực thi Scene, gửi lệnh điều khiển thiết bị.
	 * @param ack true nếu cần phản hồi, false nếu không.
	 * @param groupAddr Địa chỉ nhóm, mặc định 0.
	 * @return 0 nếu thành công, giá trị khác nếu lỗi.
	 */
	int Do(bool ack = true, uint16_t groupAddr = 0);

	/**
	 * @brief Kiểm tra xem thiết bị có trong Scene không.
	 * @param device Con trỏ tới Device.
	 * @return true nếu có, false nếu không.
	 */
	bool IsDevIn(Device *device);

	std::map<std::string, Json::Value> listRsp; /**< Danh sách dữ liệu phản hồi từ các thiết bị */
	std::mutex listRspMtx;						/**< Mutex bảo vệ listRsp */

	/**
	 * @brief Thêm dữ liệu phản hồi vào listRsp.
	 * @param device Con trỏ tới Device.
	 * @param dataValue Dữ liệu Json cần thêm.
	 */
	void AddDataToListRsp(Device *device, Json::Value &dataValue);
};
