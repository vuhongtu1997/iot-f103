/**
 * @file Ota.h
 * @brief Khai báo các hàm xử lý OTA (Over-The-Air firmware update).
 *
 * Module này quản lý quá trình cập nhật firmware từ xa thông qua URL tải file,
 * hoặc theo kiểu phân mảnh (chunk) – phù hợp với thiết bị hạn chế bộ nhớ.
 *
 * Các phương thức hỗ trợ:
 *  - Kiểm tra phiên bản mới.
 *  - Cập nhật firmware trực tiếp qua URL.
 *  - Cập nhật theo dạng truyền từng chunk (dùng cho MQTT hoặc BLE).
 *  - Kiểm tra checksum trước khi hoàn tất cập nhật.
 *
 * Yêu cầu:
 *  - Firmware phải đảm bảo đủ bộ nhớ flash.
 *  - Quá trình cập nhật phải đảm bảo tính toàn vẹn (checksum + thuật toán SHA256…).
 *  - Sau khi `UpdateFinish()` thành công, thiết bị cần restart.
 */

#pragma once

#include <string>

using namespace std;

/**
 * @namespace Ota
 * @brief Chứa các hàm điều khiển quá trình OTA firmware.
 */
namespace Ota
{
	/**
	 * @brief Kiểm tra có cần cập nhật firmware hay không.
	 *
	 * @param fwVersion Phiên bản firmware mới nhận được từ cloud/app.
	 * @return true nếu phiên bản mới khác phiên bản hiện tại và cần update.
	 * @return false nếu phiên bản giống nhau hoặc cũ hơn.
	 */
	bool CheckNeedUpdate(string fwVersion);

	/**
	 * @brief Cập nhật firmware trực tiếp bằng cách tải qua URL.
	 *
	 * @param fwUrl URL trỏ đến file firmware (hỗ trợ HTTP/HTTPS).
	 * @param fwChecksumAlgorithm Thuật toán kiểm tra checksum (vd: "SHA256").
	 * @param fwChecksum Chuỗi checksum hợp lệ tương ứng với firmware tải về.
	 * @return 0 nếu thành công, mã lỗi khác nếu thất bại.
	 *
	 * Lưu ý: Thiết bị sẽ tự động tải firmware, kiểm tra checksum và ghi flash.
	 * Nếu thành công, thiết bị cần tự restart sau quá trình update.
	 */
	int Update(string fwUrl, string fwChecksumAlgorithm, string fwChecksum);

	/**
	 * @brief Khởi tạo quá trình cập nhật OTA theo dạng chunk (phân đoạn).
	 *
	 * @param fwSize Tổng kích thước firmware.
	 * @param fwChecksumAlgorithm Thuật toán checksum (vd: "SHA256").
	 * @param fwChecksum Giá trị checksum chính xác của toàn bộ firmware.
	 * @return 0 nếu khởi tạo thành công, mã lỗi khác nếu khởi tạo thất bại.
	 *
	 * Áp dụng trong trường hợp cập nhật qua MQTT hoặc BLE, khi không tải trực tiếp được.
	 */
	int Update(int fwSize, string fwChecksumAlgorithm, string fwChecksum);

	/**
	 * @brief Gửi từng đoạn firmware vào thiết bị.
	 *
	 * @param chunkId ID của gói phân đoạn (từ 0 đến N).
	 * @param data Buffer chứa dữ liệu firmware.
	 * @param dataLen Độ dài dữ liệu.
	 * @return 0 nếu ghi thành công, mã lỗi khác nếu lỗi.
	 *
	 * Lưu ý: Cần truyền đúng thứ tự chunk để tránh lỗi ghi sai vị trí flash.
	 */
	int UpdateChunk(int chunkId, uint8_t *data, int dataLen);

	/**
	 * @brief Hoàn tất quá trình cập nhật OTA dạng chunk.
	 *
	 * @return 0 nếu hoàn tất thành công và checksum hợp lệ.
	 *         Mã lỗi khác nếu thất bại.
	 *
	 * Sau khi hoàn tất, hệ thống nên reboot để firmware mới có hiệu lực.
	 */
	int UpdateFinish();
}
