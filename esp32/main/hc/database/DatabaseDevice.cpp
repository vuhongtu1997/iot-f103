#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"

#define TABLE_NAME "[Device]"

static int DeviceParse(sqlite3_stmt *stmt, void *ptr)
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
				uint16_t addr = sqlite3_column_int(stmt, index++);
				string type = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t firmwareVersion = sqlite3_column_int(stmt, index++);
				uint16_t hardwareVersion = sqlite3_column_int(stmt, index++);
				uint32_t activeTime = sqlite3_column_int(stmt, index++);
				uint32_t updateTime = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %s, %s, 0X%04X, 0X%04X, %d, %d, %d, %d, %s", mac.c_str(), id.c_str(), name.c_str(), addr, type, firmwareVersion, hardwareVersion, activeTime, updateTime, data.c_str());

				bool isFavorite = sqlite3_column_int(stmt, index++) ? true : false;
				string devData;
				string decode = macaron::Base64::Decode(data, devData);
				Json::Value dataJson;
				dataJson.parse(devData);
				Device *device = DeviceManager::getInstance()->AddDeviceNoResetWithId(id, mac, type, name, addr, firmwareVersion, &dataJson);
				if (device)
					device->SetIsFavorite(isFavorite);
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("DeviceParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::DeviceRead()
{
	return ReadAll(TABLE_NAME, NULL, DeviceParse);
}

int Database::DeviceAdd(Device *device)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (device_id, name, mac, data, addr, type, firmware_version, active_time) VALUES ('" + device->GetId() + "','" + device->GetName() + "','" + device->GetMac() + "','" + macaron::Base64::Encode(device->GetData().toString()) + "'," + to_string(device->GetAddr()) + ",'" + device->GetType() + "'," + to_string(device->GetVersion()) + "," + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::DeviceUpdate(Device *device)
{
	string sql = "UPDATE " TABLE_NAME " SET name='" + device->GetName() + "', addr=" + to_string(device->GetAddr()) + " WHERE device_id='" + device->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceUpdateData(Device *device)
{
	string sql = "UPDATE " TABLE_NAME " SET data='" + macaron::Base64::Encode(device->GetData().toString()) + "' WHERE device_id='" + device->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceDel(Device *device)
{
	return DeviceDel(device->GetId());
}

int Database::DeviceDel(string id)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE device_id='" + id + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}

int Database::DelDevExist(Device *device)
{
	string sql = "DELETE FROM DeviceAttribute WHERE device_id = '" + device->GetId() + "';";
	Sqlite_Exec(sql);
	return CODE_OK;
}

int Database::DeviceUpdateFavorite(Device *device)
{
	string sql = "UPDATE " TABLE_NAME " SET is_favorite= " + to_string(device->GetIsFavorite()) + " WHERE device_id='" + device->GetId() + "';";
	return Sqlite_Exec(sql);
}
