#include "Gateway.h"
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
    (void)reqValue;
    respValue["cmd"] = "controlAllDevRsp";
    respValue["data"]["code"] = CODE_ERROR;
    return CODE_OK;
}
