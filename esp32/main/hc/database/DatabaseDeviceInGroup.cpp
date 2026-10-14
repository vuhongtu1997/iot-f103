#include "Database.h"
#include "Log.h"
#include "Util.h"

#define TABLE_NAME "[DeviceInGroup]"

static int DeviceInGroupParse(sqlite3_stmt *stmt, void *ptr)
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
				string groupId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string deviceId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t element = sqlite3_column_int(stmt, index++);
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %s, %d", groupId.c_str(), deviceId.c_str(), element);

				Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
				Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
				if (group)
				{
					if (device)
					{
						group->AddDevice(device, element);
					}
					else
					{
						LOGW("Device not found: %s", deviceId.c_str());
					}
				}
				else
					LOGW("Group not found: %s", groupId.c_str());
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("DeviceInGroupParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::DeviceInGroupRead()
{
	return ReadAll(TABLE_NAME, NULL, DeviceInGroupParse);
}

int Database::DeviceInGroupAdd(Group *group, Device *device, uint16_t epId)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (group_id, device_id, element, create_at) VALUES ('" + group->GetId() + "','" + device->GetId() + "'," + to_string(epId) + "," + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::DeviceInGroupDel(Group *group, Device *device, uint16_t epId)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE group_id= '" + group->GetId() + "' AND device_id='" + device->GetId() + "' AND element = " + to_string(epId) + ";";
	return Sqlite_Exec(sql);
}

int Database::DeviceInGroupDelDev(string deviceId)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE device_id='" + deviceId + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceInGroupDelDev(Device *device)
{
	return DeviceInGroupDelDev(device->GetId());
}

int Database::DeviceInGroupDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
