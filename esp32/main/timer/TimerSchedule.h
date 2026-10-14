/**
 * @file Timer.h
 * @brief Khai báo lớp Timer và TimerSchedule để quản lý và kích hoạt các sự kiện định kỳ trong hệ thống Rule Engine.
 *
 * Module này cho phép đăng ký các bộ hẹn giờ (timer) gắn với hàm callback hoặc đối tượng Rule.
 * Khi đến thời điểm định sẵn, Timer sẽ kích hoạt callback hoặc thực thi Rule tương ứng.
 */

#pragma once

#include "Rule.h"
#include <vector>
#include <mutex>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/**
 * @typedef TimerCallbackFunc
 * @brief Định nghĩa kiểu callback được gọi khi timer đến hạn.
 */
typedef std::function<void()> TimerCallbackFunc;

/**
 * @class Timer
 * @brief Đại diện cho một bộ hẹn giờ đơn lẻ.
 *
 * Mỗi Timer có thể liên kết với một callback hoặc một Rule.
 * Khi đến thời điểm được định nghĩa, Timer sẽ được kích hoạt thông qua TimerSchedule.
 */
class Timer
{
private:
	int index; /**< Mã định danh duy nhất của timer. */
	int time;  /**< Thời điểm hoặc chu kỳ kích hoạt (đơn vị giây hoặc phút tùy hệ thống). */

public:
	TimerCallbackFunc timerCallbackFunc; /**< Hàm callback được gọi khi timer đến hạn. */
	Rule *rule;							 /**< Con trỏ đến Rule liên kết (nếu có). */

	/**
	 * @brief Tạo một Timer gắn với callback.
	 *
	 * @param index              ID của timer.
	 * @param time               Thời điểm hoặc chu kỳ kích hoạt.
	 * @param timerCallbackFunc  Hàm callback được gọi khi timer đến hạn.
	 */
	Timer(int index, int time, TimerCallbackFunc timerCallbackFunc);

	/**
	 * @brief Tạo một Timer gắn với Rule.
	 *
	 * @param index ID của timer.
	 * @param time  Thời điểm hoặc chu kỳ kích hoạt.
	 * @param rule  Rule được thực thi khi timer đến hạn.
	 */
	Timer(int index, int time, Rule *rule);

	/**
	 * @brief Lấy ID của timer.
	 *
	 * @return ID của timer.
	 */
	int GetIndex();

	/**
	 * @brief Kiểm tra xem timer có khớp với thời gian hiện tại hay không.
	 *
	 * @param time Thời điểm hiện tại (theo cùng đơn vị với timer).
	 * @return true nếu đến thời điểm kích hoạt, ngược lại false.
	 */
	bool IsAtTime(int time);
};

/**
 * @class TimerSchedule
 * @brief Lớp quản lý và điều phối nhiều Timer trong hệ thống.
 *
 * Lớp này chịu trách nhiệm đăng ký, hủy và giám sát các timer.
 * Khi một timer đến hạn, callback hoặc rule tương ứng sẽ được thực thi.
 * Được triển khai theo mô hình singleton.
 */
class TimerSchedule
{
private:
	int index; /**< Bộ đếm ID cho timer tiếp theo. */

public:
	QueueHandle_t queue;			/**< Hàng đợi dùng để gửi sự kiện timer. */
	std::mutex mtx;					/**< Bảo vệ truy cập đồng thời vào danh sách timer. */
	std::vector<Timer *> timerList; /**< Danh sách tất cả timer đã đăng ký. */

	/**
	 * @brief Lấy thể hiện singleton của TimerSchedule.
	 *
	 * @return Con trỏ tới thể hiện duy nhất của TimerSchedule.
	 */
	static TimerSchedule *getInstance();

	/**
	 * @brief Hàm khởi tạo TimerSchedule.
	 */
	TimerSchedule();

	/**
	 * @brief Hàm hủy TimerSchedule, giải phóng bộ nhớ và tài nguyên.
	 */
	~TimerSchedule();

	/**
	 * @brief Khởi tạo hệ thống timer, tạo queue và task xử lý timer.
	 */
	void Init();

	/**
	 * @brief Đăng ký một timer dựa trên chuỗi thời gian.
	 *
	 * @param timer             Chuỗi biểu diễn thời gian (ví dụ "08:30").
	 * @param timerCallbackFunc Callback được gọi khi đến thời điểm này.
	 * @return ID của timer được tạo.
	 */
	int RegisterTimer(std::string timer, TimerCallbackFunc timerCallbackFunc);

	/**
	 * @brief Đăng ký một timer dựa trên giá trị thời gian số.
	 *
	 * @param time              Thời gian (giây hoặc phút).
	 * @param timerCallbackFunc Callback được gọi khi đến thời điểm này.
	 * @return ID của timer được tạo.
	 */
	int RegisterTimer(int time, TimerCallbackFunc timerCallbackFunc);

	/**
	 * @brief Đăng ký một timer gắn với Rule, sử dụng chuỗi thời gian.
	 *
	 * @param timer Chuỗi thời gian (ví dụ "23:45").
	 * @param rule  Rule được thực thi khi đến thời điểm này.
	 * @return ID của timer được tạo.
	 */
	int RegisterTimer(std::string timer, Rule *rule);

	/**
	 * @brief Đăng ký một timer gắn với Rule, sử dụng giá trị thời gian số.
	 *
	 * @param time Thời gian (giây hoặc phút).
	 * @param rule Rule được thực thi khi đến thời điểm này.
	 * @return ID của timer được tạo.
	 */
	int RegisterTimer(int time, Rule *rule);

	/**
	 * @brief Hủy đăng ký một timer theo ID.
	 *
	 * @param index ID của timer cần hủy.
	 * @return 0 nếu thành công, -1 nếu không tìm thấy.
	 */
	int UnregisterTimer(int index);
};
