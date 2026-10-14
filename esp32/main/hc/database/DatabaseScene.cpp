#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"

#define TABLE_NAME "[Scene]"

static int SceneParse(sqlite3_stmt *stmt, void *ptr)
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
				string sceneId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				uint16_t addr = sqlite3_column_int(stmt, index++);
				string name = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				string roomId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				bool isFavorite = sqlite3_column_int(stmt, index++) ? true : false;
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %d, %s, %s", sceneId.c_str(), addr, name.c_str(), roomId.c_str());

				Scene *scene = new Scene(sceneId, addr, name);
				if (scene)
				{
					scene->SetIsFavorite(isFavorite);
					if (!data.empty())
					{
						string sceneData;
						string decode = macaron::Base64::Decode(data, sceneData);
						Json::Value sceneValue;
						if (decode.empty() && sceneValue.parse(sceneData) &&
							sceneValue.isObject())
						{
							scene->SetData(sceneValue);
						}
						else
						{
							LOGW("Invalid scene data for %s", sceneId.c_str());
						}
					}
					SceneManager::getInstance()->AddScene(scene);
					Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
					if (room)
					{
						room->AddScene(scene);
					}
				}
			}
			else if (s == SQLITE_DONE)
			{
				return 0;
			}
			else
			{
				LOGE("GroupParse");
				return 1;
			}
		}
	}
	return 0;
}

int Database::SceneRead()
{
	return ReadAll(TABLE_NAME, NULL, SceneParse);
}

int Database::SceneAdd(Scene *scene)
{
	string data = macaron::Base64::Encode(scene->GetData().toString());
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME
				 " (scene_id,scene_addr,name,create_at,data) VALUES ('" +
				 scene->GetId() + "'," + to_string(scene->GetAddr()) + ",'" +
				 scene->GetName() + "'," + to_string(time(NULL)) + ",'" +
				 data + "');";
	return Sqlite_Exec(sql);
}

int Database::SceneUpdate(Scene *scene)
{
	string sql = "UPDATE " TABLE_NAME " SET name='" + scene->GetName() + "' WHERE scene_id='" + scene->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::SceneUpdateRoom(Scene *scene, string roomId)
{
	string sql = "UPDATE " TABLE_NAME " SET room_id='" + roomId + "' WHERE scene_id ='" + scene->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::SceneDel(Scene *scene)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE scene_id='" + scene->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::SceneDel(string id)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE scene_id='" + id + "';";
	return Sqlite_Exec(sql);
}

int Database::SceneDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}

int Database::SceneUpdateFavorite(Scene *scene)
{
	string sql = "UPDATE " TABLE_NAME " SET is_favorite= " + to_string(scene->GetIsFavorite()) + " WHERE scene_id='" + scene->GetId() + "';";
	return Sqlite_Exec(sql);
}
