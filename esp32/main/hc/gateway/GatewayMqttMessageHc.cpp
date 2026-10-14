#include "Gateway.h"
#include "Log.h"
#include "Wifi.h"
#include "Base64.h"
#include "Config.h"
#include "Database.h"

#include <fstream>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

void Gateway::InitMqttMessageHc()
{
	// OnDeviceRpcCallbackRegister(TYPE_OTA_HC, bind(&Gateway::OnOtaHc, this, placeholders::_1, placeholders::_2, placeholders::_3));
	OnDeviceRpcCallbackRegister("startScanBle", bind(&Gateway::OnStartScan, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("stopScanBle", bind(&Gateway::OnStopScan, this, placeholders::_1, placeholders::_2));
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

int Gateway::OnStartScan(Json::Value &reqValue, Json::Value &respValue)
{
	int rsCode = CODE_OK;

#ifdef CONFIG_ENABLE_BLE
	BleProtocol *ble = BleProtocol::getInstance();
	// OnServerReq passes payload.data to this callback directly.
	const Json::Value &data = reqValue;
	const bool hasRequestConfig = data.isObject() &&
		(data.isMember("gw_add") || data.isMember("gatewayAddress"));
	if (!hasRequestConfig && !ble->HasProvisionedMeshConfig())
		ble->InitKey();
	if (hasRequestConfig)
	{
		rsCode = ble->ConfigureProvisionedMesh(data);
		if (rsCode != CODE_OK)
		{
			respValue["params"]["error"] = "MESH_CONFIG_INVALID";
			respValue["params"]["message"] =
				"Homegy Mesh configuration is invalid or could not be applied";
		}
	}
	else if (!ble->HasProvisionedMeshConfig())
	{
		rsCode = CODE_ERROR;
		if (GetDormitory().empty())
		{
			LOGE("startScanBle rejected: homeId is unavailable");
			respValue["params"]["error"] = "HOME_ID_MISSING";
			respValue["params"]["message"] =
				"Set up the gateway with a homeId before creating its Homegy Mesh";
		}
		else if (ble->NeedsMeshRecovery())
		{
			LOGE("startScanBle rejected: BLE module holds a Mesh whose AppKey is not stored");
			respValue["params"]["error"] = "MESH_RECOVERY_REQUIRED";
			respValue["params"]["message"] =
				"Restore the matching Homegy Mesh configuration before scanning";
		}
		else
		{
			LOGE("startScanBle rejected: Homegy mesh configuration is unavailable");
			respValue["params"]["error"] = "MESH_CONFIG_MISSING";
			respValue["params"]["message"] =
				"Provide Homegy Mesh configuration in startScanBle.data before scanning";
		}
		respValue["params"]["requiredFields"] =
			"networkKey, networkKeyIndex, appKey, appKeyIndex, ivIndex, gatewayAddress, gatewayDeviceKey";
	}
	if (rsCode == CODE_OK)
	{
		ble->SetProvisioning(true);
		rsCode = ble->StartScan();
		if (rsCode != CODE_OK)
			ble->SetProvisioning(false);
	}
#endif

	respValue["params"]["code"] = rsCode;
	respValue["method"] = "startScanRsp";

	return CODE_OK;
}

int Gateway::OnStopScan(Json::Value &reqValue, Json::Value &respValue)
{
	int rsCode = CODE_OK;

#ifdef CONFIG_ENABLE_BLE
	BleProtocol::getInstance()->SetProvisioning(false);
	rsCode = BleProtocol::getInstance()->StopScan();
#endif

	respValue["params"]["code"] = rsCode;
	respValue["method"] = "stopScanRsp";
	return CODE_OK;
}

int Gateway::OnStartScanPairDev(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	int rs = CODE_OK;
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string devId = reqValue["id"].asString();
		Device *device = DeviceManager::getInstance()->GetDeviceFromId(devId);
		if (device)
		{
			respValue["method"] = "startScanChildDevRsp";
			respValue["params"]["code"] = CODE_OK;
			// DeviceRpcResponse(topicRsp, respValue.toString());
#ifdef CONFIG_ENABLE_BLE
			uint32_t maxAddr = DeviceManager::getInstance()->GetMaxAddrBle();
			LOGW("max addr: %d", maxAddr + BLE_MAX_ELEMENT);
			BleProtocol::getInstance()->AddPairDevice(device->GetAddr(), maxAddr + BLE_MAX_ELEMENT);
#endif
		}
		else
			rs = CODE_NOT_FOUND_DEVICE;
	}
	else
		rs = CODE_FORMAT_ERROR;

	// respValue["params"]["code"] = rs;
	// respValue["method"] = "stopScanChildDevRsp";
	return CODE_NOT_RESPONSE;
}

int Gateway::OnStopScanPairDev(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string deviceId = reqValue["id"].asString();
		Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
		if (device)
		{
#ifdef CONFIG_ENABLE_BLE
			BleProtocol::getInstance()->ScanStopSeftPowerRemote(device->GetAddr(), 0);
#endif
		}
	}
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "stopScanChildDevRsp";
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
