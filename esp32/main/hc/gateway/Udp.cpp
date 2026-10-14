#include "Udp.h"
#include "Log.h"

#include <stdio.h>	//printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <unistd.h>
#include <errno.h>
#include "Base64.h"

#include "Util.h"
#include "Wifi.h"
#include <thread>

#ifdef ESP_PLATFORM
#include "Led.h"
#endif

#define BUFLEN 1024

Udp::Udp(int port) : port(port)
{
	fd = -1;
	isRunning = false;
}

static void UdpHandleMessage(void *data)
{
	LOGI("Start UdpHandleMessage");
	Udp *udp = (Udp *)data;
	struct sockaddr_in si_me, si_other;
	int slen = sizeof(si_other);

	int recv_len;
#ifdef ESP_PLATFORM
	char *buf = (char *)heap_caps_malloc_prefer(BUFLEN, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
#else
	char buf[BUFLEN];
#endif

	// create a UDP socket
	if ((udp->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		LOGE("UDP socket create failed");
		udp->isRunning = false;
#ifdef ESP_PLATFORM
		vTaskDelete(NULL);
#endif
		return;
	}

	int broadcastEnable = 1;
	setsockopt(udp->fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

	// zero out the structure
	memset((char *)&si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(udp->port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind socket to port
	if (::bind(udp->fd, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
	{
		LOGE("UDP bind failed on port %d", udp->port);
		close(udp->fd);
		udp->fd = -1;
		udp->isRunning = false;
#ifdef ESP_PLATFORM
		vTaskDelete(NULL);
#endif
		return;
	}

	// keep listening for data
	while (udp->isRunning)
	{
		LOGI("UDP Waiting for data...");
		fflush(stdout);

		// try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(udp->fd, buf, BUFLEN - 1, 0, (struct sockaddr *)&si_other, (socklen_t *)&slen)) == -1)
		{
			if (!udp->isRunning)
			{
				break;
			}

			LOGW("UDP recvfrom error: errno=%d", errno);
			continue;
		}

		buf[recv_len] = '\0';
		udp->UdpOnMessage(string(buf), &si_other, slen);
	}

	if (udp->fd >= 0)
	{
		close(udp->fd);
		udp->fd = -1;
	}
	udp->isRunning = false;

#ifdef ESP_PLATFORM
	heap_caps_free(buf);
#endif
#ifdef ESP_PLATFORM
	vTaskDelete(NULL);
#endif
}

void Udp::init()
{
	if (!isRunning)
	{
		isRunning = true;
#ifdef ESP_PLATFORM
		LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		if (xTaskCreate(UdpHandleMessage, "UdpHandleMessage", 10240, this, 10, NULL) != pdPASS)
		{
			LOGE("Failed to create task");
			SetLedService(false);
		}
		vTaskDelay(10);
#else
		thread udpThread(UdpHandleMessage, this);
		udpThread.detach();
#endif
	}
}

void Udp::stop()
{
	if (!isRunning)
	{
		return;
	}

	isRunning = false;
	if (fd >= 0)
	{
		shutdown(fd, SHUT_RDWR);
		close(fd);
		fd = -1;
	}
}

int Udp::UdpCmdCallbackRegister(string cmd, OnRpcCallbackFunc onRpcCallbackFunc)
{
	LOGI("UdpCmdCallbackRegister cmd: %s", cmd.c_str());
	onRpcCallbackFuncList[cmd] = onRpcCallbackFunc;
	return CODE_OK;
}

void Udp::UdpOnMessage(string message, struct sockaddr_in *si_other, int slen)
{
	LOGD("UdpOnMessage message: %s", message.c_str());
	Json::Value respValue;
	Json::Value payloadJson;
	if (payloadJson.parse(message) && payloadJson.isObject())
	{
		if (payloadJson.isMember("cmd") && payloadJson["cmd"].isString() &&
			payloadJson.isMember("data") && payloadJson["data"].isObject() &&
			payloadJson.isMember("rqi") && payloadJson["rqi"].isString())
		{
			string cmd = payloadJson["cmd"].asString();
			string rqi = payloadJson["rqi"].asString();
			lastSenderAddr = *si_other;
			lastSenderLen = slen;
			lastRqi = rqi;
			if (onRpcCallbackFuncList.find(cmd) != onRpcCallbackFuncList.end())
			{
				OnRpcCallbackFunc onRpcCallbackFunc = onRpcCallbackFuncList[cmd];
				int rs = onRpcCallbackFunc(payloadJson["data"], respValue);
				if (rs == CODE_OK)
				{
					LOGD("Call %s OK, rs: %d", cmd.c_str(), rs);
					send(respValue.toString(), si_other, slen);
				}
				else if (rs == CODE_DATA_ARRAY)
				{
					LOGD("Call %s OK, rs: %d", cmd.c_str(), rs);
					if (respValue.isArray())
					{
						for (auto &respV : respValue)
						{
							send(respV.toString(), si_other, slen);
							SLEEP_MS(10);
						}
					}
				}
				else if (rs == CODE_NOT_RESPONSE)
				{
					LOGD("Call %s OK, rs: %d", cmd.c_str(), rs);
				}
				else if (rs == CODE_EXIT)
				{
					LOGE("Call %s OK, rs: %d", cmd.c_str(), rs);
					send(respValue.toString(), si_other, slen);
					exit(1);
				}
				else
				{
					LOGW("Call %s ERR rs: %d", cmd.c_str(), rs);
				}
			}
			else
			{
				LOGW("Method %s not registed", cmd.c_str());
			}
		}
		else
		{
			LOGW("UdpOnMessage message: %s", message.c_str());
		}
	}
}

int Udp::send(string message, struct sockaddr_in *si_other, int slen)
{
	LOGI("Send message to IP: %s:%i, len: %d",
		 inet_ntoa(si_other->sin_addr), ntohs(si_other->sin_port), slen);
	LOGD("send message: %s", message.c_str());
	// now reply the client with the same data
	if (sendto(fd, message.c_str(), message.length(), 0, (struct sockaddr *)si_other, slen) == -1)
	{
		LOGW("UDP sendto error");
		return CODE_ERROR;
	}
	return CODE_OK;
}

int Udp::SendNow(string message)
{
	if (lastSenderLen <= 0)
	{
		LOGW("SendNow: no known sender to reply to");
		return CODE_ERROR;
	}
	return send(message, &lastSenderAddr, lastSenderLen);
}
