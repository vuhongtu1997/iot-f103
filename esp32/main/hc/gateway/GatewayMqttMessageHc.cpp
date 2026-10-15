#include "Gateway.h"
#include "Log.h"
#include "Wifi.h"
#include "Config.h"
#include "Database.h"
#include "ModbusBus.h"
#include "protocol.h"

#include <cstdio>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void Gateway::InitMqttMessageRs485()
{
	OnDeviceRpcCallbackRegister(
		"scan", bind(&Gateway::OnModbusDiscover, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister(
		"discover", bind(&Gateway::OnModbusDiscover, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister(
		"assign_address", bind(&Gateway::OnModbusAssignAddress, this, placeholders::_1, placeholders::_2));
}

int Gateway::OnModbusDiscover(Json::Value &, Json::Value &respValue)
{
	bool success = ModbusDiscoverNodes();
	std::vector<ModbusNodeInfo> nodes;
	ModbusGetNodes(nodes);
	respValue["method"] = "discoverRsp";
	respValue["params"]["code"] = success ? CODE_OK : CODE_ERROR;
	respValue["params"]["nodes"] = Json::arrayValue;
	for (const ModbusNodeInfo &node : nodes)
	{
		char uid[25];
		for (size_t i = 0; i < sizeof(node.uid); ++i)
			snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02x", node.uid[i]);
		Json::Value item;
		item["address"] = node.address;
		item["uid"] = uid;
		item["version"] = static_cast<Json::UInt64>(node.version);
		item["mode"] = node.mode;
		respValue["params"]["nodes"].append(item);
	}
	if (!success)
		respValue["params"]["error"] = "Modbus scan failed or registry could not be saved";
	return CODE_OK;
}

int Gateway::OnModbusAssignAddress(Json::Value &reqValue, Json::Value &respValue)
{
	respValue["method"] = "assign_addressRsp";
	respValue["params"]["code"] = CODE_FORMAT_ERROR;
	if (!reqValue.isObject() || !reqValue.isMember("address") ||
		!reqValue["address"].isInt() || reqValue["address"].asInt() < 1 ||
		reqValue["address"].asInt() > MAX_NODES)
	{
		respValue["params"]["error"] = "address must be an integer from 1 to 247";
		return CODE_OK;
	}

	ModbusNodeInfo assigned{};
	int result = ModbusAssignAddress(
		static_cast<uint8_t>(reqValue["address"].asInt()), &assigned);
	respValue["params"]["code"] = result == 1 ? CODE_OK : CODE_ERROR;
	respValue["params"]["result"] = result;
	if (result == 1)
	{
		char uid[25];
		for (size_t i = 0; i < sizeof(assigned.uid); ++i)
			snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02x", assigned.uid[i]);
		respValue["params"]["address"] = assigned.address;
		respValue["params"]["uid"] = uid;
		respValue["params"]["version"] = static_cast<Json::UInt64>(assigned.version);
	}
	else
	{
		respValue["params"]["error"] =
			result == -1 ? "address is already registered or occupied"
			: result == -2 ? "bus activity made the address check uncertain"
			: result == -4 ? "invalid address or registry is full"
			: result == -5 ? "could not persist address reservation"
			                : "assignment was not confirmed; run scan before retrying";
	}
	return CODE_OK;
}

int Gateway::OnControlHc(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnControlHc");
	int rs = Do(reqValue);
	respValue["params"]["code"] = rs;
	respValue["method"] = "controlHcRsp";
	return CODE_OK;
}

int Gateway::OnRegisterHc(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnRegisterHc");
	if (reqValue.isMember("mac") && reqValue["mac"].isString() &&
		reqValue.isMember("dormitory") && reqValue["dormitory"].isString())
	{
		string macGw = reqValue["mac"].asString();
		string dormitoryGw = reqValue["dormitory"].asString();

		if (mac == macGw)
		{
			if (this->GetDormitory() == "" || this->GetDormitory() == dormitoryGw)
			{
				this->SetDormitory(dormitoryGw);
				Database::getInstance()->GatewayUpdateDormitory(dormitoryGw);
				respValue["method"] = "registerDevRsp";
				respValue["params"]["code"] = CODE_OK;
				respValue["params"]["mac"] = mac;
				respValue["params"]["ip"] = Wifi::GetIP();
				respValue["params"]["version"] = STR(VERSION);
				respValue["params"]["type"] = TYPE_DEVICE;
				respValue["params"]["name"] = "HC_" + mac;

				if (reqValue.isMember("latitude") && reqValue["latitude"].isDouble() &&
					reqValue.isMember("longitude") && reqValue["longitude"].isDouble())
				{
					Json::Value dataJson;
					dataJson["latitude"] = reqValue["latitude"].asDouble();
					dataJson["longitude"] = reqValue["longitude"].asDouble();
					this->SetData(dataJson.toString());
					Database::getInstance()->GatewayUpdateData(dataJson.toString());
				}
				return CODE_OK;
			}
			else
			{
				LOGW("HC have dormitory");
			}
		}
		else
		{
			LOGW("hc don't match mac");
		}
	}
	else
	{
		LOGW("data error %s", reqValue.toString().c_str());
	}
	return CODE_ERROR;
}

int Gateway::OnGetHcInfo(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnGetHcInfo");
	Json::Value dataValue;
	dataValue["mac"] = mac;
	dataValue["ip"] = Wifi::GetIP();
	dataValue["isConnectCloud"] = CloudProtocol::IsConnected();
	dataValue["name"] = "RD HC";
	dataValue["type"] = MODEL;
	dataValue["ver"] = STR(VERSION);
	if (this->dormitoryId == "")
	{
		dataValue["isInHome"] = false;
	}
	else
	{
		dataValue["isInHome"] = true;
	}
	respValue["params"] = dataValue;
	respValue["method"] = "getHcInfoRsp";
	return CODE_OK;
}

int Gateway::OnResetHC(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGW("OnResetFactory");
	ResetFactory();
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "resetHcRsp";
	return CODE_EXIT;
}

int Gateway::OnVersionHC(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("Version HC");
	respValue["params"]["code"] = CODE_OK;
	respValue["params"]["version"] = STR(VERSION);
	respValue["method"] = "versionHcRsp";
	return CODE_OK;
}

int Gateway::OnOtaHc(Json::Value &reqValue, Json::Value &respValue, std::string &topicRsp)
{
	LOGI("TYPE_OTA_HC_FROM_APP");
	// Gateway::getInstance()->FirmwareUpdate("","","");
	return CODE_OK;
}
