#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"
// #include "DeviceBleSeftPowerRemote.h"

#define TABLE_NAME "[DeviceBleChild]"

static int DeviceBleChildParse(sqlite3_stmt *stmt, void *ptr)
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
				string deviceId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string parentId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("deviceId: %s, parentId: %s, data: %s", deviceId.c_str(), parentId.c_str(), data.c_str());
				string devData;
				string decode = macaron::Base64::Decode(data, devData);
				Json::Value dataJson;
				dataJson.parse(devData);
				/*
				{
					"device_id": "1780587558227",
					"devices": "2|BLEHGAA0302|781C3C043396|8C01|64C41F40A7D28B3C58E39E57E213DEF6|NA",
					"dictDPs": {
						"1": 21,
						"2": 1,
						"3": 2,
						"4": true,
						"101": "0",
						"106": "e8ff0000000000008C01E4110200000402021500010000"
					},
					"gateWay": "0A00",
					"indexDevice": "0200",
					"led": 1,
					"mac": "781c3c043396",
					"name": "TV ",
					"parentID": "781c3c043396",
					"pid": "BLEHGAA0302",
					"state": 2
					}
				 */
				string type = dataJson["pid"].asString();
				string name = dataJson["name"].asString();
				uint16_t addr = strtoul(dataJson["indexDevice"].asString().c_str(), NULL, 16);
				Device *device = DeviceManager::getInstance()->AddDeviceNoResetWithId(deviceId, deviceId, type, name, addr, 1, &dataJson);
				// Device *child = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
				// Device *parent = DeviceManager::getInstance()->GetDeviceFromId(parentId);
				// if (parent)
				// {
				// 	if (child)
				// 	{
				// 		DeviceBleSeftPowerRemote *deviceBleSeftPowerRemote = dynamic_cast<DeviceBleSeftPowerRemote *>(child);
				// 		if (deviceBleSeftPowerRemote)
				// 			deviceBleSeftPowerRemote->SetParentDev(parent);
				// 	}
				// 	else
				// 		LOGW("child device not found");
				// }
				// else
				// 	LOGW("parent device not found");
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("Device child parese");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::DeviceBleChildRead()
{
	return ReadAll(TABLE_NAME, NULL, DeviceBleChildParse);
}

int Database::DeviceBleChildAdd(Device *child, Device *parent, string data)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (device_id, parent_id, data) VALUES ('" + child->GetId() + "','" + parent->GetId() + "','" + data + "');";
	return Sqlite_Exec(sql);
}

int Database::DeviceBleChildAdd(string childId, string parentId, string data)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (device_id, parent_id, data) VALUES ('" + childId + "','" + parentId + "','" + macaron::Base64::Encode(data) + "');";
	return Sqlite_Exec(sql);
}

int Database::DeviceBleChildUpdateData(Device *child, Device *parent, string data)
{
	string sql = "UPDATE " TABLE_NAME " SET data='" + data + "' where device_id = '" + child->GetId() + "' AND parent_id = '" + parent->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceBleChildDel(Device *child, Device *parent)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE device_id ='" + child->GetId() + "' AND parent_id='" + parent->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceBleChildDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
