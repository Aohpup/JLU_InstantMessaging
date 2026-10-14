#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// 本文件是独立接入验证，不操作正式平台或 data/jlu_im.db。
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// 执行不返回行的 SQL；错误信息使用后释放。
void execute(sqlite3* db, const char* sql)
{
    char* error = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (result != SQLITE_OK)
    {
        const std::string message = error ? error : sqlite3_errmsg(db);
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

// 读取单个整数结果，查询结束及时释放语句。
int queryInt(sqlite3* db, const char* sql)
{
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
    const int result = sqlite3_step(statement);
    const int value = result == SQLITE_ROW ? sqlite3_column_int(statement, 0) : 0;
    sqlite3_finalize(statement);
    require(result == SQLITE_ROW, "Integer query failed.");
    return value;
}

int main()
{
    sqlite3* db = nullptr;
    sqlite3_stmt* statement = nullptr;
    try
    {
        const std::filesystem::path path = JLU_SQLITE_SMOKE_DB;
        std::filesystem::create_directories(path.parent_path());
        const std::string filename = path.u8string();
        require(sqlite3_open(filename.c_str(), &db) == SQLITE_OK, "Cannot open smoke database.");
        require(std::string(sqlite3_libversion()) == SQLITE_VERSION, "Library/header version mismatch.");
        std::ifstream input(JLU_SCHEMA_PATH, std::ios::binary);
        require(input.is_open(), "Cannot read schema/create_tables.sql.");
        std::ostringstream schema;
        schema << input.rdbuf();
        execute(db, schema.str().c_str());
        require(queryInt(db, "PRAGMA foreign_keys;") == 1, "Foreign keys not enabled.");
        require(queryInt(db, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%';") == 10,
            "Expected ten project tables.");

        // 参数绑定写入中文；固定验证库中的演示行可重复更新，不增加重复人员。
        const char* insert = "INSERT INTO persons(person_id,name,birth_date,location) VALUES(1, ?, '2000-01-01', '长春') "
                             "ON CONFLICT(person_id) DO UPDATE SET name=excluded.name;";
        require(sqlite3_prepare_v2(db, insert, -1, &statement, nullptr) == SQLITE_OK, "Prepare insert failed.");
        const std::string name = "SQLite演示同学";
        require(sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK, "Bind failed.");
        require(sqlite3_step(statement) == SQLITE_DONE, "Insert failed.");
        sqlite3_finalize(statement); statement = nullptr;
        execute(db, "UPDATE id_state SET last_person_id=max(last_person_id, 1) WHERE singleton=1;");

        // 验证外键确实生效：账号不能引用不存在的人员。
        const int rejected = sqlite3_exec(db,
            "INSERT INTO service_accounts VALUES(900001, -1, 0, '900001', 'test', '2026-09-29');",
            nullptr, nullptr, nullptr);
        require(rejected == SQLITE_CONSTRAINT && sqlite3_extended_errcode(db) == SQLITE_CONSTRAINT_FOREIGNKEY,
            "Missing person was not rejected by foreign key.");
        require(sqlite3_close(db) == SQLITE_OK, "Close failed."); db = nullptr;

        // 关闭后重新打开文件，证明读到的是磁盘数据。
        require(sqlite3_open(filename.c_str(), &db) == SQLITE_OK, "Reopen failed.");
        execute(db, "PRAGMA foreign_keys=ON;");
        require(sqlite3_prepare_v2(db, "SELECT name FROM persons WHERE person_id=1;", -1, &statement, nullptr) == SQLITE_OK,
            "Prepare select failed.");
        require(sqlite3_step(statement) == SQLITE_ROW, "Stored row missing.");
        const auto* value = sqlite3_column_text(statement, 0);
        require(value && std::string(reinterpret_cast<const char*>(value)) == name, "UTF-8 text did not round-trip.");
        std::cout << "SQLite version: " << sqlite3_libversion() << "\nDatabase: " << filename
                  << "\nStored name: " << value << '\n';
        sqlite3_finalize(statement); statement = nullptr;
        require(sqlite3_close(db) == SQLITE_OK, "Final close failed."); db = nullptr;
        std::cout << "PASS open, ten-table schema, bound insert, foreign key, reopen/select and close.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        if (statement) sqlite3_finalize(statement);
        if (db) sqlite3_close(db);
        return 1;
    }
}
