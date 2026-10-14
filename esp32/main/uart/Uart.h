/**
 * @file Uart.h
 * @brief Định nghĩa lớp Uart dùng để quản lý giao tiếp UART trên nền ESP-IDF.
 *
 * Lớp này đóng vai trò là trình bao (wrapper) cho driver UART của ESP-IDF,
 * hỗ trợ khởi tạo, cấu hình, thay đổi baudrate, đọc/ghi dữ liệu và xử lý callback khi nhận dữ liệu.
 *
 * Ngoài ra, lớp Uart có thể được kế thừa để định nghĩa hành vi riêng trong hàm `OnMessage()`
 * khi nhận được dữ liệu qua UART.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

/**
 * @class Uart
 * @brief Lớp quản lý giao tiếp UART trên ESP32.
 *
 * Cung cấp các phương thức khởi tạo, gửi dữ liệu và xử lý sự kiện UART.
 * Hỗ trợ queue để nhận sự kiện từ UART driver và callback tùy chỉnh.
 */
class Uart
{
private:
	uart_port_t num; /**< Số hiệu cổng UART (UART_NUM_0, UART_NUM_1, UART_NUM_2). */
	int txPin;		 /**< GPIO sử dụng làm TX. */
	int rxPin;		 /**< GPIO sử dụng làm RX. */
	int baudrate;	 /**< Tốc độ baud hiện tại. */

	QueueHandle_t queue; /**< Hàng đợi chứa sự kiện UART từ driver ESP-IDF. */
	bool initialized; /**< True nếu UART driver đã khởi tạo thành công. */

public:
	/**
	 * @brief Hàm khởi tạo đối tượng UART.
	 *
	 * @param num      Số hiệu UART (UART_NUM_0, UART_NUM_1, UART_NUM_2).
	 * @param txPin    GPIO sử dụng cho TX.
	 * @param rxPin    GPIO sử dụng cho RX.
	 * @param baudrate Tốc độ baud mong muốn.
	 */
	Uart(uart_port_t num, int txPin, int rxPin, int baudrate);

	/**
	 * @brief Lấy số hiệu UART hiện tại.
	 *
	 * @return uart_port_t — Số UART.
	 */
	uart_port_t GetNum();

	/**
	 * @brief Lấy hàng đợi sự kiện UART.
	 *
	 * @return QueueHandle_t — Con trỏ đến queue chứa sự kiện UART.
	 */
	QueueHandle_t GetQueue();

	/**
	 * @brief Khởi tạo UART với cấu hình mặc định.
	 *
	 * Thiết lập driver UART, cấu hình GPIO TX/RX, và tạo hàng đợi sự kiện.
	 */
	void Init();

	/**
	 * @brief Mở UART với baudrate được chỉ định.
	 *
	 * @param baudrate Tốc độ baud cần thiết lập.
	 * @return 0 nếu thành công, giá trị âm nếu thất bại.
	 */
	int Open(int baudrate);

	/**
	 * @brief Đóng UART và giải phóng driver.
	 *
	 * @return 0 nếu thành công, giá trị âm nếu thất bại.
	 */
	int Close();

	/**
	 * @brief Thay đổi tốc độ baud động trong quá trình hoạt động.
	 *
	 * @param baudrate Tốc độ baud mới.
	 * @return 0 nếu thành công, giá trị âm nếu thất bại.
	 */
	int ChangeBaudrate(int baudrate);

	/**
	 * @brief Gửi dữ liệu qua UART.
	 *
	 * @param buf   Con trỏ tới buffer chứa dữ liệu.
	 * @param count Số byte cần gửi.
	 * @return Số byte đã gửi hoặc giá trị âm nếu thất bại.
	 */
	ssize_t Write(const void *buf, size_t count);

	/**
	 * @brief Hàm callback khi nhận được dữ liệu UART.
	 *
	 * Hàm này có thể được ghi đè (override) trong lớp kế thừa để xử lý nội dung cụ thể.
	 *
	 * @param data Con trỏ tới buffer chứa dữ liệu nhận được.
	 * @param len  Độ dài dữ liệu nhận được.
	 * @return 0 nếu xử lý thành công, giá trị âm nếu lỗi.
	 */
	virtual int OnMessage(unsigned char *data, int len);
};
