#include "DeviceBle.h"
#include "BleProtocol.h"
#include "Device.h"
#include "DeviceManager.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"
#include "BleOpCode.h"
#include <algorithm>

namespace
{
	int CctRawToValue(uint16_t rawCct)
	{
		if (rawCct <= 800)
			return 0;
		if (rawCct >= 20000)
			return 1000;
		return static_cast<int>(((static_cast<uint32_t>(rawCct) - 800) * 1000 +
								 9600) /
								19200);
	}

	uint16_t ToMeshElementAddr(uint16_t epId)
	{
		return bswap_16(epId);
	}

	uint16_t ToMeshGroupAddr(uint16_t groupAddr)
	{
		// hc_lux-style payloads already carry the BLE group bytes in display order
		// (for example "2DC0"), while some older internal flows still use 1..4095.
		if ((groupAddr & 0x00F0) == 0x00C0)
			return bswap_16(groupAddr);
		return static_cast<uint16_t>(groupAddr + BLE_GROUP_OFFSET);
	}
}

DeviceBle::DeviceBle(string id, string name, string mac, string type, uint16_t addr, uint16_t version, Json::Value *dataJson) : Device(id, name, mac, type, addr, version, dataJson)
{
	protocol = BLE_DEVICE;
	countElement = 1;
	deviceKey = GetDeviceKey(dataJson);
	this->timesUpdateInPeriod = 0;
	this->lastTimeCheckIsolate = time(NULL);
}

DeviceBle::~DeviceBle()
{
	for (auto &module : modules)
		delete module;
}

string DeviceBle::GetDeviceKey(Json::Value *dataJson)
{
	if (dataJson && dataJson->isObject())
	{
		if (dataJson->isMember("devicekey") && (*dataJson)["devicekey"].isString())
		{
			deviceKey = (*dataJson)["devicekey"].asString();
		}
		else if (dataJson->isMember("deviceKey") && (*dataJson)["deviceKey"].isString())
		{
			deviceKey = (*dataJson)["deviceKey"].asString();
		}
		else if (dataJson->isMember("devices") && (*dataJson)["devices"].isString())
		{
			vector<string> tokens =
				Util::splitString((*dataJson)["devices"].asString(), '|');
			if (tokens.size() >= 5)
				deviceKey = tokens[4];
		}
	}
	return deviceKey;
}

bool DeviceBle::CheckAddr(uint16_t addr)
{
	// New-core devices store logical mesh addresses (0x0011, 0x0012...).
	if (this->addr <= addr && addr < this->addr + countElement)
		return true;

	// Compatibility with old database rows which stored display-endian
	// addresses (0x1100, 0x1200...).
	uint16_t baseAddrInt = (this->addr >> 8) | (this->addr << 8);
	uint16_t checkAddrInt = (addr >> 8) | (addr << 8);
	bool match = ((baseAddrInt <= checkAddrInt) &&
				  (baseAddrInt + countElement - 1 >= checkAddrInt));
	// if (match)
	// {
	// 	LOGD("DeviceBle::CheckAddr MATCH: this->addr=0x%04X, inputAddr=0x%04X (swapped: 0x%04X vs 0x%04X)",
	// 		 this->addr, addr, baseAddrInt, checkAddrInt);
	// }
	return match;
}

string DeviceBle::GetDeviceKey()
{
	return deviceKey;
}

int DeviceBle::BuildTelemetryValue(Json::Value &pushDataValue)
{
	for (auto &module : modules)
	{
		module->BuildTelemetryValue(pushDataValue);
	}
	int temp = IsOnline();
	pushDataValue["stt"] = temp;
	return CODE_OK;
}

void DeviceBle::Isolate()
{
	this->timesUpdateInPeriod++;
	time_t timeCheck = time(NULL);
	if (((timeCheck - this->lastTimeCheckIsolate) >= BLE_PERIOD_ISOLATE) && (timeCheck - this->lastTimeCheckIsolate) <= (BLE_PERIOD_ISOLATE + 10))
	{
		if (this->timesUpdateInPeriod >= BLE_TIMES_CHECK_ISOLATE)
		{
			LOGW("Isolate device %s", this->id.c_str());
			BleProtocol::getInstance()->ResetDev(addr);
		}
		this->lastTimeCheckIsolate = timeCheck;
		this->timesUpdateInPeriod = 0;
		return;
	}
	if ((timeCheck - this->lastTimeCheckIsolate) > (BLE_PERIOD_ISOLATE + 10))
	{
		this->lastTimeCheckIsolate = timeCheck;
		this->timesUpdateInPeriod = 0;
	}
}

void DeviceBle::InputData(Json::Value &dataValue, bool isPushTelemety)
{
	values = Json::Value::null;
	for (auto &module : modules)
	{
		module->InputData(dataValue, values);
	}
	if (!values.isNull() && isPushTelemety)
		PushTelemetry(values);
}

void DeviceBle::InputData(uint8_t *data, int len, uint16_t addr)
{
	values = Json::Value::null;
	bool isCheckIsolate = true;
	for (auto &module : modules)
	{
		if (module->CheckAddr(addr))
		{
			if (isCheckIsolate)
			{
				isCheckIsolate = false;
				Isolate();
			}
			if (module->InputData(data, len, values) == CODE_OK)
				break;
		}
	}
	PushTelemetry(values);
}

bool DeviceBle::HandleBleBroadcastStatus(uint8_t *data, int len,
										 uint16_t resolvedAddr)
{
	if (len < 8 || data[0] != HG_OPCODE_CONFIG_RSP || data[1] != 0x11 ||
		data[2] != 0x02 || data[5] != 0xF1 || data[6] != 0x00)
		return false;

	Json::Value dataValues = Json::objectValue;
	int elementCount = std::min(static_cast<int>(data[7]), GetNumElement());
	elementCount = std::min(elementCount, len - 8);

	for (int i = 0; i < elementCount; i++)
	{
		dataValues[KEY_ATTRIBUTE_ONOFF + to_string(i)] =
			data[8 + i];
	}

	if (!dataValues.empty())
	{
		LOGI("Broadcast on/off status parsed: devAddr=0x%04X data=%s",
			 resolvedAddr, dataValues.toStyledString().c_str());
		SetPropertyJsonUpdate(dataValues);
	}
	return true;
}

bool DeviceBle::HandleBleStatus(uint16_t opcode, uint8_t *data, int len,
								uint16_t resolvedAddr)
{
	if (len < 5)
		return false;

	if (opcode == LIGHTNESS_LINEAR_STATUS && (data[2] == 2 || data[0] == 0x82))
	{
		typedef struct __attribute__((packed))
		{
			uint16_t opcode;
			uint8_t header;
			uint8_t status_mode;
			uint16_t value1;
			uint16_t value2;
			uint16_t value3;
		} data_message_t;
		data_message_t *data_message = (data_message_t *)data;

		Json::Value dataValues = Json::objectValue;
		if (len >= static_cast<int>(sizeof(data_message_t)) &&
			data_message->opcode == LIGHTNESS_LINEAR_STATUS &&
			data_message->header == 2)
		{
			dataValues[KEY_ATTRIBUTE_ONOFF] =
				(data_message->status_mode >> 4) & 0x0F;
			if ((data_message->status_mode & 0x0F) == 1)
			{
				dataValues[KEY_ATTRIBUTE_DIM] =
					(data_message->value1 * 100) / 65535;
				dataValues[KEY_ATTRIBUTE_CCT] =
					CctRawToValue(data_message->value2);
			}
			else if ((data_message->status_mode & 0x0F) == 0)
			{
				if (data_message->value2 > 0 && data_message->value1 > 0 &&
					data_message->value3 > 0)
				{
					dataValues[KEY_ATTRIBUTE_HUE] = data_message->value2;
					dataValues[KEY_ATTRIBUTE_SATURATION] =
						data_message->value3;
					dataValues[KEY_ATTRIBUTE_LUMINANCE] = data_message->value1;
				}
			}
		}

		if (!dataValues.empty())
		{
			LOGI("Device status parsed data: %s",
				 dataValues.toStyledString().c_str());
			SetPropertyJsonUpdate(dataValues);
		}
		else
		{
			LOGW("Device lightness status frame parse returned empty data for "
				 "devAddr=0x%04X",
				 resolvedAddr);
		}
		return true;
	}

	uint16_t vendorId = data[1] | (data[2] << 8);
	uint16_t header = data[3] | (data[4] << 8);

	if (data[0] == RD_OPCODE_CONFIG_RSP && vendorId == RD_VENDOR_ID &&
		header == RD_HEADER_REQUEST_STATUS_SWITCH)
	{
		if (len < 6)
			return true;

		Json::Value dataValues = Json::objectValue;
		int elementCount = std::min(static_cast<int>(data[5]), GetNumElement());
		elementCount = std::min(elementCount, len - 6);
		for (int i = 0; i < elementCount; i++)
		{
			dataValues[KEY_ATTRIBUTE_ONOFF + to_string(i)] =
				data[6 + i];
		}
		if (!dataValues.empty())
			SetPropertyJsonUpdate(dataValues);
		return true;
	}

	if (data[0] == RD_OPCODE_CONFIG_RSP && vendorId == RD_VENDOR_ID &&
		header == RD_HEADER_SEFTPOWER_REMOTE_PRESS)
	{
		if (len < 7)
			return true;

		uint16_t childAddr = data[5] | (data[6] << 8);
		DeviceBle *deviceBleChild =
			DeviceManager::getInstance()->GetDeviceBleFromAddr(childAddr);
		if (deviceBleChild)
			deviceBleChild->DeviceInputData(data, len, childAddr);
		return true;
	}

	if (data[0] == HG_OPCODE_CONFIG_RSP && vendorId == RD_VENDOR_ID &&
		header == HG_HEADER_000C && len >= 7)
	{
		uint16_t baseAddr = bswap_16(GetAddr());
		uint16_t elementAddr = bswap_16(resolvedAddr);
		int dpId = static_cast<int>(elementAddr - baseAddr) + 1;
		int lockValue = data[5] ? 1 : 0;

		if (dpId >= 1 && dpId <= GetNumElement())
		{
			Json::Value reported;
			reported["sender"] = 11;
			reported["type"] = TYPE_UPDATE_DEVICE;
			reported[GetCloudId()]["lock"][to_string(dpId)] = lockValue;
			// Gateway::getInstance()->DeviceTelemetry(this, reported);
			LOGI("Child lock status: device=%s dp=%d lock=%d",
				 GetCloudId().c_str(), dpId, lockValue);
		}
		else
		{
			LOGW("Invalid child lock element: base=0x%04X addr=0x%04X dp=%d",
				 GetAddr(), resolvedAddr, dpId);
		}
		return true;
	}

	return false;
}

bool DeviceBle::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	for (auto &module : modules)
	{
		if (module->CheckData(dataValue, rs))
			return true;
	}
	return false;
}

int DeviceBle::GetNumElement()
{
	return countElement;
}

int DeviceBle::AddToGroup(uint16_t groupAddr, uint16_t epId)
{
	return BleProtocol::getInstance()->AddDev2Group(
		addr, ToMeshElementAddr(epId), ToMeshGroupAddr(groupAddr));
}

int DeviceBle::RemoveFromGroup(uint16_t groupAddr, uint16_t epId)
{
	return BleProtocol::getInstance()->DelDev2Group(
		addr, ToMeshElementAddr(epId), ToMeshGroupAddr(groupAddr));
}

int DeviceBle::AddToScene(uint16_t sceneAddr, Json::Value &dataValue, uint16_t groupAddr)
{
	int modeRgb = 0;
	if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_MODE_RGB) && dataValue[KEY_ATTRIBUTE_MODE_RGB].isInt())
	{
		modeRgb = dataValue[KEY_ATTRIBUTE_MODE_RGB].asInt();
	}

	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// // Cảnh cho đèn, switch on/off
	// if ((indexType1 == 1) ||
	// 	(indexType1 == 2 && indexType2 == 1))
	// {
	// 	return BleProtocol::getInstance()->SetScene(addr, sceneAddr, modeRgb);
	// } // Cảnh cho công tắc: cài từng element
	// else if ((indexType1 == 2 && indexType2 == 2) ||
	// 		 (indexType1 == 2 && indexType2 == 4) ||
	// 		 (indexType1 == 2 && indexType2 == 6))
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (dataValue.isMember(KEY_ATTRIBUTE_BUTTON + ((i) ? to_string(i + 1) : "")))
	// 		{
	// 			if (BleProtocol::getInstance()->SetScene(addr + i, sceneAddr, 0) != CODE_OK)
	// 				rs = CODE_ERROR;
	// 		}
	// 	}
	// 	return rs;
	// }
	// else
	// 	LOGW("Type device %d not supported config scene", this->GetType());

	return CODE_ERROR;
}

int DeviceBle::RemoveFromScene(uint16_t sceneAddr, Json::Value &dataValue, uint16_t groupAddr)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// // Cảnh cho đèn, switch on/off
	// if ((indexType1 == 1) ||
	// 	(indexType1 == 2 && indexType2 == 1))
	// {
	// 	return BleProtocol::getInstance()->DelScene(addr, sceneAddr);
	// } // Cảnh cho công tắc: xóa từng element
	// else if ((indexType1 == 2 && indexType2 == 2) ||
	// 		 (indexType1 == 2 && indexType2 == 4) ||
	// 		 (indexType1 == 2 && indexType2 == 6))
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (dataValue.isMember(KEY_ATTRIBUTE_BUTTON + ((i) ? to_string(i + 1) : "")))
	// 		{
	// 			if (BleProtocol::getInstance()->DelScene(addr + i, sceneAddr) != CODE_OK)
	// 				rs = CODE_ERROR;
	// 		}
	// 	}
	// 	return rs;
	// }
	// else
	// 	LOGW("Type device %d not supported config scene", this->GetType());
	return CODE_ERROR;
}

int DeviceBle::EditInScene(uint16_t sceneAddr, Json::Value &dataValue)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// // edit cảnh cho công tắc, phải xóa data trong cảnh trước đó
	// if ((indexType1 == 2 && indexType2 == 2) ||
	// 	(indexType1 == 2 && indexType2 == 4) ||
	// 	(indexType1 == 2 && indexType2 == 6))
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (dataValue.isMember(KEY_ATTRIBUTE_BUTTON + ((i) ? to_string(i + 1) : "")))
	// 		{
	// 			if (BleProtocol::getInstance()->DelScene(addr + i, sceneAddr) != CODE_OK)
	// 				rs = CODE_ERROR;
	// 		}
	// 	}
	// 	return rs;
	// }
	return CODE_ERROR;
}

int DeviceBle::AddToRoom(uint16_t roomAddr)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1) // Đèn
	// {
	// 	return BleProtocol::getInstance()->AddDev2Group(addr, addr, roomAddr + BLE_GROUP_OFFSET);
	// }
	// else if (indexType1 == 2 && indexType2 == 2)
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (BleProtocol::getInstance()->AddDev2Group(addr, addr + i, roomAddr + BLE_GROUP_OFFSET) != CODE_OK)
	// 			rs = CODE_ERROR;
	// 	}
	// 	return rs;
	// }
	return CODE_OK;
}

int DeviceBle::RemoveFromRoom(uint16_t roomAddr)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1)
	// {
	// 	return BleProtocol::getInstance()->DelDev2Group(addr, addr, roomAddr + BLE_GROUP_OFFSET);
	// }
	// else if (indexType1 == 2 && indexType2 == 2)
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (BleProtocol::getInstance()->DelDev2Group(addr, addr + i, roomAddr + BLE_GROUP_OFFSET) != CODE_OK)
	// 			rs = CODE_ERROR;
	// 	}
	// 	return rs;
	// }
	return CODE_OK;
}

int DeviceBle::AddToGroupInRoom(uint16_t groupAddr, uint32_t groupType)
{

	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1)
	// {
	// 	if (this->GetType() == groupType)
	// 	{
	// 		return BleProtocol::getInstance()->AddDev2Group(addr, addr, groupAddr + BLE_GROUP_OFFSET);
	// 	}
	// }
	// else if (indexType1 == 2 && indexType2 == 2)
	// {
	// 	if (groupType == 1)
	// 	{
	// 		int rs = CODE_OK;
	// 		for (int i = 0; i < this->GetNumElement(); i++)
	// 		{
	// 			if (BleProtocol::getInstance()->AddDev2Group(addr, addr + i, groupAddr + BLE_GROUP_OFFSET) != CODE_OK)
	// 				rs = CODE_ERROR;
	// 		}
	// 		return rs;
	// 	}
	// }
	return CODE_OK;
}

int DeviceBle::AddToSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue, uint16_t roomAddr)
{

	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1)
	// {
	// 	return AddToScene(sceneAddr, dataValue);
	// }
	return CODE_OK;
}

int DeviceBle::RemoveFromGroupInRoom(uint16_t groupAddr)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1)
	// {
	// 	return BleProtocol::getInstance()->DelDev2Group(addr, addr, groupAddr + BLE_GROUP_OFFSET);
	// }
	// else if (indexType1 == 2 && indexType2 == 2)
	// {
	// 	int rs = CODE_OK;
	// 	for (int i = 0; i < this->GetNumElement(); i++)
	// 	{
	// 		if (BleProtocol::getInstance()->DelDev2Group(addr, addr + i, groupAddr + BLE_GROUP_OFFSET) != CODE_OK)
	// 			rs = CODE_ERROR;
	// 	}
	// 	return rs;
	// }
	return CODE_OK;
}

int DeviceBle::RemoveFromSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue, uint16_t roomAddr)
{
	// int indexType1 = (this->GetType() >> 16) & 0xFF;
	// int indexType2 = (this->GetType() >> 8) & 0xFF;
	// if (indexType1 == 1)
	// {
	// 	return RemoveFromScene(sceneAddr, dataValue);
	// }
	return CODE_OK;
}

int DeviceBle::Do(Json::Value &dataValue)
{
	int rs = CODE_ERROR;
	for (auto &module : modules)
	{
		if (module->Do(dataValue) == CODE_OK)
			rs = CODE_OK;
	}
	return rs;
}

int DeviceBle::InitAttribute(string attribute, double value)
{
	for (auto &module : modules)
	{
		module->InitAttribute(attribute, value);
	}
	return CODE_OK;
}
