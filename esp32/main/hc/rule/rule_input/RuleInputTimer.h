/**
 * @file RuleInputTimer.h
 * @brief Định nghĩa lớp RuleInputTimer — đầu vào (input) dạng hẹn giờ cho hệ thống Rule.
 *
 * Lớp này kế thừa từ RuleInput, đại diện cho một đầu vào kiểu “Timer” trong cơ chế Rule.
 * Nó được sử dụng để kích hoạt một Rule dựa trên khoảng thời gian định sẵn, có thể lặp lại
 * hoặc chỉ chạy một lần.
 *
 * @note RuleInputTimer thường được sử dụng cho các tự động hóa định kỳ hoặc tác vụ hẹn giờ.
 */

#pragma once

#include "RuleInput.h"
#include "Rule.h"

using namespace std;

/**
 * @class RuleInputTimer
 * @brief Lớp đại diện cho đầu vào dạng hẹn giờ (Timer) trong hệ thống Rule.
 *
 * Lớp này cho phép thiết lập thời gian kích hoạt và số lần lặp lại,
 * giúp Rule có thể được thực thi tự động theo chu kỳ hoặc một lần duy nhất.
 */
class RuleInputTimer : public RuleInput
{
private:
	int timer;				///< Thời gian (đơn vị: giây hoặc mili-giây tùy hệ thống) trước khi kích hoạt Rule.
	int repeat;				///< Số lần lặp lại của bộ hẹn giờ (0 nghĩa là chỉ chạy một lần).
	int timerRegisterIndex; ///< Chỉ số đăng ký trong bảng quản lý timer nội bộ.
	bool isRunOneTime;		///< Cờ xác định Rule chỉ được kích hoạt một lần duy nhất.

public:
	/**
	 * @brief Khởi tạo một RuleInputTimer mới.
	 * @param rule Con trỏ đến Rule cha mà timer này thuộc về.
	 * @param timer Thời gian kích hoạt tính theo đơn vị cấu hình.
	 * @param repeat Số lần lặp lại (0 = chỉ một lần, >0 = lặp lại).
	 */
	RuleInputTimer(Rule *rule, int timer, int repeat);

	/**
	 * @brief Destructor.
	 */
	~RuleInputTimer();

	/**
	 * @brief Kiểm tra điều kiện kích hoạt Rule dựa trên bộ đếm thời gian.
	 * @return true nếu đến thời điểm kích hoạt Rule, false nếu chưa.
	 */
	bool Check();
};
