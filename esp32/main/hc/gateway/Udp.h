#pragma once

#include <functional>
#include "json.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ErrorCode.h"

using namespace std;

class Udp
{
private:
	typedef function<int(Json::Value &reqValue, Json::Value &respValue)> OnRpcCallbackFunc;
	map<string, OnRpcCallbackFunc> onRpcCallbackFuncList;

public:
	int fd;
	int port;
	bool isRunning;

	// Địa chỉ và rqi của gói tin đang được xử lý, dùng để gửi phản hồi sớm trong callback
	struct sockaddr_in lastSenderAddr;
	int lastSenderLen = 0;
	string lastRqi;

	Udp(int port);

	void init();
	void stop();

	int UdpCmdCallbackRegister(string cmd, OnRpcCallbackFunc onRpcCallbackFunc);
	void UdpOnMessage(string message, struct sockaddr_in *si_other, int slen);
	int send(string message, struct sockaddr_in *si_other, int slen);
	int SendNow(string message);
};
