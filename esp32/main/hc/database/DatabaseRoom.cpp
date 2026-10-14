#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"

#define TABLE_NAME "[Room]"

static int RoomParse(sqlite3_stmt *stmt, void *ptr)
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
				uint16_t addr = sqlite3_column_int(stmt, index++);
				string name = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %d, %s, %s", roomId.c_str(), addr, name.c_str(), data.c_str());

				Room *room = new Room(roomId, addr, name);
				if (room)
				{
					string roomData;
					string decode = macaron::Base64::Decode(data, roomData);
					if (decode == "")
					{
						Json::Value roomValue;
						if (roomValue.parse(roomData) && roomValue.isObject())
						{
							room->SetDataConfig(roomValue.toString());
						}
					}
					else
					{
						LOGW("Decode data error: %s", data.c_str());
					}
					RoomManager::getInstance()->AddRoom(room);
				}
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("RoomParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::RoomRead()
{
	return ReadAll(TABLE_NAME, NULL, RoomParse);
}

int Database::RoomAdd(Room *room)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (room_id, room_addr, name, data, create_at) VALUES ('" + room->GetId() + "'," + to_string(room->GetAddr()) + ",'" + room->GetName() + "','" + macaron::Base64::Encode(room->GetDataConfig()) + "'," + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::RoomUpdateName(Room *room)
{
	string sql = "UPDATE " TABLE_NAME " SET name='" + room->GetName() + "' WHERE room_id = '" + room->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RoomUpdateData(Room *room)
{
	string sql = "UPDATE " TABLE_NAME " SET data='" + macaron::Base64::Encode(room->GetDataConfig()) + "' WHERE room_id = '" + room->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RoomDel(Room *room)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE room_id = '" + room->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RoomDel(string id)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE room_id = '" + id + "';";
	return Sqlite_Exec(sql);
}

int Database::RoomDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
