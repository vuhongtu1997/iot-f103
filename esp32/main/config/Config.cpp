#include <iostream>
#include <endian.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "Config.h"
#include "Log.h"
#include <vector>

using namespace std;

namespace
{
	constexpr size_t PROVISION_JSON_MAX_SIZE = 3072;

	std::shared_ptr<nvs::NVSHandle> openConfigNvs(esp_err_t &err)
	{
		return nvs::open_nvs_handle("nvs", NVS_READWRITE, &err);
	}

	bool erase_config_entry(const char *name)
	{
		esp_err_t err;
		std::shared_ptr<nvs::NVSHandle> handle = openConfigNvs(err);
		if (err != ESP_OK)
		{
			LOGE("Error (%s) opening NVS handle", esp_err_to_name(err));
			return false;
		}
		esp_err_t eraseErr = handle->erase_item(name);
		if (eraseErr == ESP_OK || eraseErr == ESP_ERR_NVS_NOT_FOUND)
		{
			return handle->commit() == ESP_OK;
		}
		return false;
	}

	string get_string_config_entry(const char *name, size_t maxLen)
	{
		std::vector<char> buffer(maxLen, 0);
		esp_err_t err;
		std::shared_ptr<nvs::NVSHandle> handle = openConfigNvs(err);
		if (err != ESP_OK)
		{
			LOGE("Error (%s) opening NVS handle", esp_err_to_name(err));
			return "";
		}
		if (handle->get_string(name, buffer.data(), buffer.size()) == ESP_OK)
		{
			return string(buffer.data());
		}
		return "";
	}
} // namespace

Config *Config::getInstance()
{
	static Config *config = NULL;
	if (!config)
	{
		config = new Config();
	}
	return config;
}

static bool set_str_config_entry(const char *name, const char *section_name, const char *value)
{
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = openConfigNvs(err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		err = handle->set_string(name, value);
		if (err == ESP_OK)
		{
			err = handle->commit();
			if (err == ESP_OK)
				return true;
		}
		LOGE("NVS set/commit failed for %s: %s", name, esp_err_to_name(err));
	}
	return false;
}

static bool set_int_config_entry(const char *section, const char *name, int value)
{
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = openConfigNvs(err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		if (handle->set_item(name, value) == ESP_OK)
			if (handle->commit() == ESP_OK)
				return true;
	}
	return false;
}

Config::Config()
{
}

Config::~Config()
{
	LOGW("Delete config object");
}

void Config::ReadConfig()
{
	char str_temp[STRING_VALUE_MAX_SIZE];
	int int_temp = 0;
	printf("Opening Non-Volatile Storage (NVS) handle... ");
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = openConfigNvs(err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		if (handle->get_string(HOST_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			host = string(str_temp);
		else
			host = HOST_DEFAULT;

		if (handle->get_item(PORT_KEY, int_temp) == ESP_OK)
			port = int_temp;
		else
			port = PORT_DEFAULT;

		if (handle->get_string(CLIENT_ID_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			clientId = string(str_temp);
		else
			clientId = CLIENT_ID_DEFAULT;

		if (handle->get_string(USERNAME_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			username = string(str_temp);
		else
			username = USERNAME_DEFAULT;

		if (handle->get_string(PASSWORD_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			password = string(str_temp);
		else
			password = PASSWORD_DEFAULT;

		if (handle->get_item(KEEP_ALIVE_KEY, int_temp) == ESP_OK)
			keepAlive = int_temp;
		else
			keepAlive = KEEP_ALIVE_DEFAULT;
	}
	Print();
}

void Config::Print()
{
	if (host == "")
		LOGE("Server NULL");
	LOGI("host: %s", host.c_str());
	LOGI("port: %d", port);
	LOGI("clientId: %s", clientId.c_str());
	LOGI("username: %s", username.c_str());
	LOGI("password: %s", password.c_str());
	LOGI("keepAlive: %d", keepAlive);
}

string Config::GetHost()
{
	return host;
}

int Config::GetPort()
{
	return port;
}

string Config::GetClientId()
{
	return clientId;
}

string Config::GetUsername()
{
	return username;
}

string Config::GetPassword()
{
	return password;
}

int Config::GetKeepAlive()
{
	return keepAlive;
}

bool Config::GetSsl()
{
	return false;
}

bool Config::SetHost(string host)
{
	if (set_str_config_entry(HOST_KEY, HOST_KEY, host.c_str()))
	{
		this->host = host;
		return true;
	}
	return false;
}

bool Config::SetPort(int port)
{
	if (set_int_config_entry(PORT_KEY, PORT_KEY, port))
	{
		this->port = port;
		return true;
	}
	return false;
}

bool Config::SetClientId(string clientId)
{
	if (set_str_config_entry(CLIENT_ID_KEY, CLIENT_ID_KEY, clientId.c_str()))
	{
		this->clientId = clientId;
		return true;
	}
	return false;
}

bool Config::SetUsername(string username)
{
	if (set_str_config_entry(USERNAME_KEY, USERNAME_KEY, username.c_str()))
	{
		this->username = username;
		return true;
	}
	return false;
}

bool Config::SetPassword(string password)
{
	if (set_str_config_entry(PASSWORD_KEY, PASSWORD_KEY, password.c_str()))
	{
		this->password = password;
		return true;
	}
	return false;
}

bool Config::SetKeepAlive(int keepAlive)
{
	if (set_int_config_entry(KEEP_ALIVE_KEY, KEEP_ALIVE_KEY, keepAlive))
	{
		this->keepAlive = keepAlive;
		return true;
	}
	return false;
}

bool Config::SetSsl(bool ssl)
{
	this->ssl = ssl;
	return true;
}

bool Config::SaveProvisionConfigJson(const std::string &json)
{
	return set_str_config_entry(HC_CFG_JSON_KEY, HC_CFG_JSON_KEY, json.c_str());
}

std::string Config::GetProvisionConfigJson()
{
	return get_string_config_entry(HC_CFG_JSON_KEY, PROVISION_JSON_MAX_SIZE);
}

bool Config::SetWifiSsid(const std::string &ssid)
{
	return set_str_config_entry(WIFI_SSID_KEY, WIFI_SSID_KEY, ssid.c_str());
}

bool Config::SetWifiPassword(const std::string &password)
{
	return set_str_config_entry(WIFI_PASS_KEY, WIFI_PASS_KEY, password.c_str());
}

bool Config::SetThingId(const std::string &thingId)
{
	return set_str_config_entry(THING_ID_KEY, THING_ID_KEY, thingId.c_str());
}

std::string Config::GetWifiSsid()
{
	return get_string_config_entry(WIFI_SSID_KEY, STRING_VALUE_MAX_SIZE);
}

std::string Config::GetWifiPassword()
{
	return get_string_config_entry(WIFI_PASS_KEY, STRING_VALUE_MAX_SIZE);
}

std::string Config::GetThingId()
{
	return get_string_config_entry(THING_ID_KEY, STRING_VALUE_MAX_SIZE);
}

bool Config::ClearProvisioningData()
{
	bool ok = true;
	ok = erase_config_entry(HC_CFG_JSON_KEY) && ok;
	ok = erase_config_entry(WIFI_SSID_KEY) && ok;
	ok = erase_config_entry(WIFI_PASS_KEY) && ok;
	ok = erase_config_entry(THING_ID_KEY) && ok;
	return ok;
}
