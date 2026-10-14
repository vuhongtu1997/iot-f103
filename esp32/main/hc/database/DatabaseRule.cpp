#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "Base64.h"

#define TABLE_NAME "[Rule]"

static int RuleParse(sqlite3_stmt *stmt, void *ptr)
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
				string data = Util::setString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, index++)));
				int type = sqlite3_column_int(stmt, index++);
				bool enable = sqlite3_column_int(stmt, index++) ? true : false;
				uint16_t addr = sqlite3_column_int(stmt, index++);
				long create_at = sqlite3_column_int(stmt, index++);
				bool isFavorite = sqlite3_column_int(stmt, index++) ? true : false;
				bool isFirstRun = sqlite3_column_int(stmt, index++) ? true : false;
				LOGD("%s, %s, %d, %s, %d, %s, %s", id.c_str(), data.c_str(), type, enable ? "true" : "flase", addr, isFavorite ? "true" : "false", isFirstRun ? "true" : "false");

				string ruledata;
				string decode = macaron::Base64::Decode(data, ruledata);
				if (decode == "")
				{
					Json::Value ruleValue;
					if (ruleValue.parse(ruledata) && ruleValue.isObject())
					{
						Rule *rule = RuleManager::getInstance()->AddRule(ruleValue);
						if (rule)
						{
							rule->SetStatus(enable);
							rule->SetIsFavorite(isFavorite);
							// rule->Check();
						}
					}
					else
					{
						LOGW("RuleRead json format error rule: %s", ruledata.c_str());
					}
				}
				else
				{
					LOGW("Decode data err: %s", decode.c_str());
				}
			}
			else if (s == SQLITE_DONE)
			{
				return CODE_OK;
			}
			else
			{
				LOGE("RuleParse");
				return CODE_ERROR;
			}
		}
	}
	return CODE_OK;
}

int Database::RuleRead()
{
	return ReadAll(TABLE_NAME, NULL, RuleParse);
}

int Database::RuleAdd(Rule *rule, string data, int type)
{
	string sql = "INSERT OR REPLACE INTO " TABLE_NAME " (rule_id, data, type, enable, rule_addr, create_at) VALUES ('" + rule->GetId() + "','" + macaron::Base64::Encode(data) + "'," + to_string(type) + ", " + to_string(rule->GetStatus()) + ", " + to_string(rule->GetAddr()) + "," + to_string(time(NULL)) + ");";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateData(Rule *rule, string data)
{
	string sql = "UPDATE " TABLE_NAME " SET data='" + macaron::Base64::Encode(data) + "' WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateStatus(Rule *rule)
{
	string sql = "UPDATE " TABLE_NAME " SET enable=" + to_string(rule->GetStatus()) + " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateType(Rule *rule, int type)
{
	string sql = "UPDATE " TABLE_NAME " SET type=" + to_string(type) + " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateAddr(Rule *rule)
{
	string sql = "UPDATE " TABLE_NAME " SET rule_addr=" + to_string(rule->GetAddr()) + " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateFavorite(Rule *rule, bool isFavorite)
{
	string sql = "UPDATE " TABLE_NAME " SET is_favorite =" + to_string(isFavorite) + " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleUpdateFirstRun(Rule *rule, bool isFirstRun)
{
	string sql = "UPDATE " TABLE_NAME " SET is_first_run =" + to_string(isFirstRun) + " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleDel(Rule *rule)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE rule_id='" + rule->GetId() + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleDel(string id)
{
	string sql = "DELETE FROM " TABLE_NAME " WHERE rule_id='" + id + "';";
	return Sqlite_Exec(sql);
}

int Database::RuleDelAll()
{
	string sql = "DELETE FROM " TABLE_NAME ";";
	return Sqlite_Exec(sql);
}
