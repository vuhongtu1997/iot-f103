/**
 * @file RuleOutputDelay.h
 * @brief Định nghĩa lớp RuleOutputDelay — lớp đầu ra thực hiện delay.
 *
 * RuleOutputDelay triển khai RuleOutput để thực hiện một khoảng thời gian trì hoãn
 * khi Rule được kích hoạt. Thường được dùng để trì hoãn các hành động khác.
 */

#pragma once

#include "Device.h"
#include "RuleOutput.h"

/**
 * @class RuleOutputDelay
 * @brief Lớp đại diện cho hành động trì hoãn (delay) khi Rule được kích hoạt.
 *
 * Khi Rule được kích hoạt, RuleOutputDelay sẽ thực hiện một khoảng thời gian delay
 * trước khi cho phép các hành động tiếp theo thực thi.
 */
class RuleOutputDelay : public RuleOutput
{
private:
	int delayTime; ///< Thời gian trì hoãn tính bằng milliseconds hoặc đơn vị được định nghĩa.

public:
	/**
	 * @brief Constructor.
	 * @param delayTime Thời gian trì hoãn.
	 */
	RuleOutputDelay(int delayTime);

	/**
	 * @brief Destructor.
	 */
	~RuleOutputDelay();

	/**
	 * @brief Thực thi hành động trì hoãn.
	 *
	 * Khi Rule được kích hoạt, phương thức này sẽ chặn hoặc trì hoãn việc thực thi tiếp theo
	 * trong khoảng thời gian delayTime.
	 */
	void RunOutput();
};
