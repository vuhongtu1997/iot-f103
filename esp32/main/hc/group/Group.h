/**
 * @file Group.h
 * @brief Định nghĩa lớp Group và DeviceInGroup dùng để quản lý nhóm thiết bị trong hệ thống.
 *
 * Lớp này cho phép thêm, xoá, tìm kiếm và điều khiển các thiết bị nằm trong một nhóm.
 * Đồng thời cung cấp cơ chế thread-safe để truy cập danh sách thiết bị và phản hồi dữ liệu.
 */

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "json.h"
#include "Object.h"
#include "Device.h"

/**
 * @class DeviceInGroup
 * @brief Đại diện cho một thiết bị nằm trong nhóm, kèm theo ID endpoint.
 */
class DeviceInGroup
{
public:
	Device *device; ///< Con trỏ trỏ đến đối tượng Device
	uint16_t epId;	///< Endpoint ID của thiết bị trong nhóm

	/**
	 * @brief Khởi tạo đối tượng DeviceInGroup.
	 * @param device Con trỏ đến thiết bị.
	 * @param epId Endpoint ID của thiết bị.
	 */
	DeviceInGroup(Device *device, uint16_t epId);
};

/**
 * @class Group
 * @brief Đại diện cho một nhóm thiết bị, cho phép điều khiển và quản lý các thiết bị bên trong.
 */
class Group : public Object
{
private:
	std::mutex mtx; ///< Mutex bảo vệ thao tác với danh sách thiết bị

public:
	std::vector<DeviceInGroup *> deviceList; ///< Danh sách các thiết bị trong nhóm

	/**
	 * @brief Hàm khởi tạo đối tượng Group.
	 * @param id ID của nhóm.
	 * @param addr Địa chỉ nhóm (Group Address).
	 * @param name Tên của nhóm.
	 */
	Group(std::string id, uint16_t addr, std::string name);

	/**
	 * @brief Hàm huỷ Group, giải phóng bộ nhớ danh sách thiết bị.
	 */
	~Group();

	/**
	 * @brief Lấy vị trí của thiết bị trong danh sách nhóm.
	 * @param device Con trỏ đến thiết bị cần tìm.
	 * @param epId Endpoint ID của thiết bị.
	 * @return Vị trí (index) của thiết bị trong danh sách, hoặc -1 nếu không tồn tại.
	 */
	int GetPositionDevice(Device *device, uint16_t epId);

	/**
	 * @brief Thêm một thiết bị vào nhóm.
	 * @param device Con trỏ đến thiết bị cần thêm.
	 * @param epId Endpoint ID của thiết bị.
	 * @return CODE_OK nếu thêm thành công, CODE_ERROR nếu thất bại.
	 */
	int AddDevice(Device *device, uint16_t epId);

	/**
	 * @brief Xoá một thiết bị ra khỏi nhóm dựa trên endpoint ID.
	 * @param device Con trỏ đến thiết bị cần xoá.
	 * @param epId Endpoint ID của thiết bị.
	 * @return CODE_OK nếu xoá thành công, CODE_ERROR nếu thất bại.
	 */
	int DelDevice(Device *device, uint16_t epId);

	/**
	 * @brief Chuyển DP ID của thiết bị sang địa chỉ element BLE.
	 */
	static uint16_t GetElementFromDp(Device *device, int dpId);

	/**
	 * @brief Chuyển địa chỉ element BLE về DP ID.
	 */
	static int GetDpFromElement(Device *device, uint16_t epId);

	/**
	 * @brief Kiểm tra một DP của thiết bị đã có trong nhóm hay chưa.
	 */
	bool HasLinkDevice(Device *device, int dpId);

	/**
	 * @brief Đăng ký DP của thiết bị vào group link và cập nhật membership.
	 */
	int AddLinkDevice(Device *device, int dpId);

	/**
	 * @brief Gỡ DP của thiết bị khỏi group link và cập nhật membership.
	 */
	int RemoveLinkDevice(Device *device, int dpId);

	/**
	 * @brief Tạo danh sách member group link để trả về cloud.
	 */
	Json::Value BuildLinkDevicesJson();

	/**
	 * @brief Xoá toàn bộ instance của một thiết bị khỏi nhóm (mọi endpoint).
	 * @param device Con trỏ đến thiết bị cần xoá.
	 * @return CODE_OK nếu xoá thành công, CODE_ERROR nếu không tìm thấy.
	 */
	int RemoveDevice(Device *device);

	/**
	 * @brief Gửi lệnh điều khiển đến toàn bộ thiết bị trong nhóm.
	 * @param dataValue Dữ liệu JSON chứa thông tin điều khiển (như ON/OFF, DIM, CCT...).
	 * @param ack Cờ xác nhận (true nếu cần phản hồi từ thiết bị).
	 * @return CODE_OK nếu thực thi thành công.
	 */
	int Do(Json::Value &dataValue, bool ack = true);

	/**
	 * @brief Kiểm tra xem thiết bị có thuộc nhóm hay không.
	 * @param device Con trỏ đến thiết bị cần kiểm tra.
	 * @return true nếu thiết bị thuộc nhóm, false nếu không.
	 */
	bool IsDevIn(Device *device);

	std::map<std::string, Json::Value> listRsp; ///< Danh sách phản hồi từ thiết bị trong nhóm
	std::mutex listRspMtx;						///< Mutex bảo vệ truy cập listRsp

	/**
	 * @brief Thêm dữ liệu phản hồi của thiết bị vào danh sách phản hồi.
	 * @param device Thiết bị gửi phản hồi.
	 * @param dataValue Dữ liệu JSON phản hồi từ thiết bị.
	 */
	void AddDataToListRsp(Device *device, Json::Value &dataValue);
};
