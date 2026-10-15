/**
 * @file RuleOutput.h
 * @brief Định nghĩa lớp cơ sở RuleOutput cho các hành động đầu ra của Rule.
 *
 * RuleOutput là lớp trừu tượng cơ sở cho các hành động được thực thi khi Rule được kích hoạt.
 * Các lớp dẫn xuất sẽ triển khai phương thức RunOutput để thực hiện hành động cụ thể
 * trên thiết bị hoặc gateway.
 */

#pragma once

/**
 * @class RuleOutput
 * @brief Lớp cơ sở cho các hành động đầu ra của Rule.
 *
 * RuleOutput định nghĩa giao diện chung cho các hành động đầu ra khi Rule được kích hoạt.
 * Lớp này chỉ định nghĩa phương thức ảo RunOutput, các lớp con sẽ triển khai chi tiết.
 */
class RuleOutput
{
protected:
	// Có thể thêm các thành viên chung cho tất cả các hành động đầu ra nếu cần.

public:
	/**
	 * @brief Destructor ảo.
	 */
	virtual ~RuleOutput() {}

	/**
	 * @brief Thực thi hành động đầu ra.
	 *
	 * Phương thức này được các lớp dẫn xuất override để thực hiện hành động cụ thể
	 * khi Rule được kích hoạt.
	 */
	virtual void RunOutput() {}
};
