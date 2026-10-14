#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"

#define TABLE_NAME "[DeviceInScene]"

static int DeviceInSceneParse(sqlite3_stmt *stmt, void *ptr)
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
				string deviceId = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				long create_at = sqlite3_column_int(stmt, index++);
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				LOGD("%s, %s, %s", sceneId.c_str(), deviceId.c_str(), data.c_str());

				Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
				Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
				if (scene)
				{
					if (device)
					{
						string devInSceneData;
						string decode = macaron::Base64::Decode(data, devInSceneData);
						if (decode == "")
						{
							Json::Value devInSceneJson;
							if (devInSceneJson.parse(devInSceneData) && devInSceneJson.isObject())
							{
								scene->AddDevice(device, devInSceneJson);
							}
							else
							{
								LOGE("data json is not object");
							}
							// TODO: Check cho du lieu V2
						}
						else
						{
							LOGW("Decode data error");
						}
					}
					else
						LOGW("Device not found %s", deviceId.c_str());
				}
				else
				{
					LOGW("Device or scene does not exist");
				}
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

int Database::DeviceInSceneRead()
{
	return ReadAll(TABLE_NAME, NULL, DeviceInSceneParse);
}

int Database::DeviceInSceneAdd(Scene *scene, Device *device, string data)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (scene_id, device_id, data, create_at) VALUES ('" + scene->GetId() + "','" + device->GetId() + "','" + macaron::Base64::Encode(data) + "', " + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::DeviceInSceneDel(Scene *scene, Device *device)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE scene_id= '" + scene->GetId() + "' AND device_id='" + device->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceInSceneDelDev(string deviceId)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE device_id='" + deviceId + "';";
	return Sqlite_Exec(sql);
}

int Database::DeviceInSceneDelDev(Device *device)
{
	return DeviceInSceneDelDev(device->GetId());
}

int Database::DeviceInSceneDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
