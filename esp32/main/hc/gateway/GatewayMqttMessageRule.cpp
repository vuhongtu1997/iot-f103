#include "Gateway.h"
#include "Log.h"
#include "Database.h"
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace
{
struct LocalSceneOperation
{
    Device *device;
    Json::ArrayIndex itemIndex;
    uint8_t elementIndex;
    uint8_t value;
};

Device *FindSceneDevice(const std::string &identifier)
{
    return DeviceManager::getInstance()->GetDeviceFromCloudId(identifier);
}

bool ParseOnOff(const Json::Value &data, Device *device, uint8_t &elementIndex,
                uint8_t &value, bool condition)
{
    if (!data.isObject() || !device || device->GetProtocol() != BLE_DEVICE)
        return false;
    const auto keys = data.getMemberNames();
    if (keys.size() != (condition ? 2u : 1u) ||
        (condition && (!data.isMember("op") || !data["op"].isString() ||
                       data["op"].asString() != "==")))
        return false;
    for (const auto &key : keys)
    {
        if (key == "op")
            continue;
        const bool onoffKey = key.compare(0, 5, "onoff") == 0;
        const std::string indexText = onoffKey ? key.substr(5) : key;
        if (indexText.empty() ||
            !std::all_of(indexText.begin(), indexText.end(),
                         [](unsigned char c) { return std::isdigit(c) != 0; }))
            return false;
        const unsigned long parsedIndex = std::strtoul(indexText.c_str(), nullptr, 10);
        // onoff0 is zero-based; numeric device DPs ("1", "3") are one-based.
        if (!onoffKey && parsedIndex == 0)
            return false;
        const unsigned long index = onoffKey ? parsedIndex : parsedIndex - 1;
        if (index >= device->GetNumElement() || index > 255)
            return false;
        const Json::Value &onoff = data[key];
        if (!onoff.isBool() && !onoff.isInt() && !onoff.isUInt())
            return false;
        const int parsed = onoff.asInt();
        if (parsed != 0 && parsed != 1)
            return false;
        elementIndex = static_cast<uint8_t>(index);
        value = static_cast<uint8_t>(parsed);
        return true;
    }
    return false;
}

int CreateLocalRule(Json::Value &request, Json::Value &response)
{
#ifndef CONFIG_ENABLE_BLE
    response["data"]["code"] = CODE_ERROR;
    response["data"]["error"] = "BLE is disabled";
    return CODE_OK;
#else
    Json::Value &reply = response["data"];
    reply["code"] = CODE_FORMAT_ERROR;
    LOGI("createRule local request: %s", request.toString().c_str());
    if (!request.isMember("id") || !request["id"].isString() ||
        request["id"].asString().empty() ||
        (request.isMember("enable") &&
         (!request["enable"].isInt() ||
          (request["enable"].asInt() != 0 &&
           request["enable"].asInt() != 1))) ||
        !request.isMember("input") || !request["input"].isObject() ||
        !request["input"].isMember("device") ||
        !request["input"]["device"].isArray() ||
        request["input"]["device"].empty() ||
        !request.isMember("output") || !request["output"].isArray() ||
        request["output"].empty())
        return CODE_OK;

    const std::string id = request["id"].asString();
    if (RuleManager::getInstance()->GetRuleFromId(id) ||
        SceneManager::getInstance()->GetSceneFromId(id))
    {
        reply["error"] = "Rule ID already exists";
        return CODE_OK;
    }

    std::vector<LocalSceneOperation> conditions;
    std::vector<LocalSceneOperation> actions;
    Json::Value &items = request["input"]["device"];
    for (Json::ArrayIndex i = 0; i < items.size(); ++i)
    {
        Json::Value &item = items[i];
        Device *device = item.isObject() && item.isMember("id") &&
                                 item["id"].isString()
                             ? FindSceneDevice(item["id"].asString())
                             : nullptr;
        uint8_t element = 0, value = 0;
        if (!device)
        {
            LOGW("createRule input[%u] device not found: %s", i,
                 item.get("id", "").asString().c_str());
            reply["error"] = "Input device not found";
            reply["item"] = static_cast<int>(i);
            return CODE_OK;
        }
        if (!item.isMember("data") ||
            !ParseOnOff(item["data"], device, element, value, true))
        {
            LOGW("createRule input[%u] invalid data: id=%s type=%s "
                 "protocol=%d elements=%d data=%s", i,
                 item.get("id", "").asString().c_str(),
                 device->GetType().c_str(), static_cast<int>(device->GetProtocol()),
                 device->GetNumElement(), item.get("data", Json::nullValue).toString().c_str());
            reply["error"] = "Invalid input onoff element";
            reply["item"] = static_cast<int>(i);
            return CODE_OK;
        }
        LOGI("createRule input[%u] id=%s addr=0x%04X element=%u value=%u",
             i, item["id"].asString().c_str(),
             device->GetAddr(), element, value);
        conditions.push_back({device, i, element, value});
    }
    Json::Value &outputItems = request["output"];
    for (Json::ArrayIndex i = 0; i < outputItems.size(); ++i)
    {
        Json::Value &item = outputItems[i];
        Device *device = item.isObject() && item.isMember("deviceId") &&
                                 item["deviceId"].isString()
                             ? FindSceneDevice(item["deviceId"].asString())
                             : nullptr;
        uint8_t element = 0, value = 0;
        if (!device)
        {
            LOGW("createRule output[%u] device not found: %s", i,
                 item.get("deviceId", "").asString().c_str());
            reply["error"] = "Output device not found";
            reply["item"] = static_cast<int>(i);
            return CODE_OK;
        }
        if (!item.isMember("data") ||
            !ParseOnOff(item["data"], device, element, value, false))
        {
            LOGW("createRule output[%u] invalid data: id=%s type=%s "
                 "protocol=%d elements=%d data=%s", i,
                 item.get("deviceId", "").asString().c_str(),
                 device->GetType().c_str(), static_cast<int>(device->GetProtocol()),
                 device->GetNumElement(), item.get("data", Json::nullValue).toString().c_str());
            reply["error"] = "Invalid output onoff element";
            reply["item"] = static_cast<int>(i);
            return CODE_OK;
        }
        LOGI("createRule output[%u] id=%s addr=0x%04X element=%u value=%u",
             i, item["deviceId"].asString().c_str(),
             device->GetAddr(), element, value);
        actions.push_back({device, i, element, value});
    }

    uint16_t sceneId = 0;
    if (id.size() <= 4 &&
        std::all_of(id.begin(), id.end(),
                    [](unsigned char c) { return std::isxdigit(c) != 0; }))
        sceneId = static_cast<uint16_t>(std::strtoul(id.c_str(), nullptr, 16));
    else
        sceneId = SceneManager::getInstance()->GetNextSceneAddr();
    if (!sceneId || SceneManager::getInstance()->GetSceneFromAddr(sceneId))
    {
        reply["error"] = "No free BLE scene ID";
        return CODE_OK;
    }

    bool allSucceeded = true;
    unsigned successfulWrites = 0;
    for (const auto &op : conditions)
    {
        // The parsed index is zero-based for both onoffN and numeric DPs.
        const uint16_t target = op.device->GetAddr() + op.elementIndex;
        const int result = BleProtocol::getInstance()->SetConditionDeviceScene(
            -2, target, sceneId, op.device->GetType(), op.value, 1);
        request["input"]["device"][op.itemIndex]["state"] =
            result == CODE_OK ? 0 : -1;
        if (result == CODE_OK)
            ++successfulWrites;
        allSucceeded = allSucceeded && result == CODE_OK;
    }
    for (const auto &op : actions)
    {
        const uint16_t target = op.device->GetAddr() + op.elementIndex;
        const int result = BleProtocol::getInstance()->SetDeviceActionScene(
            -2, target, sceneId, op.device->GetType(), op.value,
            op.elementIndex + 1);
        request["output"][op.itemIndex]["state"] = result == CODE_OK ? 0 : -1;
        if (result == CODE_OK)
            ++successfulWrites;
        allSucceeded = allSucceeded && result == CODE_OK;
    }
    if (allSucceeded)
        allSucceeded = BleProtocol::getInstance()->SetLocalSceneState(
                           sceneId, request.get("enable", 1).asInt() == 1) == CODE_OK;

    reply["code"] = allSucceeded ? CODE_OK : CODE_ERROR;
    reply["id"] = id;
    reply["sceneId"] = sceneId;
    reply["input"] = request["input"];
    reply["output"] = request["output"];
    if (successfulWrites == 0)
    {
        reply["error"] = "No BLE node acknowledged scene configuration";
        LOGW("createRule local scene=0x%04X: no node response; scene not saved",
             sceneId);
        return CODE_OK;
    }

    Scene *scene = new Scene(id, sceneId, request.get("name", id).asString());
    scene->SetData(request);
    if (Database::getInstance()->SceneAdd(scene) != CODE_OK)
    {
        delete scene;
        reply["code"] = CODE_ERROR;
        reply["error"] = "Cannot persist local scene";
        return CODE_OK;
    }
    SceneManager::getInstance()->AddScene(scene);
    return CODE_OK;
#endif
}
}

void Gateway::InitMqttMessageRule()
{
	OnDeviceRpcCallbackRegister("createRule", bind(&Gateway::OnCreateRule, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("editRule", bind(&Gateway::OnEditRule, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("delRule", bind(&Gateway::OnDeleteRule, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("activeRule", bind(&Gateway::OnActiveRule, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("actionRule", bind(&Gateway::OnActionRule, this, placeholders::_1, placeholders::_2));
}

int Gateway::OnCreateRule(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.get("isLocal", false).asBool())
	{
		respValue["cmd"] = "createRuleRsp";
		return CreateLocalRule(reqValue, respValue);
	}
	Rule *rule = RuleManager::getInstance()->AddRule(reqValue);
	if (rule)
	{
		LOGI("Add Rule v2 %s", rule->GetId().c_str());
		Json::Value deviceList = Json::arrayValue;
		respValue["data"]["code"] = CODE_OK;
		respValue["data"]["id"] = rule->GetId();
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["cmd"] = "createRuleRsp";
	return CODE_OK;
}

int Gateway::OnEditRule(Json::Value &reqValue, Json::Value &respValue)
{
	Scene *existingScene =
		reqValue.isMember("id") && reqValue["id"].isString()
			? SceneManager::getInstance()->GetSceneFromId(reqValue["id"].asString())
			: nullptr;
	if (reqValue.get("isLocal", false).asBool() ||
		(existingScene && existingScene->GetData().get("isLocal", false).asBool()))
	{
		respValue["cmd"] = "editRuleRsp";
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		respValue["data"]["error"] = "Local scene edit is not supported";
		return CODE_OK;
	}
	int rs = CODE_ERROR;
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string ruleId = reqValue["id"].asString();
		Rule *rule = RuleManager::getInstance()->GetRuleFromId(ruleId);
		if (rule)
		{
			RuleManager::getInstance()->DelRule(rule);
		}
		rule = RuleManager::getInstance()->AddRule(reqValue);
		if (rule)
		{
			LOGI("Edit Rule %s", rule->GetId().c_str());
			Json::Value deviceList = Json::arrayValue;
			rs = CODE_OK;
		}
		else
		{
			rs = CODE_FORMAT_ERROR;
		}
		respValue["data"]["id"] = ruleId;
	}
	else
	{
		rs = CODE_FORMAT_ERROR;
	}
	respValue["data"]["code"] = rs;
	respValue["cmd"] = "editRuleRsp";
	return CODE_OK;
}

int Gateway::OnDeleteRule(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string ruleId = reqValue["id"].asString();
		Scene *localScene = SceneManager::getInstance()->GetSceneFromId(ruleId);
		if (localScene && localScene->GetData().get("isLocal", false).asBool())
		{
			int result = CODE_OK;
#ifdef CONFIG_ENABLE_BLE
			Json::Value saved = localScene->GetData();
			const uint16_t sceneId = localScene->GetAddr();
			if (BleProtocol::getInstance()->SetLocalSceneState(sceneId, false) != CODE_OK)
				result = CODE_ERROR;
			for (const auto &item : saved["input"]["device"])
			{
				if (item.get("state", -1).asInt() != 0)
					continue;
                Device *device = FindSceneDevice(item.get("id", "").asString());
				uint8_t element = 0, value = 0;
				if (!ParseOnOff(item["data"], device, element, value, true) ||
					BleProtocol::getInstance()->SetConditionDeviceScene(
						-3, device->GetAddr() + element, sceneId,
						device->GetType(), value, 1) != CODE_OK)
					result = CODE_ERROR;
			}
			for (const auto &item : saved["output"])
			{
				if (item.get("state", -1).asInt() != 0)
					continue;
                Device *device = FindSceneDevice(item.get("deviceId", "").asString());
				uint8_t element = 0, value = 0;
				if (!ParseOnOff(item["data"], device, element, value, false) ||
					BleProtocol::getInstance()->SetDeviceActionScene(
						-3, device->GetAddr() + element, sceneId,
						device->GetType(), value, element + 1) != CODE_OK)
					result = CODE_ERROR;
			}
#else
			result = CODE_ERROR;
#endif
			if (result == CODE_OK)
			{
				result = Database::getInstance()->SceneDel(localScene);
				if (result == CODE_OK)
					SceneManager::getInstance()->DelScene(localScene);
			}
			respValue["data"]["code"] = result;
			respValue["data"]["id"] = ruleId;
			respValue["cmd"] = "delRuleRsp";
			return CODE_OK;
		}
		Rule *rule = RuleManager::getInstance()->GetRuleFromId(ruleId);
		if (rule)
		{
			Json::Value deviceList = Json::arrayValue;
			RuleManager::getInstance()->DelRule(rule);
			respValue["data"]["code"] = CODE_OK;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_RULE;
		}
		respValue["data"]["id"] = ruleId;
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["cmd"] = "delRuleRsp";
	return CODE_OK;
}

int Gateway::OnActiveRule(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("status") && reqValue["status"].isInt())
	{
		string id = reqValue["id"].asString();
		int status = reqValue["status"].asInt();
		Scene *localScene = SceneManager::getInstance()->GetSceneFromId(id);
		if (localScene && localScene->GetData().get("isLocal", false).asBool())
		{
			int result = CODE_ERROR;
#ifdef CONFIG_ENABLE_BLE
			if ((status == 0 || status == 1) &&
				BleProtocol::getInstance()->SetLocalSceneState(
					localScene->GetAddr(), status == 1) == CODE_OK)
			{
				Json::Value data = localScene->GetData();
				data["enable"] = status;
				localScene->SetData(data);
				result = Database::getInstance()->SceneAdd(localScene);
			}
#endif
			respValue["data"]["code"] = result;
			respValue["data"]["id"] = id;
			respValue["cmd"] = "activeRuleRsp";
			return CODE_OK;
		}
		Rule *rule = RuleManager::getInstance()->GetRuleFromId(id);
		if (rule)
		{
			rule->SetStatus(status);
			Database::getInstance()->RuleUpdateStatus(rule);
		}
	}
	respValue["data"]["code"] = CODE_OK;
	respValue["cmd"] = "activeRuleRsp";
	return CODE_OK;
}

int Gateway::OnActionRule(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string id = reqValue["id"].asString();
		Scene *localScene = SceneManager::getInstance()->GetSceneFromId(id);
		if (localScene && localScene->GetData().get("isLocal", false).asBool())
		{
			respValue["cmd"] = "actionRuleRsp";
			respValue["data"]["id"] = id;
			respValue["data"]["code"] = localScene->Do(false);
			return CODE_OK;
		}
		Rule *rule = RuleManager::getInstance()->GetRuleFromId(id);
		if (rule)
		{
			Json::Value dataRsp;
			respValue["method"] = "actionRuleRsp";
			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = rule->GetId();

			rule->RunOutput();
		}
	}
	return CODE_NOT_RESPONSE;
}
