#include "Gateway.h"
#include "BleProtocol.h"
#include "Device.h"
#include "DeviceManager.h"
#include "Log.h"
#include <algorithm>
#include <cctype>

using namespace std;

namespace
{
string NormalizeIdentifier(string value)
{
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
    return value;
}

Device *FindControlDevice(const string &identifier)
{
    if (identifier.empty())
        return nullptr;

    Device *device = DeviceManager::getInstance()->GetDeviceFromId(identifier);
    if (device)
        return device;

    const string normalizedIdentifier = NormalizeIdentifier(identifier);
    DeviceManager::getInstance()->ForEach([&](Device *candidate) {
        if (!device &&
            NormalizeIdentifier(candidate->GetCloudId()) == normalizedIdentifier)
            device = candidate;
    });
    if (device)
        return device;

    string normalizedMac = normalizedIdentifier;
    normalizedMac.erase(remove(normalizedMac.begin(), normalizedMac.end(), ':'),
                        normalizedMac.end());
    return DeviceManager::getInstance()->GetDeviceFromMac(normalizedMac);
}

} // namespace

void Gateway::InitMqttMessageDevice()
{
    OnDeviceRpcCallbackRegister(
        "controlDev",
        bind(&Gateway::OnControlDeviceV3, this, placeholders::_1,
             placeholders::_2));
    OnDeviceRpcCallbackRegister(
        "configDev",
        bind(&Gateway::OnControlDeviceV3, this, placeholders::_1,
             placeholders::_2));
    OnDeviceRpcCallbackRegister(
        "controlAllDev",
        bind(&Gateway::OnControlAllDeviceV3, this, placeholders::_1,
             placeholders::_2));
}

int Gateway::OnControlDeviceV3(Json::Value &reqValue, Json::Value &respValue)
{
    respValue["cmd"] = "controlDevRsp";
    respValue["data"]["code"] = CODE_ERROR;

    if (!reqValue.isObject() || !reqValue.isMember("id") ||
        !reqValue["id"].isString() || !reqValue.isMember("data") ||
        !reqValue["data"].isObject())
    {
        LOGW("controlDev invalid data: %s", reqValue.toString().c_str());
        return CODE_OK;
    }

    Device *device = FindControlDevice(reqValue["id"].asString());
    if (!device)
    {
        LOGW("controlDev device not found: %s",
             reqValue["id"].asString().c_str());
        return CODE_OK;
    }

    Json::Value controlData = reqValue["data"];
    int result = device->Do(controlData);
    respValue["data"]["code"] = result;
    LOGI("controlDev requestedId=%s localId=%s cloudId=%s type=%s data=%s "
         "result=%d",
         reqValue["id"].asString().c_str(), device->GetId().c_str(),
         device->GetCloudId().c_str(), device->GetType().c_str(),
         controlData.toString().c_str(), result);
    return CODE_OK;
}

int Gateway::OnControlAllDeviceV3(Json::Value &reqValue,
                                  Json::Value &respValue)
{
    respValue["cmd"] = "controlAllDevRsp";
    int result = CODE_ERROR;

#ifdef CONFIG_ENABLE_BLE
    if (reqValue.isObject())
    {
        bool requested = false;
        bool succeeded = true;
        const Json::Value &onoff = reqValue.isMember(KEY_ATTRIBUTE_ONOFF)
                                           ? reqValue[KEY_ATTRIBUTE_ONOFF]
                                           : reqValue[KEY_ATTRIBUTE_BUTTON];
        if (!onoff.isNull() && (onoff.isInt() || onoff.isBool()))
        {
            requested = true;
            int value = onoff.isBool() ? static_cast<int>(onoff.asBool())
                                       : onoff.asInt();
            succeeded &= BleProtocol::getInstance()->SetOnOffLight(
                             0xFFFF, value, TRANSITION_DEFAULT, true) == CODE_OK;
        }
        if (reqValue.isMember(KEY_ATTRIBUTE_DIM) &&
            reqValue[KEY_ATTRIBUTE_DIM].isInt())
        {
            requested = true;
            int value = reqValue[KEY_ATTRIBUTE_DIM].asInt();
            if (value == 0)
            {
                succeeded &= BleProtocol::getInstance()->SetOnOffLight(
                                0xFFFF, 0, TRANSITION_DEFAULT, true) == CODE_OK;
            }
            else
            {
                succeeded &= value >= 0 && value <= 100 &&
                            BleProtocol::getInstance()->SetDimmingLight(
                                0xFFFF, value * 65535 / 100,
                                TRANSITION_DEFAULT, true) == CODE_OK;
            }
        }
        if (reqValue.isMember(KEY_ATTRIBUTE_CCT) &&
            reqValue[KEY_ATTRIBUTE_CCT].isInt())
        {
            requested = true;
            int value = reqValue[KEY_ATTRIBUTE_CCT].asInt();
            succeeded &= value >= 0 && value <= 100 &&
                         BleProtocol::getInstance()->SetCctLight(
                             0xFFFF, 800 + (value * 19200 + 50) / 100,
                             TRANSITION_DEFAULT, true) == CODE_OK;
        }
        if (reqValue.isMember(KEY_ATTRIBUTE_HUE) &&
            reqValue[KEY_ATTRIBUTE_HUE].isInt() &&
            reqValue.isMember(KEY_ATTRIBUTE_SATURATION) &&
            reqValue[KEY_ATTRIBUTE_SATURATION].isInt() &&
            reqValue.isMember(KEY_ATTRIBUTE_LUMINANCE) &&
            reqValue[KEY_ATTRIBUTE_LUMINANCE].isInt())
        {
            requested = true;
            succeeded &= BleProtocol::getInstance()->SetHSLLight(
                             0xFFFF, reqValue[KEY_ATTRIBUTE_HUE].asInt(),
                             reqValue[KEY_ATTRIBUTE_SATURATION].asInt(),
                             reqValue[KEY_ATTRIBUTE_LUMINANCE].asInt(),
                             TRANSITION_DEFAULT, true) == CODE_OK;
        }
        if (reqValue.isMember(KEY_ATTRIBUTE_MODE_RGB) &&
            reqValue[KEY_ATTRIBUTE_MODE_RGB].isInt())
        {
            requested = true;
            succeeded &= BleProtocol::getInstance()->CallModeRgb(
                             0xFFFF,
                             reqValue[KEY_ATTRIBUTE_MODE_RGB].asInt()) == CODE_OK;
        }
        result = requested && succeeded ? CODE_OK : CODE_ERROR;
    }
#endif

    respValue["data"]["code"] = CODE_OK;
    return CODE_OK;
}
