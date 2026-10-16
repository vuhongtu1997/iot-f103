#include "Mqtt.h"
#include "Log.h"
#include "esp_heap_caps.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mbedtls/error.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace
{
	constexpr UBaseType_t kMqttQueueLength = 128;
	constexpr uint32_t kMqttWorkerStackSize = 28 * 1024;
	constexpr uint32_t kMqttQueueWarnThreshold = 96;
	constexpr uint32_t kMqttWorkerStackWarnBytes = 3 * 1024;
	constexpr size_t kMqttPayloadLogLimit = 512;

	struct queued_mqtt_message_t
	{
		Mqtt *mqtt;
		std::string topic;
		std::string payload;
	};

	struct mqtt_worker_ctx_t
	{
		Mqtt *mqtt;
		QueueHandle_t queue;
	};

	static void mqtt_message_worker(void *arg)
	{
		mqtt_worker_ctx_t *ctx = static_cast<mqtt_worker_ctx_t *>(arg);
		if (!ctx || !ctx->mqtt || !ctx->queue)
		{
			delete ctx;
			vTaskDelete(NULL);
			return;
		}

		while (true)
		{
			queued_mqtt_message_t *message = nullptr;
			if (xQueueReceive(ctx->queue, &message, portMAX_DELAY) == pdTRUE &&
				message)
			{
				ctx->mqtt->DispatchMessage(message->topic, message->payload);
				delete message;
			}
		}
	}

	static bool ShouldLogFullPayload(const std::string &topic,
									 const std::string &payload)
	{
		if (payload.size() > kMqttPayloadLogLimit)
		{
			return false;
		}

		return true;
	}
} // namespace

static void log_tls_diagnostics(uint32_t verify_flags)
{
	time_t now = time(nullptr);
	struct tm utc = {};
	char timeStr[32] = {};
	if (gmtime_r(&now, &utc))
	{
		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &utc);
		LOGI("TLS system time: %s (epoch=%lld)", timeStr,
			 static_cast<long long>(now));
	}

	if (verify_flags & 0x01)
		LOGE("TLS certificate is expired");
	if (verify_flags & 0x02)
		LOGE("TLS certificate is revoked");
	if (verify_flags & 0x04)
		LOGE("TLS certificate hostname does not match");
	if (verify_flags & 0x08)
		LOGE("TLS certificate chain is not trusted by the configured CA");
	if (verify_flags & 0x10)
		LOGE("TLS certificate revocation list is not trusted");
	if (verify_flags & 0x20)
		LOGE("TLS certificate revocation list is expired");
	if (verify_flags & 0x40)
		LOGE("TLS peer certificate is missing");
	if (verify_flags & 0x80)
		LOGE("TLS certificate verification was skipped");
	if (verify_flags & 0x100)
		LOGE("TLS certificate has another verification error");
	if (verify_flags & 0x200)
		LOGE("TLS certificate is not valid yet");
}

static void log_tcp_probe(const std::string &host, int port)
{
	struct addrinfo hints = {};
	struct addrinfo *res = nullptr;
	char portStr[8] = {};
	snprintf(portStr, sizeof(portStr), "%d", port);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int gai = getaddrinfo(host.c_str(), portStr, &hints, &res);
	if (gai != 0 || !res)
	{
		LOGE("TCP probe DNS failed: host=%s, err=%d", host.c_str(), gai);
		return;
	}

	char ipStr[INET_ADDRSTRLEN] = {};
	auto *addr = reinterpret_cast<sockaddr_in *>(res->ai_addr);
	inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
	LOGI("TCP probe resolved %s:%d -> %s", host.c_str(), port, ipStr);

	int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0)
	{
		LOGE("TCP probe socket failed: errno=%d (%s)", errno, strerror(errno));
		freeaddrinfo(res);
		return;
	}

	struct timeval timeout = {};
	timeout.tv_sec = 5;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	int ret = connect(sock, res->ai_addr, res->ai_addrlen);
	if (ret == 0)
	{
		LOGI("TCP probe connect OK: %s:%d", host.c_str(), port);
	}
	else
	{
		LOGE("TCP probe connect failed: %s:%d, errno=%d (%s)", host.c_str(),
			 port, errno, strerror(errno));
	}

	close(sock);
	freeaddrinfo(res);
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	LOGV("Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	Mqtt *mqtt = (Mqtt *)handler_args;
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		LOGI("MQTT_EVENT_CONNECTED");
		mqtt->on_connected(event);
		break;
	case MQTT_EVENT_DISCONNECTED:
		LOGV("MQTT_EVENT_DISCONNECTED");
		mqtt->on_disconnected(event);
		break;
	case MQTT_EVENT_SUBSCRIBED:
		LOGV("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		mqtt->on_subscribed(event);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		LOGV("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		mqtt->on_unsubscribed(event);
		break;
	case MQTT_EVENT_PUBLISHED:
		LOGV("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		LOGV("MQTT_EVENT_DATA");
		mqtt->on_data(event);
		break;
	case MQTT_EVENT_ERROR:
		LOGV("MQTT_EVENT_ERROR");
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			const int tlsStackError =
				event->error_handle->esp_tls_stack_err;
			char tlsErrorText[160] = {};
			mbedtls_strerror(tlsStackError, tlsErrorText,
							 sizeof(tlsErrorText));
			LOGE("MQTT transport error: esp_tls_last_esp_err=%s, "
				 "esp_tls_stack_err=%d (-0x%x), error=%s, "
				 "cert_verify_flags=0x%x, "
				 "sock_errno=%d (%s)",
				 esp_err_to_name(event->error_handle->esp_tls_last_esp_err),
				 tlsStackError, -tlsStackError, tlsErrorText,
				 event->error_handle->esp_tls_cert_verify_flags,
				 event->error_handle->esp_transport_sock_errno,
				 strerror(event->error_handle->esp_transport_sock_errno));
			LOGI("TLS heap: free_internal=%u largest_internal=%u "
				 "free_8bit=%u largest_8bit=%u free_spiram=%u",
				 static_cast<unsigned>(
					 heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
				 static_cast<unsigned>(
					 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
				 static_cast<unsigned>(
					 heap_caps_get_free_size(MALLOC_CAP_8BIT)),
				 static_cast<unsigned>(
					 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
				 static_cast<unsigned>(
					 heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
			log_tls_diagnostics(
				event->error_handle->esp_tls_cert_verify_flags);
		}
		break;
	default:
		LOGV("Other event id:%d", event->event_id);
		break;
	}
}

Mqtt::Mqtt(string host, int port, string clientId, string username, string password, int keepalive, bool useTls)
	: host(host), port(port), clientId(clientId), username(username), password(password), keepalive(keepalive), useTls(useTls)
{
	connected = false;
	client = nullptr;
	messageQueue = nullptr;
	messageTaskHandle = nullptr;
}

void Mqtt::Init()
{
	LOGI("Mqtt connect: client id: %s, host: %s, port: %d, username: %s", clientId.c_str(), host.c_str(), port, username.c_str());
	esp_mqtt_client_config_t mqtt_cfg = {0};
	// mqtt_cfg.broker.address.uri = host.c_str();
	mqtt_cfg.broker.address.hostname = host.c_str();
	mqtt_cfg.broker.address.port = (uint32_t)port;
	mqtt_cfg.broker.address.transport = useTls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
	mqtt_cfg.credentials.client_id = clientId.c_str();
	mqtt_cfg.credentials.username = username.c_str();
	mqtt_cfg.credentials.authentication.password = password.c_str();
	mqtt_cfg.session.last_will.topic = willsetTopic.c_str();
	mqtt_cfg.session.last_will.msg = willsetPayload.c_str();
	mqtt_cfg.session.last_will.msg_len = (int)willsetPayload.length();
	mqtt_cfg.session.keepalive = keepalive;
	mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_5;
	mqtt_cfg.task.stack_size = 10 * 1024;
	mqtt_cfg.buffer.size = 20 * 1024;

	EnsureMessageWorker();

	client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, this);
	esp_mqtt_client_start(client);
}

void Mqtt::EnsureMessageWorker()
{
	if (!messageQueue)
	{
		messageQueue =
			xQueueCreate(kMqttQueueLength, sizeof(queued_mqtt_message_t *));
	}
	if (messageQueue && !messageTaskHandle)
	{
		mqtt_worker_ctx_t *ctx = new mqtt_worker_ctx_t{this, messageQueue};
		if (!ctx || xTaskCreate(mqtt_message_worker, "MqttMsgWorker",
								kMqttWorkerStackSize, ctx, 5,
								&messageTaskHandle) != pdPASS)
		{
			LOGE("Failed to create MQTT worker task");
			delete ctx;
			messageTaskHandle = nullptr;
		}
		else
		{
			LOGI("MQTT worker created: queue_len=%u stack=%u",
				 static_cast<unsigned>(kMqttQueueLength),
				 static_cast<unsigned>(kMqttWorkerStackSize));
		}
	}
}

void Mqtt::SetWillset(string willsetTopic, string willsetPayload)
{
	this->willsetTopic = willsetTopic;
	this->willsetPayload = willsetPayload;
}

int Mqtt::Connect(int timeout)
{
	return 0;
}

int Mqtt::Reconnect()
{
	if (!client)
	{
		return -1;
	}

	connected = false;
	log_tcp_probe(host, port);
	esp_err_t err = esp_mqtt_client_reconnect(client);
	if (err != ESP_OK)
	{
		LOGW("MQTT reconnect failed: %s", esp_err_to_name(err));
		return -1;
	}
	return 0;
}

int Mqtt::Subscribe(string topic)
{
	LOGD("Subscribe: %s", topic.c_str());
	if (!client)
		return -1;
	esp_mqtt_client_subscribe(client, topic.c_str(), 0);
	return 0;
}

int Mqtt::Unsubscribe(string topic)
{
	LOGD("Unsubscribe: %s", topic.c_str());
	if (!client)
		return -1;
	esp_mqtt_client_unsubscribe(client, topic.c_str());
	return 0;
}

int Mqtt::Publish(string topic, string payload, int qos)
{
	LOGD("Publish topic=%s bytes=%u", topic.c_str(), static_cast<unsigned>(payload.size()));
	if (!client)
		return -1;
	int msgId = esp_mqtt_client_publish(
		client, topic.c_str(), payload.c_str(), payload.length(), qos, 0);
	if (msgId < 0)
	{
		LOGE("MQTT publish enqueue failed: topic=%s connected=%d",
			 topic.c_str(), connected);
		return -1;
	}
	// LOGD("MQTT publish queued: topic=%s msg_id=%d", topic.c_str(), msgId);
	return 0;
}

int Mqtt::Publish(string topic, char *payload, int payloadLen)
{
	printf("Publish Topic: %s\nMessage: %s\n", topic.c_str(), payload);
	if (!client)
		return -1;
	int msgId =
		esp_mqtt_client_publish(client, topic.c_str(), payload, payloadLen, 0, 0);
	if (msgId < 0)
	{
		LOGE("MQTT publish enqueue failed: topic=%s connected=%d",
			 topic.c_str(), connected);
		return -1;
	}
	// LOGD("MQTT publish queued: topic=%s msg_id=%d", topic.c_str(), msgId);
	return 0;
}

bool Mqtt::IsConnected()
{
	return connected;
}

void Mqtt::ResubcribeAllTopic()
{
	for (auto &action : actionCallbacks)
	{
		Subscribe(action.getTopic());
	}
}

void Mqtt::addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType1, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType2, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType3, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
	topicList.push_back(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType4, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
	topicList.push_back(topic);
}

static vector<string> splitString(string str, char splitter)
{
	vector<string> result;
	string current = "";
	for (size_t i = 0; i < str.size(); i++)
	{
		if (str[i] == splitter)
		{
			if (current != "")
			{
				result.push_back(current);
				current = "";
			}
			continue;
		}
		current += str[i];
	}
	if (current.size() != 0)
		result.push_back(current);
	return result;
}

static int checkMqttTopic(string retrieveTopic, string registerTopic)
{
	vector<string> retrieveList = splitString(retrieveTopic, '/');
	vector<string> registerList = splitString(registerTopic, '/');
	for (size_t i = 0; i < retrieveList.size(); i++)
	{
		if (i >= registerList.size())
			return 0;
		if (registerList.at(i) == "#")
			return 1; // OK
		if (registerList.at(i) == "+")
			continue;
		if (registerList.at(i) != retrieveList.at(i))
			return 0;
	}
	if (registerList.size() == retrieveList.size())
		return 1; // OK
	return 0;
}

int Mqtt::findActionCallbackFuncFromTopic(string topic, ActionCallback **actionCallback)
{
	for (auto &action : actionCallbacks)
	{
		if (checkMqttTopic(topic, action.getTopic()))
		{
			*actionCallback = &action;
			return 1;
		}
	}
	return -1;
}

void Mqtt::on_connected(esp_mqtt_event_handle_t const event)
{
	ResubcribeAllTopic();
	connected = true;
	OnConnect(true, true);
}

void Mqtt::on_disconnected(esp_mqtt_event_handle_t const event)
{
	connected = false;
	OnConnect(false, true);
}

void Mqtt::on_subscribed(esp_mqtt_event_handle_t const event)
{
}

void Mqtt::on_unsubscribed(esp_mqtt_event_handle_t const event)
{
}

void Mqtt::on_data(esp_mqtt_event_handle_t const event)
{
	string topic = string(event->topic, event->topic + event->topic_len);
	if (ShouldDropBeforeQueue(topic, event->data, event->data_len))
	{
		LOGV("Drop MQTT message before queue: topic=%s", topic.c_str());
		return;
	}
	string payloadStr = string(event->data, event->data_len);

	queued_mqtt_message_t *message =
		new queued_mqtt_message_t{this, std::move(topic), std::move(payloadStr)};
	if (!message)
	{
		LOGE("Drop MQTT message: allocation failed");
		return;
	}

	UBaseType_t queueDepth = messageQueue ? uxQueueMessagesWaiting(messageQueue)
										  : 0;
	if (messageQueue && queueDepth >= kMqttQueueWarnThreshold)
	{
		LOGW("MQTT queue busy before enqueue: depth=%u/%u topic=%s payload=%u",
			 static_cast<unsigned>(queueDepth),
			 static_cast<unsigned>(kMqttQueueLength), message->topic.c_str(),
			 static_cast<unsigned>(message->payload.size()));
	}

	if (!messageQueue ||
		xQueueSend(messageQueue, &message, pdMS_TO_TICKS(10)) != pdTRUE)
	{
		LOGW("Drop MQTT message: queue full topic=%s", message->topic.c_str());
		delete message;
	}
}

void Mqtt::DispatchMessage(std::string topic, std::string payload)
{
	if (messageQueue)
	{
		UBaseType_t queueDepth = uxQueueMessagesWaiting(messageQueue);
		if (queueDepth >= kMqttQueueWarnThreshold)
		{
			LOGW("MQTT worker backlog: depth=%u/%u topic=%s",
				 static_cast<unsigned>(queueDepth),
				 static_cast<unsigned>(kMqttQueueLength), topic.c_str());
		}
	}

	UBaseType_t stackWords = uxTaskGetStackHighWaterMark(NULL);
	size_t stackBytes = static_cast<size_t>(stackWords) * sizeof(StackType_t);
	if (stackBytes <= kMqttWorkerStackWarnBytes)
	{
		LOGW("MQTT worker stack low: free=%u bytes topic=%s",
			 static_cast<unsigned>(stackBytes), topic.c_str());
	}

	if (ShouldIgnoreIncomingMessage(topic, payload))
	{
		return;
	}

	if (ShouldLogFullPayload(topic, payload))
	{
		printf("Receive Topic: %s \nPayload: %s\n", topic.c_str(),
			   payload.c_str());
	}
	else
	{
		LOGI("Receive Topic: %s payload_len=%u", topic.c_str(),
			 static_cast<unsigned>(payload.size()));
	}
	ActionCallback *actionCallback;
	int getCallback = findActionCallbackFuncFromTopic(topic, &actionCallback);
	if (getCallback == 1)
	{
		ActionCallback action = *actionCallback;
		if (action.getType() == 1)
		{
			action.actionCallbackFuncType1(topic, payload);
		}
		else if (action.getType() == 2)
		{
			action.actionCallbackFuncType2(topic, payload.data(),
										   static_cast<int>(payload.size()));
		}
		else if (action.getType() == 3)
		{
			action.actionCallbackFuncType3(topic, payload);
		}
		else if (action.getType() == 4)
		{
			action.actionCallbackFuncType4(topic, payload.data(),
										   static_cast<int>(payload.size()));
		}
	}
}

string Mqtt::GetHost()
{
	return host;
}
int Mqtt::GetPort()
{
	return port;
}
string Mqtt::GetClientId()
{
	return clientId;
}
string Mqtt::GetUsername()
{
	return username;
}
string Mqtt::GetPassword()
{
	return password;
}
int Mqtt::GetKeepalive()
{
	return keepalive;
}
bool Mqtt::GetUseTls()
{
	return useTls;
}

void Mqtt::SetHost(string host)
{
	this->host = host;
}
void Mqtt::SetPort(int port)
{
	this->port = port;
}
void Mqtt::SetClientId(string clientId)
{
	this->clientId = clientId;
}
void Mqtt::SetUsername(string username)
{
	this->username = username;
}
void Mqtt::SetPassword(string password)
{
	this->password = password;
}
void Mqtt::SetKeepalive(int keepalive)
{
	this->keepalive = keepalive;
}
void Mqtt::SetUseTls(bool useTls)
{
	this->useTls = useTls;
}
