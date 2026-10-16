/**
 * @file Config.h
 * @brief Lớp quản lý cấu hình kết nối MQTT cho thiết bị
 *
 * File này khai báo lớp `Config` — chịu trách nhiệm lưu trữ, đọc, ghi và
 * truy xuất các thông tin cấu hình MQTT bao gồm:
 * - Host, Port
 * - Client ID, Username, Password
 * - Keep Alive, SSL
 *
 * Lớp sử dụng mẫu Singleton để đảm bảo chỉ tồn tại một thể hiện duy nhất
 * trong toàn bộ hệ thống. Điều này giúp các module khác có thể truy cập
 * cấu hình chung một cách an toàn và nhất quán.
 *
 */

#pragma once

#include <stdint.h>
#include <string>

/**
 * @brief Kích thước tối đa cho các giá trị chuỗi trong cấu hình.
 */
#define STRING_VALUE_MAX_SIZE 128

//======================================================================
// Định nghĩa các khóa (key) và giá trị mặc định cho cấu hình MQTT
//======================================================================

/** @brief Khóa cho địa chỉ máy chủ MQTT */
#define HOST_KEY "host"
/** @brief Giá trị mặc định cho host */
#define HOST_DEFAULT "broker.hivemq.com"

/** @brief Khóa cho cổng MQTT */
#define PORT_KEY "port"
/** @brief Giá trị mặc định cho port */
#define PORT_DEFAULT 1883

/** @brief Khóa cho Client ID MQTT */
#define CLIENT_ID_KEY "client_id"
/** @brief Giá trị mặc định cho Client ID */
#define CLIENT_ID_DEFAULT ""

/** @brief Khóa cho tên đăng nhập MQTT */
#define USERNAME_KEY "username"
/** @brief Giá trị mặc định cho username */
#define USERNAME_DEFAULT ""

/** @brief Khóa cho mật khẩu MQTT */
#define PASSWORD_KEY "password"
/** @brief Giá trị mặc định cho password */
#define PASSWORD_DEFAULT ""

/** @brief Khóa cho khoảng thời gian keep-alive (giây) */
#define KEEP_ALIVE_KEY "keep_alive"
/** @brief Giá trị mặc định cho keep-alive */
#define KEEP_ALIVE_DEFAULT 120

/** @brief Khóa lưu JSON config provision đã convert */
#define HC_CFG_JSON_KEY "hc_cfg_json"
/** @brief Khóa lưu SSID Wi-Fi sau provision */
#define WIFI_SSID_KEY "wifi_ssid"
/** @brief Khóa lưu mật khẩu Wi-Fi sau provision */
#define WIFI_PASS_KEY "wifi_pass"
/** @brief Khóa lưu thing/home id */
#define THING_ID_KEY "thing_id"

//======================================================================
// Khai báo lớp Config
//======================================================================

/**
 * @class Config
 * @brief Lớp quản lý cấu hình ứng dụng (MQTT)
 *
 * Lớp `Config` cung cấp các phương thức để đọc và cập nhật cấu hình hệ thống.
 * Các trường bao gồm địa chỉ host, port, client ID, username, password,
 * thời gian keep-alive và cờ SSL.
 *
 * @note Lớp được cài đặt theo mẫu Singleton — chỉ có một thể hiện tồn tại.
 *        Gọi `Config::getInstance()` để truy cập.
 */
class Config
{
private:
	std::string host;	  /**< Địa chỉ host của MQTT broker */
	int port;			  /**< Cổng kết nối MQTT */
	std::string clientId; /**< Client ID của thiết bị */
	std::string username; /**< Tên đăng nhập MQTT (nếu có) */
	std::string password; /**< Mật khẩu MQTT (nếu có) */
	int keepAlive;		  /**< Khoảng thời gian keep-alive (giây) */
	bool ssl;			  /**< Bật/tắt kết nối SSL */

	/**
	 * @brief Hàm khởi tạo riêng tư (theo mẫu Singleton).
	 */
	Config();

	/**
	 * @brief Hàm hủy riêng tư (theo mẫu Singleton).
	 */
	~Config();

public:
	/**
	 * @brief Lấy thể hiện duy nhất của lớp Config.
	 *
	 * @return Con trỏ tới đối tượng Config duy nhất.
	 */
	static Config *getInstance();

	/**
	 * @brief Đọc cấu hình từ nơi lưu trữ (file, NVS, LittleFS...).
	 *
	 * Hàm này nạp giá trị các trường nội bộ từ nguồn lưu trữ.
	 * Cách đọc thực tế sẽ được định nghĩa trong phần triển khai (.cpp).
	 */
	void ReadConfig();

	/**
	 * @brief In cấu hình hiện tại ra console/log (dùng để debug).
	 */
	void Print();

	//==================================================================
	// Getter - truy xuất thông tin cấu hình
	//==================================================================

	/** @brief Lấy địa chỉ host MQTT */
	std::string GetHost();

	/** @brief Lấy số cổng kết nối */
	int GetPort();

	/** @brief Lấy Client ID */
	std::string GetClientId();

	/** @brief Lấy username */
	std::string GetUsername();

	/** @brief Lấy password */
	std::string GetPassword();

	/** @brief Lấy giá trị keep-alive (giây) */
	int GetKeepAlive();

	/** @brief Kiểm tra xem SSL có được bật hay không */
	bool GetSsl();

	//==================================================================
	// Setter - cập nhật thông tin cấu hình
	//==================================================================

	/**
	 * @brief Thiết lập host MQTT.
	 * @param host Tên miền hoặc IP của MQTT broker.
	 * @return true nếu cập nhật thành công.
	 */
	bool SetHost(std::string host);

	/**
	 * @brief Thiết lập cổng MQTT.
	 * @param port Số cổng (ví dụ: 1883 hoặc 8883 cho SSL).
	 * @return true nếu giá trị hợp lệ và được cập nhật.
	 */
	bool SetPort(int port);

	/**
	 * @brief Thiết lập Client ID.
	 * @param clientId ID định danh thiết bị MQTT.
	 * @return true nếu cập nhật thành công.
	 */
	bool SetClientId(std::string clientId);

	/**
	 * @brief Thiết lập username cho MQTT.
	 * @param username Tên đăng nhập MQTT.
	 * @return true nếu cập nhật thành công.
	 */
	bool SetUsername(std::string username);

	/**
	 * @brief Thiết lập password cho MQTT.
	 * @param password Mật khẩu MQTT.
	 * @return true nếu cập nhật thành công.
	 */
	bool SetPassword(std::string password);

	/**
	 * @brief Thiết lập giá trị keep-alive.
	 * @param keepAlive Thời gian keep-alive (giây).
	 * @return true nếu giá trị hợp lệ và được cập nhật.
	 */
	bool SetKeepAlive(int keepAlive);

	/**
	 * @brief Bật hoặc tắt SSL.
	 * @param ssl true để bật SSL, false để tắt.
	 * @return true nếu cập nhật thành công.
	 */
	bool SetSsl(bool ssl);

	/**
	 * @brief Lưu JSON cấu hình provision đã convert xuống NVS.
	 * @param json Chuỗi JSON phẳng chứa cấu hình gateway/wifi/home.
	 * @return true nếu lưu thành công.
	 */
	bool SaveProvisionConfigJson(const std::string &json);

	/**
	 * @brief Đọc lại JSON cấu hình provision từ NVS.
	 * @return Chuỗi JSON đã lưu, rỗng nếu chưa có hoặc đọc lỗi.
	 */
	std::string GetProvisionConfigJson();

	/**
	 * @brief Lưu SSID Wi-Fi sau provision.
	 */
	bool SetWifiSsid(const std::string &ssid);

	/**
	 * @brief Lưu mật khẩu Wi-Fi sau provision.
	 */
	bool SetWifiPassword(const std::string &password);

	/**
	 * @brief Lưu thing id / home id sau provision.
	 */
	bool SetThingId(const std::string &thingId);

	/**
	 * @brief Đọc SSID Wi-Fi đã lưu.
	 */
	std::string GetWifiSsid();

	/**
	 * @brief Đọc mật khẩu Wi-Fi đã lưu.
	 */
	std::string GetWifiPassword();

	/**
	 * @brief Đọc thing id đã lưu.
	 */
	std::string GetThingId();

	/**
	 * @brief Xóa toàn bộ dữ liệu provision khỏi NVS.
	 */
	bool ClearProvisioningData();
};
