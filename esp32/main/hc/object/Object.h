/**
 * @file Object.h
 * @brief Định nghĩa lớp cơ sở `Object` dùng chung cho các đối tượng trong hệ thống (ví dụ: thiết bị, nhóm...).
 *
 * @details
 * Lớp `Object` lưu trữ các thông tin cơ bản như:
 * - `id`: định danh duy nhất của đối tượng (thường là chuỗi UUID hoặc mã thiết bị).
 * - `name`: tên hiển thị của đối tượng.
 * - `addr`: địa chỉ định danh (có thể là địa chỉ mạng, địa chỉ BLE Mesh hoặc nhóm).
 *
 * Lớp này cung cấp các phương thức getter/setter để truy cập và thay đổi thông tin đối tượng.
 *
 * @note
 * Đây là lớp cơ sở trừu tượng, thường được kế thừa bởi các lớp khác như `Device`, `Group`, v.v.
 */

#pragma once

#include <string>
#include <stdint.h>

/**
 * @class Object
 * @brief Lớp cơ sở lưu trữ thông tin định danh và tên của một đối tượng trong hệ thống.
 */
class Object
{
protected:
	std::string id;	  ///< ID duy nhất của đối tượng
	std::string name; ///< Tên của đối tượng
	uint16_t addr;	  ///< Địa chỉ định danh (ví dụ: địa chỉ nhóm hoặc thiết bị)

public:
	/**
	 * @brief Khởi tạo một đối tượng Object với ID, địa chỉ và tên.
	 * @param id     ID của đối tượng
	 * @param addr   Địa chỉ (uint16_t)
	 * @param name   Tên đối tượng
	 */
	Object(std::string id, uint16_t addr, std::string name);

	/// @brief Destructor.
	~Object();

	/// @brief Lấy ID của đối tượng.
	std::string GetId();

	/// @brief Gán ID mới cho đối tượng.
	void SetId(std::string id);

	/// @brief Lấy tên đối tượng.
	std::string GetName();

	/// @brief Gán tên mới cho đối tượng.
	void SetName(std::string name);

	/// @brief Lấy địa chỉ đối tượng.
	uint16_t GetAddr();

	/// @brief Gán địa chỉ mới cho đối tượng.
	void SetAddr(uint16_t addr);
};
