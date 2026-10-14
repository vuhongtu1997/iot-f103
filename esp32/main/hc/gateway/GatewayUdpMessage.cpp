#include "Gateway.h"
#include "Log.h"
#include "Wifi.h"
#include "Database.h"
#include <algorithm>

#ifdef ESP_PLATFORM
// #include "mongoose.h"
#include "Led.h"
#endif

void Gateway::InitUdpMessage()
{
	UdpCmdCallbackRegister("SCAN_HC", bind(&Gateway::OnUdpScanHc, this, placeholders::_1, placeholders::_2));
	UdpCmdCallbackRegister("HC_SCAN_WIFI", bind(&Gateway::OnUdpHcScanWifi, this, placeholders::_1, placeholders::_2));
	UdpCmdCallbackRegister("setupHC", bind(&Gateway::OnUdpHcSetup, this, placeholders::_1, placeholders::_2));
	UdpCmdCallbackRegister("aiHubBroadCast", bind(&Gateway::OnUdpHcInfo, this, placeholders::_1, placeholders::_2));
	UdpCmdCallbackRegister("scanIpHc", bind(&Gateway::OnScanIpHc, this, placeholders::_1, placeholders::_2));
}

int Gateway::OnUdpScanHc(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnUdpScanHc");
	if (reqValue.isMember("DORMITORY_ID") && reqValue["DORMITORY_ID"].isString())
	{
		string dormitoryId = reqValue["DORMITORY_ID"].asString();
		if (this->dormitoryId != "" && this->dormitoryId != dormitoryId)
		{
			return CODE_ERROR;
		}
		string macGw = mac;
		string hostName = "";
		macGw.erase(remove_if(macGw.begin(), macGw.end(), [](char c)
							  { return c == ':'; }),
					macGw.end());
		respValue["CMD"] = "HC_RESPONSE";
		respValue["IP"] = Wifi::GetIP();
#ifdef ESP_PLATFORM
		hostName = "RD_MH_" + macGw.substr(macGw.size() - 4, 4);
		respValue["TYPE"] = 2;
#if MG_ENABLE_MBEDTLS
		respValue["TLS"] = true;
		respValue["MQTT_PORT"] = 8883;
#else
		respValue["TLS"] = false;
		respValue["MQTT_PORT"] = 1883;
#endif
#else
		hostName = "RD_HC_" + macGw.substr(macGw.size() - 4, 4);
#ifdef __OPENWRT__
		respValue["TYPE"] = 1;
#elif defined(__ANDROID4__)
		respValue["TYPE"] = 4;
#elif defined(__ANDROID__)
		respValue["TYPE"] = 3;
#endif
		if (LocalProtocol::GetTls())
		{
			respValue["TLS"] = true;
			respValue["MQTT_PORT"] = 8883;
		}
		else
		{
			respValue["TLS"] = false;
			respValue["MQTT_PORT"] = 1883;
		}
#endif
		for (auto &c : hostName)
			c = toupper(c);
		respValue["HOSTNAME"] = hostName;
		respValue["MAC"] = mac;
		respValue["VERSION"] = STR(VERSION);
		return CODE_OK;
	}
	else
	{
		LOGW("OnUdpScanHc payload: %s error", reqValue.toString().c_str());
	}
	return CODE_ERROR;
}

int Gateway::OnUdpHcScanWifi(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnUdpHcScanWifi");
	string rqi = "";
	if (reqValue.isMember("REQUEST_ID") && reqValue["REQUEST_ID"].isString())
	{
		rqi = reqValue["REQUEST_ID"].asString();
	}
	if (reqValue.isMember("FROM") && reqValue.isMember("TO"))
	{
		Json::Value from;
		Json::Value to;
		from = reqValue["FROM"];
		to = reqValue["TO"];
		if (from.isMember("TYPE") && from["TYPE"].isInt() && to.isMember("TYPE") && to["TYPE"].isInt())
		{
			if (from["TYPE"].asInt() == 0 && to["TYPE"].asInt() == 2)
			{
				Json::Value fromRsp;
				Json::Value toRsp;
				Json::Value dataRsp;
				respValue["CMD"] = "HC_SCAN_WIFI_RESPONSE";
				respValue["REQUEST_ID"] = rqi;
				respValue["TIME"] = Util::GetCurrentTimeStr();
				respValue["CONNECTION_TYPE"] = 0;
				fromRsp["TYPE"] = 2;
				respValue["FROM"] = fromRsp;
				toRsp["TYPE"] = 0;
				respValue["TO"] = toRsp;
				Wifi::ScanWifi(dataRsp);
				respValue["DATA"] = dataRsp;
				return CODE_OK;
			}
			else
			{
				LOGW("OnUdpHcScanWifi payload: %s error direction", reqValue.toString().c_str());
			}
		}
		else
		{
			LOGW("OnUdpHcScanWifi payload: %s error", reqValue.toString().c_str());
		}
	}
	else
	{
#ifdef __ANDROID__
		LOGW("OnUdpHcScanWifi payload: %s error", reqValue.toString().c_str());
#else
		Json::Value fromRsp;
		Json::Value toRsp;
		Json::Value dataRsp;
		respValue["CMD"] = "HC_SCAN_WIFI_RESPONSE";
		respValue["REQUEST_ID"] = rqi;
		respValue["TIME"] = Util::GetCurrentTimeStr();
		respValue["CONNECTION_TYPE"] = 0;
		fromRsp["TYPE"] = 2;
		respValue["FROM"] = fromRsp;
		toRsp["TYPE"] = 0;
		respValue["TO"] = toRsp;
		Wifi::ScanWifi(dataRsp);
		respValue["DATA"] = dataRsp;
		return CODE_OK;
#endif
	}

	return CODE_ERROR;
}

int Gateway::OnUdpHcSetup(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnUdpHcSetup");
	if (reqValue.isMember("ssid") && reqValue["ssid"].isString() &&
		reqValue.isMember("pass") && reqValue["pass"].isString() &&
		reqValue.isMember("homeId") && reqValue["homeId"].isString())
	{
		string ssid = reqValue["ssid"].asString();
		string password = reqValue["pass"].asString();
		string homeId = reqValue["homeId"].asString();
		this->SetDormitory(homeId);
		Database::getInstance()->GatewayUpdateDormitory(homeId);
		LOGD("ssid: %s, password: %s", ssid.c_str(), password.c_str());

		// Báo nhận lệnh setup ngay, trước khi tiến hành kết nối wifi (có thể mất thời gian)
		Json::Value ackValue;
		ackValue["cmd"] = "setupHCRsp";
		ackValue["rqi"] = lastRqi;
		ackValue["data"]["code"] = CODE_OK;
		ackValue["data"]["mac"] = mac;
		this->SendNow(ackValue.toString());

		if (Wifi::ConnectToWifi(ssid, password) == 0)
		{
			respValue["data"]["code"] = CODE_OK;
			return CODE_OK;
		}
		else
		{
			respValue["data"]["code"] = CODE_ERROR;
			return CODE_ERROR;
		}
	}
	return CODE_ERROR;
}

int Gateway::OnUdpHcInfo(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnRpcRspHcInfo");
	string macGw = mac;
	macGw.erase(remove_if(macGw.begin(), macGw.end(), [](char c)
						  { return c == ':'; }),
				macGw.end());
	Json::Value dataValue;
	dataValue["mac"] = mac;
	dataValue["ip"] = Wifi::GetIP();
#ifdef ESP_PLATFORM
	dataValue["name"] = "RD_MH_" + macGw.substr(macGw.size() - 4, 4);
#else
	dataValue["name"] = "RD_HC_" + macGw.substr(macGw.size() - 4, 4);
#endif
	dataValue["type"] = MODEL;
	dataValue["ver"] = STR(VERSION);
	respValue["data"] = dataValue;
	respValue["cmd"] = "getHcInfoRsp";
	return CODE_OK;
}

int Gateway::OnScanIpHc(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnScanIpHc");
	string macGw = mac;
	macGw.erase(remove_if(macGw.begin(), macGw.end(), [](char c)
						  { return c == ':'; }),
				macGw.end());
	Json::Value dataValue = Json::objectValue;
	dataValue["mac"] = mac;
#ifndef ESP_PLATFORM
	string ipWlan = Wifi::GetIP("wlan0");
	string ipEth = Wifi::GetIP("eth0");
	dataValue["ipWlan"] = ipWlan;
	dataValue["ipEth"] = ipEth;
	dataValue["name"] = "RD_HC_" + macGw.substr(macGw.size() - 4, 4);
	dataValue["type"] = MODEL;
	dataValue["ver"] = STR(VERSION);
#endif
	respValue["data"] = dataValue;
	respValue["cmd"] = "scanIpHcRsp";
	return CODE_OK;
}
