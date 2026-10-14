/**
 * @file RuleInputDevice.h
 * @brief Định nghĩa lớp RuleInputDevice — đầu vào (input) dạng thiết bị cho hệ thống Rule.
 *
 * Lớp này kế thừa từ RuleInput và đại diện cho một đầu vào kiểu “thiết bị”.
 * Nó cho phép Rule được kích hoạt khi một thiết bị nhất định thay đổi trạng thái
 * (ví dụ: bật/tắt, thay đổi giá trị cảm biến, v.v.).
 *
 * @note RuleInputDevice thường được dùng để thiết lập tự động hóa dựa trên trạng thái của thiết bị thật.
 */

#pragma once

#include "RuleInput.h"
#include "Rule.h"
#include <functional>
#include "json.h"

using namespace std;

/**
 * @typedef DeviceRuleInputCallbackFunc
 * @brief Định nghĩa kiểu callback được gọi khi thiết bị thay đổi trạng thái.
 * @param value Giá trị boolean thể hiện trạng thái mới của thiết bị (true = kích hoạt, false = tắt).
 */
typedef function<void(bool)> DeviceRuleInputCallbackFunc;

class Device;

/**
 * @class RuleInputDevice
 * @brief Lớp đại diện cho đầu vào dạng thiết bị trong hệ thống Rule.
 *
 * Lớp này theo dõi trạng thái của một thiết bị và kích hoạt Rule khi thiết bị thay đổi.
 * Thông tin chi tiết của thiết bị và điều kiện kích hoạt được lưu trong JSON `data`.
 */
class RuleInputDevice : public RuleInput
{
private:
	Device *device;	  ///< Con trỏ tới đối tượng thiết bị liên quan đến đầu vào Rule này.
	Json::Value data; ///< Cấu hình chi tiết của điều kiện đầu vào thiết bị (ví dụ: trạng thái, giá trị ngưỡng...).

public:
	/**
	 * @brief Khởi tạo một RuleInputDevice mới.
	 * @param rule Con trỏ đến Rule cha mà đầu vào này thuộc về.
	 * @param device Con trỏ đến thiết bị được giám sát.
	 * @param data Cấu hình điều kiện đầu vào dạng JSON.
	 */
	RuleInputDevice(Rule *rule, Device *device, Json::Value &data);

	/**
	 * @brief Destructor.
	 */
	~RuleInputDevice();

	/**
	 * @brief Lấy dữ liệu cấu hình đầu vào của thiết bị.
	 * @return Con trỏ đến đối tượng Json::Value chứa cấu hình.
	 */
	Json::Value *GetData();

	/**
	 * @brief Kích hoạt (trigger) đầu vào Rule khi thiết bị thay đổi trạng thái.
	 * @param value Trạng thái mới của thiết bị (true = kích hoạt, false = tắt).
	 */
	void Trigger(bool value);

	/**
	 * @brief Cập nhật trạng thái hiện tại của thiết bị được theo dõi.
	 * @param status Trạng thái hiện tại (true nếu thiết bị đang hoạt động, false nếu không).
	 */
	void UpdateStatus(bool status);
};
