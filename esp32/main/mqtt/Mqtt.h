/**
 * @file Mqtt.h
 * @brief Khai báo lớp Mqtt quản lý kết nối, subscribe/publish và callback sự kiện MQTT.
 *
 * Lớp này đóng vai trò như một lớp trừu tượng bao bọc (wrapper) cho MQTT client
 * của ESP-IDF (`esp_mqtt_client_handle_t`), cho phép:
 * - Kết nối và ngắt kết nối với MQTT broker.
 * - Quản lý danh sách topic subscribe.
 * - Gửi và nhận dữ liệu qua các topic.
 * - Đăng ký callback để xử lý sự kiện MQTT và dữ liệu nhận được.
 */

#pragma once

#include "esp_event.h"
#include "mqtt_client.h"
#include "ActionCallback.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * @class Mqtt
 * @brief Lớp quản lý kết nối MQTT và xử lý các sự kiện liên quan.
 *
 * Lớp `Mqtt` cung cấp API cấp cao để giao tiếp với MQTT broker
 * bao gồm các chức năng kết nối, subscribe, publish, và quản lý callback.
 */
class Mqtt
{
private:
	std::string host;				 /**< Địa chỉ MQTT broker. */
	int port;						 /**< Cổng MQTT broker. */
	std::string clientId;			 /**< Client ID dùng để xác định client với broker. */
	std::string username;			 /**< Tên đăng nhập MQTT. */
	std::string password;			 /**< Mật khẩu MQTT. */
	int keepalive;					 /**< Thời gian giữ kết nối (giây). */
	bool useTls;					 /**< Sử dụng TLS bảo mật nếu true. */
	std::string willsetTopic;		 /**< Topic dùng cho will message khi client mất kết nối. */
	std::string willsetPayload;		 /**< Nội dung will message. */
protected:
	int connected; /**< Trạng thái kết nối MQTT: 1 = đã kết nối, 0 = chưa. */
	esp_mqtt_client_handle_t client; /**< Handle client MQTT của ESP-IDF. */
	QueueHandle_t messageQueue; /**< Hàng đợi tách xử lý message khỏi mqtt_task. */
	TaskHandle_t messageTaskHandle; /**< Worker task xử lý message MQTT. */
	void EnsureMessageWorker();

	std::vector<std::string> topicList;			 /**< Danh sách các topic đã subscribe. */
	std::vector<ActionCallback> actionCallbacks; /**< Danh sách các callback xử lý dữ liệu theo topic. */

	virtual bool ShouldDropBeforeQueue(const std::string &topic, const char *payload, int payloadLen) { return false; }
	virtual bool ShouldIgnoreIncomingMessage(const std::string &topic, const std::string &payload) { return false; }

public:
	/**
	 * @brief Khởi tạo một đối tượng Mqtt với thông tin cấu hình cơ bản.
	 *
	 * @param host        Địa chỉ MQTT broker.
	 * @param port        Cổng kết nối MQTT.
	 * @param clientId    Client ID.
	 * @param username    Tên người dùng.
	 * @param password    Mật khẩu.
	 * @param keepalive   Thời gian giữ kết nối (giây).
	 * @param useTls      Sử dụng TLS hay không (mặc định = false).
	 */
	Mqtt(std::string host, int port, std::string clientId, std::string username, std::string password, int keepalive, bool useTls = false);

	/**
	 * @brief Khởi tạo client MQTT nội bộ.
	 *
	 * Cấu hình và chuẩn bị MQTT client để kết nối với broker.
	 */
	void Init();

	/**
	 * @brief Cấu hình will message cho MQTT client.
	 *
	 * @param willset_topic   Topic sẽ gửi will message.
	 * @param willset_payload Nội dung của will message.
	 */
	void SetWillset(std::string willset_topic, std::string willset_payload);

	/**
	 * @brief Kết nối đến MQTT broker.
	 *
	 * @param timeout  Thời gian chờ kết nối (giây).
	 * @return 0 nếu thành công, khác 0 nếu thất bại.
	 */
	int Connect(int timeout = 10);

	/**
	 * @brief Buộc MQTT tạo lại kết nối TCP trên interface mạng hiện tại.
	 */
	int Reconnect();

	/**
	 * @brief Callback khi client kết nối thành công hoặc reconnect.
	 *
	 * @param isConnected  True nếu là kết nối lần đầu, false nếu reconnect.
	 * @param isReconnect  Cờ cho biết có phải reconnect hay không.
	 */
	virtual void OnConnect(bool isConnected, bool isReconnect) {}

	/**
	 * @brief Subscribe đến một topic cụ thể.
	 *
	 * @param topic Tên topic cần subscribe.
	 * @return ID của subscription hoặc lỗi nếu thất bại.
	 */
	int Subscribe(std::string topic);

	/**
	 * @brief Hủy đăng ký (unsubscribe) một topic.
	 *
	 * @param topic Tên topic cần hủy đăng ký.
	 * @return ID của yêu cầu unsubscribe hoặc lỗi.
	 */
	int Unsubscribe(std::string topic);

	/**
	 * @brief Gửi dữ liệu dạng chuỗi đến một topic.
	 *
	 * @param topic   Tên topic cần publish.
	 * @param payload Nội dung chuỗi gửi.
	 * @return ID của message hoặc lỗi nếu thất bại.
	 */
	int Publish(std::string topic, std::string payload, int qos = 0);

	/**
	 * @brief Gửi dữ liệu nhị phân đến một topic.
	 *
	 * @param topic      Tên topic cần publish.
	 * @param payload    Dữ liệu dạng con trỏ.
	 * @param payloadLen Độ dài dữ liệu.
	 * @return ID của message hoặc lỗi nếu thất bại.
	 */
	int Publish(std::string topic, char *payload, int payloadLen);

	/**
	 * @brief Kiểm tra trạng thái kết nối MQTT.
	 *
	 * @return true nếu đang kết nối, false nếu không.
	 */
	bool IsConnected();

	/**
	 * @brief Subscribe lại toàn bộ topic sau khi reconnect.
	 */
	void ResubcribeAllTopic();

	/**
	 * @brief Đăng ký callback xử lý cho một topic (với 4 kiểu callback hỗ trợ).
	 *
	 * Các hàm này cho phép đăng ký hàm xử lý tương ứng với topic cụ thể.
	 */
	void addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, std::string topic);
	void addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, std::string topic);
	void addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, std::string topic);
	void addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, std::string topic);

	/**
	 * @brief Tìm callback phù hợp với một topic nhận được.
	 *
	 * @param topic          Tên topic cần tìm.
	 * @param actionCallback Con trỏ kết quả trả về callback tương ứng (nếu có).
	 * @return 0 nếu tìm thấy, -1 nếu không.
	 */
	int findActionCallbackFuncFromTopic(std::string topic, ActionCallback **actionCallback);

	/**
	 * @brief Callback nội bộ khi client kết nối thành công.
	 *
	 * @param event Thông tin sự kiện MQTT.
	 */
	void on_connected(esp_mqtt_event_handle_t const event);

	/**
	 * @brief Callback nội bộ khi client mất kết nối.
	 *
	 * @param event Thông tin sự kiện MQTT.
	 */
	void on_disconnected(esp_mqtt_event_handle_t const event);

	/**
	 * @brief Callback khi subscribe thành công.
	 *
	 * @param event Thông tin sự kiện MQTT.
	 */
	void on_subscribed(esp_mqtt_event_handle_t const event);

	/**
	 * @brief Callback khi unsubscribe thành công.
	 *
	 * @param event Thông tin sự kiện MQTT.
	 */
	void on_unsubscribed(esp_mqtt_event_handle_t const event);

	/**
	 * @brief Callback khi có dữ liệu được publish đến topic đã subscribe.
	 *
	 * @param event Thông tin sự kiện MQTT chứa dữ liệu.
	 */
	void on_data(esp_mqtt_event_handle_t const event);
	void DispatchMessage(std::string topic, std::string payload);

	/* ----- Getters ----- */
	string GetHost();
	int GetPort();
	string GetClientId();
	string GetUsername();
	string GetPassword();
	int GetKeepalive();
	bool GetUseTls();

	/* ----- Setters ----- */
	void SetHost(string host);
	void SetPort(int port);
	void SetClientId(string clientId);
	void SetUsername(string username);
	void SetPassword(string password);
	void SetKeepalive(int keepalive);
	void SetUseTls(bool useTls);	
};

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
