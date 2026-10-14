#include "Database.h"
#include "Log.h"
#include "Util.h"

#define TABLE_NAME "[DeviceInRoom]"

static int DeviceInRoomParse(sqlite3_stmt *stmt, void *ptr)
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
				string roomId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string deviceId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t element = sqlite3_column_int(stmt, index++);
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %s", roomId.c_str(), deviceId.c_str());

				Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
				Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
				if (room)
				{
					if (device)
					{
						room->AddDevice(device, element);
					}
					else
						LOGW("Device not found %s", deviceId.c_str());
				}
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("DeviceInRoomParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::DeviceInRoomRead()
{
	return ReadAll(TABLE_NAME, NULL, DeviceInRoomParse);
}

int Database::DeviceInRoomAdd(Room *room, Device *device, uint16_t epId)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (room_id, device_id, element, create_at) VALUES ('" + room->GetId() + "','" + device->GetId() + "'," + to_string(epId) + ", " + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::DeviceInRoomDel(Room *room, Device *device, uint16_t epId)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE room_id= '" + room->GetId() + "' AND device_id='" + device->GetId() + "' AND element = " + to_string(epId) + ";";
	return Sqlite_Exec(sql);
}

int Database::DeviceInRoomDelDev(string deviceId)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE device_id='" + deviceId + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceInRoomDelDev(Device *device)
{
	return DeviceInRoomDelDev(device->GetId());
}

int Database::DeviceInRoomDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME " ; ";
	return Sqlite_Exec(sql);
}
