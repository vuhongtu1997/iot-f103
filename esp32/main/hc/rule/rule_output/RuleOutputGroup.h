/**
 * @file RuleOutputGroup.h
 * @brief Định nghĩa lớp RuleOutputGroup — lớp đầu ra (output) để điều khiển Group.
 *
 * RuleOutputGroup triển khai RuleOutput cho phép thực hiện hành động lên một Group
 * khi Rule được kích hoạt. Dữ liệu JSON chứa các thông số hành động cho Group.
 */

#pragma once

#include "json.h"
#include "Group.h"
#include "RuleOutput.h"

/**
 * @class RuleOutputGroup
 * @brief Lớp đại diện cho đầu ra Group của một Rule.
 *
 * Khi Rule được kích hoạt, RuleOutputGroup sẽ chạy hành động tương ứng trên Group
 * đã được liên kết.
 */
class RuleOutputGroup : public RuleOutput
{
private:
	Group *group;	  ///< Con trỏ tới Group mà đầu ra sẽ điều khiển.
	Json::Value data; ///< Dữ liệu cấu hình hành động.

public:
	/**
	 * @brief Constructor.
	 * @param group Con trỏ tới Group sẽ điều khiển.
	 * @param data Dữ liệu JSON chứa các thông số hành động.
	 */
	RuleOutputGroup(Group *group, Json::Value &data);

	/**
	 * @brief Destructor.
	 */
	~RuleOutputGroup();

	/**
	 * @brief Thực thi hành động của Rule trên Group.
	 *
	 * Khi Rule được kích hoạt, phương thức này sẽ áp dụng dữ liệu action lên Group.
	 */
	void RunOutput();
};
