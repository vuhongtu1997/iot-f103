#pragma once

/**
 * @brief Các mã lỗi dùng trong hệ thống.
 *
 * Mỗi giá trị của enum đại diện cho một tình trạng hoặc lỗi cụ thể.
 */
typedef enum
{
	CODE_ERROR = -1,		  /**< Lỗi chung không xác định */
	CODE_OK = 0,			  /**< Hoạt động thành công */
	CODE_NOT_RESPONSE,		  /**< Không có phản hồi */
	CODE_DATA_ARRAY,		  /**< Dữ liệu trả về là mảng */
	CODE_TIMEOUT,			  /**< Hết thời gian chờ */
	CODE_NOT_FOUND_DEVICE,	  /**< Không tìm thấy thiết bị */
	CODE_NOT_FOUND_GROUP,	  /**< Không tìm thấy nhóm */
	CODE_NOT_FOUND_SCENE,	  /**< Không tìm thấy cảnh (scene) */
	CODE_NOT_FOUND_RULE,	  /**< Không tìm thấy rule */
	CODE_NOT_FOUND_ROOM,	  /**< Không tìm thấy phòng */
	CODE_FORMAT_ERROR,		  /**< Lỗi định dạng dữ liệu */
	CODE_DATABASE_ERROR,	  /**< Lỗi cơ sở dữ liệu */
	CODE_MEMORY_ERROR,		  /**< Lỗi bộ nhớ */
	CODE_FILE_ERROR,		  /**< Lỗi tập tin */
	CODE_EXIT,				  /**< Lệnh thoát */
	CODE_REBOOT,			  /**< Lệnh khởi động lại thiết bị */
	CODE_FACTORY,			  /**< Khởi tạo về mặc định nhà máy */
	CODE_OTA_ERROR,			  /**< Lỗi cập nhật OTA */
	CODE_OTA_DATA_ERROR,	  /**< Lỗi dữ liệu OTA */
	CODE_OTA_CHECK_CRC_ERROR, /**< Lỗi kiểm tra CRC OTA */
} ErrorCode;