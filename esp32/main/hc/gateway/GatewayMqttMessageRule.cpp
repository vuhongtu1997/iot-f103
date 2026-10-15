#include "Gateway.h"
#include "Database.h"
#include "Log.h"

void Gateway::InitMqttMessageRule()
{
  OnDeviceRpcCallbackRegister(
      "createRule", bind(&Gateway::OnCreateRule, this, placeholders::_1, placeholders::_2));
  OnDeviceRpcCallbackRegister(
      "editRule", bind(&Gateway::OnEditRule, this, placeholders::_1, placeholders::_2));
  OnDeviceRpcCallbackRegister(
      "delRule", bind(&Gateway::OnDeleteRule, this, placeholders::_1, placeholders::_2));
  OnDeviceRpcCallbackRegister(
      "activeRule", bind(&Gateway::OnActiveRule, this, placeholders::_1, placeholders::_2));
  OnDeviceRpcCallbackRegister(
      "actionRule", bind(&Gateway::OnActionRule, this, placeholders::_1, placeholders::_2));
}

int Gateway::OnCreateRule(Json::Value &reqValue, Json::Value &respValue)
{
  respValue["cmd"] = "createRuleRsp";
  if (reqValue.get("isLocal", false).asBool())
  {
    LOGW("Local scene rules are not supported");
    respValue["data"]["code"] = CODE_FORMAT_ERROR;
    return CODE_OK;
  }

  Rule *rule = RuleManager::getInstance()->AddRule(reqValue);
  if (rule)
  {
    LOGI("Add Rule v2 %s", rule->GetId().c_str());
    respValue["data"]["code"] = CODE_OK;
    respValue["data"]["id"] = rule->GetId();
  }
  else
  {
    respValue["data"]["code"] = CODE_FORMAT_ERROR;
  }
  return CODE_OK;
}

int Gateway::OnEditRule(Json::Value &reqValue, Json::Value &respValue)
{
  respValue["cmd"] = "editRuleRsp";
  if (reqValue.get("isLocal", false).asBool())
  {
    respValue["data"]["code"] = CODE_FORMAT_ERROR;
    respValue["data"]["error"] = "Local scene rules are not supported";
    return CODE_OK;
  }

  if (!reqValue.isMember("id") || !reqValue["id"].isString())
  {
    respValue["data"]["code"] = CODE_FORMAT_ERROR;
    return CODE_OK;
  }

  const string id = reqValue["id"].asString();
  Rule *existing = RuleManager::getInstance()->GetRuleFromId(id);
  if (existing)
    RuleManager::getInstance()->DelRule(existing);

  Rule *rule = RuleManager::getInstance()->AddRule(reqValue);
  respValue["data"]["code"] = rule ? CODE_OK : CODE_FORMAT_ERROR;
  respValue["data"]["id"] = id;
  return CODE_OK;
}

int Gateway::OnDeleteRule(Json::Value &reqValue, Json::Value &respValue)
{
  respValue["cmd"] = "delRuleRsp";
  if (!reqValue.isMember("id") || !reqValue["id"].isString())
  {
    respValue["data"]["code"] = CODE_FORMAT_ERROR;
    return CODE_OK;
  }

  const string id = reqValue["id"].asString();
  Rule *rule = RuleManager::getInstance()->GetRuleFromId(id);
  if (!rule)
  {
    respValue["data"]["code"] = CODE_NOT_FOUND_RULE;
  }
  else
  {
    RuleManager::getInstance()->DelRule(rule);
    respValue["data"]["code"] = Database::getInstance()->RuleDel(id);
  }
  respValue["data"]["id"] = id;
  return CODE_OK;
}

int Gateway::OnActiveRule(Json::Value &reqValue, Json::Value &respValue)
{
  respValue["cmd"] = "activeRuleRsp";
  if (reqValue.isMember("id") && reqValue["id"].isString() &&
      reqValue.isMember("status") && reqValue["status"].isInt())
  {
    const string id = reqValue["id"].asString();
    Rule *rule = RuleManager::getInstance()->GetRuleFromId(id);
    if (rule)
    {
      rule->SetStatus(reqValue["status"].asInt());
      respValue["data"]["code"] =
          Database::getInstance()->RuleUpdateStatus(rule);
      respValue["data"]["id"] = id;
      return CODE_OK;
    }
  }
  respValue["data"]["code"] = CODE_OK;
  return CODE_OK;
}

int Gateway::OnActionRule(Json::Value &reqValue, Json::Value &respValue)
{
  (void)respValue;
  if (reqValue.isMember("id") && reqValue["id"].isString())
  {
    Rule *rule =
        RuleManager::getInstance()->GetRuleFromId(reqValue["id"].asString());
    if (rule)
      rule->RunOutput();
  }
  return CODE_NOT_RESPONSE;
}
