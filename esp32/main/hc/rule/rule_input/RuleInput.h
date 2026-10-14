/**
 * @file RuleInput.h
 * @brief Định nghĩa lớp RuleInput — lớp cơ sở cho các loại đầu vào (input) của hệ thống Rule.
 *
 * RuleInput là lớp trừu tượng (abstract base class) đại diện cho một đầu vào
 * có thể kích hoạt Rule. Các lớp con sẽ triển khai chi tiết cách kiểm tra trạng thái
 * của đầu vào (ví dụ: Timer, Device, Group...).
 *
 * @note Lớp này chỉ định trạng thái khả dụng của đầu vào thông qua biến isAvailable.
 */

#pragma once

class Rule;

/**
 * @class RuleInput
 * @brief Lớp cơ sở trừu tượng cho các loại đầu vào Rule.
 *
 * RuleInput lưu trữ trạng thái khả dụng của đầu vào và con trỏ đến Rule mà nó
 * thuộc về. Các lớp con sẽ triển khai phương thức Check() để xác định điều kiện
 * kích hoạt Rule.
 */
class RuleInput
{
protected:
	bool isAvailable; ///< Trạng thái khả dụng của đầu vào (true = có thể kích hoạt Rule).
	Rule *rule;		  ///< Con trỏ tới Rule mà đầu vào này liên kết.

public:
	/**
	 * @brief Destructor ảo.
	 */
	virtual ~RuleInput() {}

	/**
	 * @brief Kiểm tra trạng thái hiện tại của đầu vào.
	 * @return true nếu đầu vào khả dụng, false nếu không.
	 * @note Các lớp con có thể override phương thức này để triển khai logic kiểm tra riêng.
	 */
	virtual bool Check() { return isAvailable; }
};
