/**
 * @file Util.h
 * @brief Cung cấp các hàm tiện ích (utility functions) dùng chung cho toàn hệ thống.
 *
 * File này bao gồm các hàm xử lý thời gian, chuỗi, JSON, LED trạng thái, và các thao tác hệ thống.
 * Các hàm này hỗ trợ cả môi trường ESP32 (FreeRTOS) và Linux/OpenWRT.
 */

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include "json.h"
#include "ErrorCode.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
/// Macro ngủ theo mili-giây trong môi trường FreeRTOS
#define SLEEP_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#else
/// Macro ngủ theo mili-giây trong môi trường Linux hoặc OpenWRT
#define SLEEP_MS(ms) usleep(ms * 1000)
#endif

/**
 * @namespace Util
 * @brief Chứa các hàm tiện ích dùng chung trong hệ thống.
 */
namespace Util
{
	/**
	 * @brief Sinh random chuỗi ký tự có độ dài chỉ định.
	 *
	 * @param size
	 * @return string
	 */
	std::string GenRandRQI(int size);

	/**
	 * @brief Lấy số giây hiện tại trong ngày (0–86399).
	 */
	int GetCurrentTimeInDay();

	/**
	 * @brief Lấy năm hiện tại (ví dụ: 2025).
	 */
	int GetYearsCurrent();

	/**
	 * @brief Lấy tháng hiện tại (1–12).
	 */
	int GetMonthsCurrent();

	/**
	 * @brief Lấy ngày trong tháng hiện tại (1–31).
	 */
	int GetDateCurrent();

	/**
	 * @brief Lấy thứ trong tuần hiện tại (0 = Chủ nhật, 1–6 = Thứ hai đến Thứ bảy).
	 */
	int GetDaysCurrent();

	/**
	 * @brief Lấy giờ hiện tại (0–23).
	 */
	int GetHoursCurrent();

	/**
	 * @brief Lấy phút hiện tại (0–59).
	 */
	int GetMinutesCurrent();

	/**
	 * @brief Lấy giây hiện tại (0–59).
	 */
	int GetSecondsCurrent();

	/**
	 * @brief Lấy thời gian hiện tại tính bằng mili-giây kể từ epoch.
	 * @return Thời gian hiện tại (uint64_t).
	 */
	uint64_t GetTimeMillis();

	/**
	 * @brief Lấy chuỗi thời gian hiện tại ở định dạng "YYYY-MM-DD HH:MM:SS".
	 */
	std::string GetCurrentTimeStr();

	/**
	 * @brief Kiểm tra xem một ngày có nằm trong cấu hình tuần (repeater bitmask) hay không.
	 * @param day Thứ trong tuần (0–6).
	 * @param repeater Bitmask của ngày lặp lại.
	 * @return true nếu nằm trong repeater, ngược lại false.
	 */
	int CheckDayInWeek(int day, int repeater);

	/**
	 * @brief Lấy thứ trong tuần hiện tại.
	 */
	int GetCurrentWeekDay();

	/**
	 * @brief Chuyển chuỗi thời gian "HH:MM" thành số giây trong ngày.
	 */
	int ConvertStrTimeToInt(std::string time);

	/**
	 * @brief Kiểm tra thiết bị có module RTC hoạt động hay không.
	 */
	bool HaveRTC();

	/**
	 * @brief Chuyển dữ liệu nhị phân sang chuỗi hex.
	 * @param data Dữ liệu đầu vào.
	 * @param len Độ dài dữ liệu.
	 * @return Chuỗi hex biểu diễn dữ liệu.
	 */
	std::string ConvertU32ToHexString(uint8_t *data, int len);

	/**
	 * @brief So sánh 2 hoặc 3 số nguyên theo phép toán chỉ định.
	 * @param op Toán tử so sánh ("<", ">", "==", "between", ...).
	 * @param a Số thứ nhất.
	 * @param b Số thứ hai.
	 * @param c Số thứ ba (tuỳ chọn).
	 * @return true nếu điều kiện đúng.
	 */
	bool CompareNumber(std::string op, int a, int b, int c = 0);

	/**
	 * @brief Thực thi lệnh hệ thống (Linux/OpenWRT).
	 * @param command Lệnh cần thực thi.
	 * @return Chuỗi kết quả stdout.
	 */
	std::string ExecuteCMD(std::string command);

	/**
	 * @brief Tương tự ExecuteCMD, nhận tham số kiểu const char*.
	 */
	std::string ExecuteCMD(char const *command);

	// ---------------------- Điều khiển LED trạng thái ----------------------

	/**
	 * @brief Bật/tắt đèn LED Internet.
	 */
	void LedInternet(bool value);

	/**
	 * @brief Bật/tắt đèn LED Service.
	 */
	void LedService(bool value);

	/**
	 * @brief Bật/tắt đèn LED BLE.
	 */
	void LedBle(bool value);

	/**
	 * @brief Bật/tắt toàn bộ LED trên thiết bị.
	 */
	void LedAll(bool value);

	/**
	 * @brief Khôi phục trạng thái LED trước đó.
	 */
	void LedRestoreLastValue();

	/**
	 * @brief Khoá điều khiển LED Service (ngăn LED thay đổi trong quá trình xử lý).
	 */
	void LedServiceLock();

	/**
	 * @brief Mở khoá điều khiển LED Service.
	 */
	void LedServiceUnlock();

	/**
	 * @brief Lấy trạng thái hiện tại của LED BLE.
	 */
	bool GetStatusLedBle();

	/**
	 * @brief Lấy trạng thái hiện tại của LED Service.
	 */
	bool GetStatusLedService();

	/**
	 * @brief Lấy trạng thái hiện tại của LED Internet.
	 */
	bool GetStatusLedInternet();

	// ---------------------- Hàm xử lý chuỗi & JSON ----------------------

	/**
	 * @brief Tách chuỗi thành các phần nhỏ dựa theo ký tự phân cách.
	 * @param str Chuỗi đầu vào.
	 * @param splitter Ký tự phân tách.
	 * @return Vector chứa các phần tử tách ra.
	 */
	std::vector<std::string> splitString(const std::string &str, char splitter);

	/**
	 * @brief Tạo std::string từ con trỏ const char*.
	 */
	std::string setString(const char *value);

	/**
	 * @brief Chuyển UUID (16 byte) thành chuỗi định dạng.
	 */
	std::string uuidToStr(uint8_t *uuid);

	/**
	 * @brief So sánh hai đối tượng JSON theo trường ID.
	 * @return true nếu obj1.ID < obj2.ID.
	 */
	bool compareByID(const Json::Value &obj1, const Json::Value &obj2);

	/**
	 * @brief Sắp xếp các phần tử JSON theo ID.
	 */
	Json::Value arrangeJson(Json::Value &obj);

#ifndef ESP_PLATFORM
	// ---------------------- Hàm bảo mật (chỉ dùng cho Linux/OpenWRT) ----------------------

	/**
	 * @brief Mã hoá AES-128.
	 * @param key Khoá mã hoá.
	 * @param plaintext Dữ liệu gốc.
	 * @return Chuỗi mã hoá.
	 */
	std::string encryptAes128(std::string key, std::string plaintext);

	/**
	 * @brief Tính checksum SHA-256 của file.
	 * @param filePath Đường dẫn file.
	 * @return Chuỗi hash SHA-256 dạng hex.
	 */
	std::string calculateSHA256Checksum(std::string &filePath);
#endif
}
