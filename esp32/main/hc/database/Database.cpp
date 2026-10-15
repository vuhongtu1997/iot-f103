#include "Database.h"
#include "Log.h"
#include "Util.h"
#include "esp_partition.h"
#include <stdlib.h>
#include <sys/stat.h>

#ifdef ESP_PLATFORM
#ifdef CONFIG_ENABLE_LITTLEFS
#include "esp_littlefs.h"
#else
#include "esp_spiffs.h"
#endif
#endif

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

#ifdef ESP_PLATFORM
static bool IsPartitionErased(const esp_partition_t *partition)
{
    if (partition == NULL)
        return false;

    const size_t bufferSize = 4096;
    uint8_t *buffer = static_cast<uint8_t *>(malloc(bufferSize));
    if (buffer == NULL)
    {
        LOGE("Cannot allocate buffer to inspect storage partition");
        return false;
    }

    bool erased = true;
    for (size_t offset = 0; offset < partition->size && erased;
         offset += bufferSize)
    {
        const size_t remaining = partition->size - offset;
        const size_t readSize =
            remaining < bufferSize ? remaining : bufferSize;
        esp_err_t err = esp_partition_read(partition, offset, buffer, readSize);
        if (err != ESP_OK)
        {
            LOGE("Cannot inspect storage partition at 0x%x: %s",
                 static_cast<unsigned int>(offset), esp_err_to_name(err));
            erased = false;
            break;
        }

        for (size_t i = 0; i < readSize; ++i)
        {
            if (buffer[i] != 0xFF)
            {
                erased = false;
                break;
            }
        }
    }

    free(buffer);
    return erased;
}
#endif

Database *Database::getInstance()
{
	static Database *database = NULL;
	if (!database)
	{
		database = new Database();
	}
	return database;
}

Database::Database() : db(NULL)
{
	LOGI("Init db");
}

Database::~Database()
{
	if (db)
		sqlite3_close_v2(db);
}

void Database::Init(void)
{
#ifdef ESP_PLATFORM
#ifdef CONFIG_ENABLE_LITTLEFS
    LOGI("Initializing LITTLEFS");
    esp_vfs_littlefs_conf_t conf = {
#else
    LOGI("Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
#endif
        .base_path = "/littlefs",
        .partition_label = "storage",
#ifndef CONFIG_ENABLE_LITTLEFS
        .max_files = 5,
#endif
        .format_if_mount_failed = false,
#ifdef CONFIG_ENABLE_LITTLEFS
        .dont_mount = false,
#endif
    };

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        conf.partition_label);

    if (partition == NULL)
    {
        LOGE("Storage partition NOT FOUND!");
    }
    else
    {
        LOGI("Found storage partition: offset=0x%lx, size=%lu",
             partition->address, partition->size);
    }

#ifdef CONFIG_ENABLE_LITTLEFS
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
#else
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
#endif

    if (ret != ESP_OK && IsPartitionErased(partition))
    {
        LOGW("Storage is blank after flash erase; creating a new filesystem");
        conf.format_if_mount_failed = true;
#ifdef CONFIG_ENABLE_LITTLEFS
        ret = esp_vfs_littlefs_register(&conf);
#else
        ret = esp_vfs_spiffs_register(&conf);
#endif
    }

    if (ret != ESP_OK)
    {
        LOGE("Storage mount failed: %s. Existing data was not formatted",
             esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
#ifdef CONFIG_ENABLE_LITTLEFS
    esp_littlefs_info(conf.partition_label, &total, &used);
#else
    esp_spiffs_info(conf.partition_label, &total, &used);
#endif

    LOGI("FS size: total=%d, used=%d", total, used);

    sqlite3_initialize();
#endif

    // 🔥 XÓA DB nếu bị lỗi cũ (tránh corrupt kéo dài)
    bool dbExisted = IsHaveDb(DB_NAME);
    LOGW("SQLite DB path: %s, existed=%d", DB_NAME, dbExisted ? 1 : 0);
    if (!dbExisted)
    {
        LOGW("DB not found → will create new");
    }

    int openRc = sqlite3_open_v2(DB_NAME, &db,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                 0);

    if (openRc != SQLITE_OK || db == NULL)
    {
        LOGE("Failed to open DB: %s",
             db ? sqlite3_errmsg(db) : "db handle null");
        if (db)
        {
            sqlite3_close_v2(db);
            db = NULL;
        }
        return;
    }

    LOGW("SQLite DB opened: %s", DB_NAME);

    sqlite3_busy_timeout(db, 3000);

    // PRAGMA toi uu cho flash.
    sqlite3_exec(db, "PRAGMA journal_mode=DELETE;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous=FULL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA locking_mode=EXCLUSIVE;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA cache_size=2000;", NULL, NULL, NULL);

    int createRc = CreateTableIfNotExists();
    if (createRc == SQLITE_OK)
    {
        LOGW("SQLite DB ready: tables created/verified");
    }
    else
    {
        LOGE("SQLite DB table init failed, rc=%d", createRc);
    }
}


bool Database::EnsureReady()
{
	if (db != NULL)
		return true;

	sqlite3_initialize();
	if (sqlite3_open_v2(DB_NAME, &db,
						SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
							SQLITE_OPEN_MAIN_JOURNAL,
						0) != SQLITE_OK)
	{
		LOGE("Failed to open database: %s, Error: %s", DB_NAME, sqlite3_errmsg(db));
		db = NULL;
		return false;
	}

	CreateTableIfNotExists();
	return true;
}

bool Database::IsHaveDb(const char *dbName)
{
	struct stat st;
	return !stat(dbName, &st);
}

int Database::CreateTableIfNotExists()
{
	const char *statements[] = {
		"CREATE TABLE IF NOT EXISTS Device (mac VARCHAR, device_id VARCHAR NOT NULL, name VARCHAR, addr INTEGER, type VARCHAR, firmware_version INTEGER, hardware_version INTEGER, active_time INTEGER, update_time INTEGER, data TEXT, is_favorite BOOLEAN, PRIMARY KEY (device_id)) WITHOUT ROWID;",
		"CREATE TABLE IF NOT EXISTS DeviceAttribute (device_id VARCHAR NOT NULL, attribute TEXT, value DOUBLE, PRIMARY KEY (device_id, attribute)) WITHOUT ROWID;",
		"CREATE TABLE IF NOT EXISTS DeviceHistory (id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER, cause_type INTEGER, cause_id TEXT, event_type INTEGER, device_id TEXT, dp_id INTEGER, dp_value TEXT);",
		"CREATE INDEX IF NOT EXISTS DeviceHistoryTimeDevice ON DeviceHistory(device_id, time DESC);",
		"CREATE TABLE IF NOT EXISTS Gateway (mac VARCHAR NOT NULL ,gateway_id VARCHAR, name VARCHAR, version VARCHAR, ble_netkey VARCHAR, ble_appkey VARCHAR, ble_devicekey VARCHAR, ble_addr INTEGER, ble_iv_index INTEGER, dormitory TEXT, refresh_token TEXT, zigbee_netkey VARCHAR, create_at INTEGER, data TEXT, PRIMARY KEY (mac)) WITHOUT ROWID;",
		"CREATE TABLE IF NOT EXISTS Rule (rule_id VARCHAR NOT NULL, data TEXT NOT NULL, type INTEGER, enable BOOLEAN, rule_addr INTEGER, create_at INTEGER, is_favorite BOOLEAN, is_first_run BOOLEAN, PRIMARY KEY (rule_id)) WITHOUT ROWID;",
		};

	int rc = SQLITE_OK;
	for (size_t i = 0; i < sizeof(statements) / sizeof(statements[0]); ++i)
	{
		string sql = statements[i];
		LOGI("Creating table with SQL: %s", sql.c_str());
		rc = Sqlite_Exec(sql);
		if (rc != SQLITE_OK)
		{
			return rc;
		}
	}
	return rc;
}

int Database::DeviceHistoryAdd(const string &deviceId, int dpId,
							   const string &dpValue, int causeType,
							   const string &causeId, int eventType)
{
	if (!EnsureReady())
		return CODE_DATABASE_ERROR;

	const char *sql =
		"INSERT INTO DeviceHistory "
		"(time, cause_type, cause_id, event_type, device_id, dp_id, dp_value) "
		"VALUES (?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt *stmt = nullptr;
	int rc;

	lock_guard<recursive_mutex> lock(mtx);
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int64(stmt, 1,
						  static_cast<sqlite3_int64>(Util::GetTimeMillis()));
		sqlite3_bind_int(stmt, 2, causeType);
		sqlite3_bind_text(stmt, 3, causeId.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, eventType);
		sqlite3_bind_text(stmt, 5, deviceId.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 6, dpId);
		sqlite3_bind_text(stmt, 7, dpValue.c_str(), -1, SQLITE_TRANSIENT);
		rc = sqlite3_step(stmt);
	}

	if (stmt)
		sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE)
	{
		LOGE("DeviceHistoryAdd failed: %s", sqlite3_errmsg(db));
		return CODE_DATABASE_ERROR;
	}
	return CODE_OK;
}

int Database::DeviceHistoryFind(const string &deviceId, uint64_t startTime,
								uint64_t endTime, const vector<int> &dpIds,
								int causeType, int eventType, int limit,
								Json::Value &rows)
{
	if (!EnsureReady())
		return CODE_DATABASE_ERROR;

	if (limit <= 0)
		limit = 20;
	if (limit > 100)
		limit = 100;

	string sql =
		"SELECT time, cause_type, cause_id, event_type, dp_id, dp_value "
		"FROM DeviceHistory WHERE time >= ? AND time <= ? AND device_id = ?";
	if (!dpIds.empty())
	{
		sql += " AND dp_id IN (";
		for (size_t i = 0; i < dpIds.size(); ++i)
			sql += i == 0 ? "?" : ",?";
		sql += ")";
	}
	if (causeType >= 0)
		sql += " AND cause_type = ?";
	if (eventType >= 0)
		sql += " AND event_type = ?";
	sql += " ORDER BY time DESC LIMIT ?;";

	rows = Json::arrayValue;
	sqlite3_stmt *stmt = nullptr;
	int rc;

	lock_guard<recursive_mutex> lock(mtx);
	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK)
	{
		LOGE("DeviceHistoryFind prepare failed: %s", sqlite3_errmsg(db));
		return CODE_DATABASE_ERROR;
	}

	int bindIndex = 1;
	sqlite3_bind_int64(stmt, bindIndex++,
					   static_cast<sqlite3_int64>(startTime));
	sqlite3_bind_int64(stmt, bindIndex++,
					   static_cast<sqlite3_int64>(endTime));
	sqlite3_bind_text(stmt, bindIndex++, deviceId.c_str(), -1,
					  SQLITE_TRANSIENT);
	for (int dpId : dpIds)
		sqlite3_bind_int(stmt, bindIndex++, dpId);
	if (causeType >= 0)
		sqlite3_bind_int(stmt, bindIndex++, causeType);
	if (eventType >= 0)
		sqlite3_bind_int(stmt, bindIndex++, eventType);
	sqlite3_bind_int(stmt, bindIndex, limit);

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		Json::Value row;
		row["time"] = Json::Value(
			static_cast<Json::UInt64>(sqlite3_column_int64(stmt, 0)));
		row["causeType"] = sqlite3_column_int(stmt, 1);
		const unsigned char *causeIdValue = sqlite3_column_text(stmt, 2);
		row["causeId"] =
			causeIdValue ? reinterpret_cast<const char *>(causeIdValue) : "";
		row["eventType"] = sqlite3_column_int(stmt, 3);
		row["dpId"] = sqlite3_column_int(stmt, 4);
		const unsigned char *dpValue = sqlite3_column_text(stmt, 5);
		row["dpValue"] =
			dpValue ? reinterpret_cast<const char *>(dpValue) : "";
		rows.append(row);
	}

	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE)
	{
		LOGE("DeviceHistoryFind failed: %s", sqlite3_errmsg(db));
		return CODE_DATABASE_ERROR;
	}
	return CODE_OK;
}

static int sqlite_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	for (i = 0; i < argc; i++)
	{
		LOGD("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	LOGD("\n");
	return CODE_OK;
}

int Database::SqliteBenginTransaction()
{
	int rc = SQLITE_ERROR;
	char *err_msg = 0;
	mtx.lock();
	rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
	if (rc != SQLITE_OK)
	{
		LOGE("Error executing sql statement: %s",
			 err_msg ? err_msg : "unknown error");
		if (err_msg)
			sqlite3_free(err_msg);
		mtx.unlock();
	}
	return rc;
}

int Database::SqliteEndTransaction()
{
	int rc = SQLITE_ERROR;
	char *err_msg = 0;
	rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &err_msg);
	if (rc != SQLITE_OK)
	{
		LOGE("Error executing sql statement: %s",
			 err_msg ? err_msg : "unknown error");
		if (err_msg)
			sqlite3_free(err_msg);
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
	}
	mtx.unlock();
	return rc;
}

int Database::SqliteRollbackTransaction()
{
	int rc = SQLITE_ERROR;
	char *err_msg = 0;
	rc = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &err_msg);
	if (rc != SQLITE_OK)
	{
		LOGE("Error rolling back SQLite transaction: %s",
			 err_msg ? err_msg : "unknown error");
		if (err_msg)
			sqlite3_free(err_msg);
	}
	mtx.unlock();
	return rc;
}

int Database::Sqlite_Exec(string &sql)
{
	if (db == NULL)
	{
		LOGE("Database not initialized, cannot execute: %s", sql.c_str());
		return SQLITE_ERROR;
	}
	int rc = SQLITE_ERROR;
	char *err_msg = 0;
	mtx.lock();
	rc = sqlite3_exec(db, sql.c_str(), sqlite_callback, NULL, &err_msg);
	if (rc != SQLITE_OK)
	{
		LOGE("Error executing sql statement: %s", err_msg ? err_msg : "unknown error");
		if (err_msg)
			sqlite3_free(err_msg);
	}
	mtx.unlock();
	return rc;
}

int Database::ReadAll(string table, void *listPtr, int (*Parse)(sqlite3_stmt *, void *))
{
	int rc = SQLITE_ERROR;
	sqlite3_stmt *stmt;
	string sql = "SELECT * FROM " + table + ";";

	if (!Parse)
	{
		LOGW("Parse func NULL");
		return CODE_ERROR;
	}

	LOGD("ReadAll table %s", table.c_str());
	if (db == NULL)
	{
		LOGE("Database not initialized, cannot read table: %s", table.c_str());
		return SQLITE_ERROR;
	}
	mtx.lock();
	rc = sqlite3_prepare_v2(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		LOGD("sqlite3_prepare_v2 successfully");
		Parse(stmt, listPtr);
		sqlite3_finalize(stmt);
	}
	else
	{
		LOGW("SQL error: %d - %s", rc, sqlite3_errmsg(db));
	}
	mtx.unlock();
	SLEEP_MS(10);
	return rc;
}

void Database::DelDatabase()
{
	if (db)
	{
		sqlite3_close_v2(db);
		db = NULL;
	}
#ifdef ESP_PLATFORM
	if (unlink(DB_NAME) != 0)
	{
		LOGE("Failed to delete file\n");
	}
#ifdef CONFIG_ENABLE_LITTLEFS
	esp_vfs_littlefs_unregister(NULL);
#else
	esp_vfs_spiffs_unregister(NULL);
#endif
#else
	string rmDb = "rm " DB_NAME;
	system(rmDb.c_str());
#endif
}
