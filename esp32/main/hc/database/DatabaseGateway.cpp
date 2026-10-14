#include "Database.h"
#include "Log.h"
#include "Util.h"

#define TABLE_NAME "[Gateway]"

static int GatewayParse(sqlite3_stmt *stmt, void *ptr)
{
	int s, index;
	if (stmt)
	{
		while (1)
		{
			s = sqlite3_step(stmt);
			if (s == SQLITE_ROW)
			{
				index = 0;
				string mac = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string id = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string name = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string version = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string ble_netkey = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string ble_appkey = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string ble_devicekey = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t ble_addr = sqlite3_column_int(stmt, index++);
				uint32_t ble_iv_index = sqlite3_column_int(stmt, index++);
				string dormitory = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string refresh = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string zigbee_netkey = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));

				// Gateway::getInstance()->SetMac(mac);
				Gateway::getInstance()->SetId(id);
				Gateway::getInstance()->SetName(name);
				Gateway::getInstance()->SetBleNetkey(ble_netkey);
				Gateway::getInstance()->SetBleAppkey(ble_appkey);
				Gateway::getInstance()->SetBleDevicekey(ble_devicekey);
				Gateway::getInstance()->SetBleAddr(ble_addr);
				Gateway::getInstance()->SetBleIvIndex(ble_iv_index);
				Gateway::getInstance()->SetDormitory(dormitory);
				Gateway::getInstance()->SetRefreshToken(refresh);
				Gateway::getInstance()->SetData(data);
				Gateway::getInstance()->SetVersion(version);

			LOGI("Gateway mac: %s, id: %s, ble_addr: %u, ble_iv_index: %lu, dormitory: %s",
				 Gateway::getInstance()->GetMac().c_str(),
				 Gateway::getInstance()->GetId().c_str(),
				 Gateway::getInstance()->GetBleAddr(),
				 static_cast<unsigned long>(Gateway::getInstance()->GetBleIvIndex()),
				 Gateway::getInstance()->GetDormitory().c_str());

				return CODE_OK;
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("GatewayParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::GatewayRead()
{
	return ReadAll(TABLE_NAME, NULL, GatewayParse);
}

int Database::GatewayAdd()
{
	string sql = "INSERT OR IGNORE INTO " TABLE_NAME " (mac, gateway_id, name, version, ble_netkey, ble_appkey, ble_devicekey, ble_addr, ble_iv_index, dormitory,data) VALUES ('" + Gateway::getInstance()->GetMac() + "','" + Gateway::getInstance()->GetId() + "','" + Gateway::getInstance()->GetName() + "','" + Gateway::getInstance()->GetVersion() + "','" + Gateway::getInstance()->GetBleNetKey() + "','" + Gateway::getInstance()->GetBleAppKey() + "','" + Gateway::getInstance()->GetBleDeviceKey() + "'," + to_string(Gateway::getInstance()->GetBleAddr()) + "," + to_string(Gateway::getInstance()->GetBleIvIndex()) + ",'','" + Gateway::getInstance()->GetData() + "');";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateId(string id)
{
	string sql = "UPDATE " TABLE_NAME " SET gateway_id = '" + id + "' WHERE mac = '" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateNetKey(string netkey)
{
	string sql = "UPDATE " TABLE_NAME " SET ble_netkey='" + netkey + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateAppKey(string appkey)
{
	string sql = "UPDATE " TABLE_NAME " SET ble_appkey='" + appkey + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateDeviceKey(string devicekey)
{
	string sql = "UPDATE " TABLE_NAME " SET ble_devicekey='" + devicekey + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateUnicast(uint16_t unicast)
{
	string sql = "UPDATE " TABLE_NAME " SET ble_addr =" + to_string(unicast) + " WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateIvIndex(uint32_t iv_index)
{
	string sql = "UPDATE " TABLE_NAME " SET ble_iv_index =" + to_string(iv_index) + " WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateDormitory(string dormitory)
{
	string sql = "UPDATE " TABLE_NAME " SET dormitory='" + dormitory + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateRefreshToken(string refreshToken)
{
	string sql = "UPDATE " TABLE_NAME " SET refresh_token='" + refreshToken + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateData(string data)
{
	string sql = "UPDATE " TABLE_NAME " SET data='" + data + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayUpdateVersion(string version)
{
	string sql = "UPDATE " TABLE_NAME " SET version='" + version + "' WHERE mac='" + Gateway::getInstance()->GetMac() + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayDel()
{
	return GatewayDel(Gateway::getInstance()->GetMac());
}

int Database::GatewayDel(string mac)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE mac='" + mac + "';";
	return Sqlite_Exec(sql);
}

int Database::GatewayDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
