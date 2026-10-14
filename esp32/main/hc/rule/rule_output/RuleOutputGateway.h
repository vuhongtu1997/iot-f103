/**
 * @file RuleOutputGateway.h
 * @brief Định nghĩa lớp RuleOutputGateway — lớp đầu ra (output) để điều khiển Gateway.
 *
 * RuleOutputGateway triển khai RuleOutput cho phép thực hiện hành động lên Gateway
 * khi Rule được kích hoạt. Dữ liệu JSON chứa các thông số hành động cho Gateway.
 */

#pragma once

#include "json.h"
#include "RuleOutput.h"

/**
 * @class RuleOutputGateway
 * @brief Lớp đại diện cho đầu ra Gateway của một Rule.
 *
 * Khi Rule được kích hoạt, RuleOutputGateway sẽ chạy hành động tương ứng trên Gateway.
 */
class RuleOutputGateway : public RuleOutput
{
private:
	Json::Value data; ///< Dữ liệu JSON chứa các thông số hành động cho Gateway.

public:
	/**
	 * @brief Constructor.
	 * @param data Dữ liệu JSON chứa các thông số hành động.
	 */
	RuleOutputGateway(Json::Value &data);

	/**
	 * @brief Destructor.
	 */
	~RuleOutputGateway();

	/**
	 * @brief Thực thi hành động của Rule trên Gateway.
	 *
	 * Khi Rule được kích hoạt, phương thức này sẽ áp dụng dữ liệu action lên Gateway.
	 */
	void RunOutput();
};
