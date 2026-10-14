/**
 * @file RuleOutputDevice.h
 * @brief Định nghĩa lớp RuleOutputDevice — lớp đầu ra (output) để điều khiển thiết bị.
 *
 * RuleOutputDevice triển khai RuleOutput cho phép thực hiện hành động lên một Device cụ thể
 * khi Rule được kích hoạt. Dữ liệu JSON chứa các thông số hành động cho thiết bị.
 */

#pragma once

#include "json.h"
#include "Device.h"
#include "RuleOutput.h"

/**
 * @class RuleOutputDevice
 * @brief Lớp đại diện cho đầu ra thiết bị của một Rule.
 *
 * Khi Rule được kích hoạt, RuleOutputDevice sẽ chạy hành động tương ứng trên Device cụ thể.
 */
class RuleOutputDevice : public RuleOutput
{
private:
	Device *device;	  ///< Con trỏ tới thiết bị mà hành động sẽ được áp dụng.
	Json::Value data; ///< Dữ liệu JSON chứa các thông số hành động cho thiết bị.

public:
	/**
	 * @brief Constructor.
	 * @param device Con trỏ tới thiết bị.
	 * @param data Dữ liệu JSON chứa các thông số hành động.
	 */
	RuleOutputDevice(Device *device, Json::Value &data);

	/**
	 * @brief Destructor.
	 */
	~RuleOutputDevice();

	/**
	 * @brief Thực thi hành động của Rule trên Device.
	 *
	 * Khi Rule được kích hoạt, phương thức này sẽ áp dụng dữ liệu action lên thiết bị.
	 */
	void RunOutput();
};
