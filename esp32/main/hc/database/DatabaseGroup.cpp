#include "Database.h"
#include "Log.h"
#include "Util.h"

#define TABLE_NAME "[Group]"

static int GroupParse(sqlite3_stmt *stmt, void *ptr)
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
				string id = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t addr = sqlite3_column_int(stmt, index++);
				string name = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string roomId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %d, %s, %s", id.c_str(), addr, name.c_str(), roomId.c_str());

				Group *group = new Group(id, addr, name);
				if (group)
				{
					GroupManager::getInstance()->AddGroup(group);
					Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
					if (room)
					{
						room->AddGroup(group);
					}
				}
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("GroupParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::GroupRead()
{
	return ReadAll(TABLE_NAME, NULL, GroupParse);
}

int Database::GroupAdd(Group *group)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (group_id, name, group_addr, create_at) VALUES ('" + group->GetId() + "','" + group->GetName() + "'," + to_string(group->GetAddr()) + "," + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::GroupUpdate(Group *group)
{
	string sql = "UPDATE " TABLE_NAME " SET name='" + group->GetName() + "' WHERE group_id= '" + group->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::GroupUpdateRoom(Group *group, string roomId)
{
	string sql = "UPDATE " TABLE_NAME " SET room_id='" + roomId + "' WHERE group_id= '" + group->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::GroupDel(Group *group)
{
	return GroupDel(group->GetId());
}

int Database::GroupDel(string id)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE group_id = '" + id + "';";
	return Sqlite_Exec(sql);
}

int Database::GroupDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
