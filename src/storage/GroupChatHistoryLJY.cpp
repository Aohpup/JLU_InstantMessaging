#include "storage/GroupChatHistoryLJY.h"
#include <sqlite3.h>
#include <algorithm>
#include <stdexcept>
namespace
{
// 一次语句使用一次局部对象，异常时也会释放 SQLite 语句。
struct GroupStatementLJY
{
    sqlite3_stmt *value = nullptr;
    ~GroupStatementLJY()
    {
        if (value)
            sqlite3_finalize(value);
    }
};
void bindText(sqlite3_stmt *statement, int column, const QString &value)
{
    const auto bytes = value.toUtf8();
    if (sqlite3_bind_text(statement, column, bytes.constData(), bytes.size(), SQLITE_TRANSIENT) != SQLITE_OK)
        throw std::runtime_error("群历史绑定文字失败。");
}
} // namespace
void GroupChatHistoryLJY::check(int result)
{
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW)
        throw std::runtime_error(db_ ? sqlite3_errmsg(db_) : "群历史打开失败。");
}
GroupChatHistoryLJY::GroupChatHistoryLJY(const QString &path)
{
    try
    {
        check(sqlite3_open_v2(path.toUtf8().constData(), &db_, SQLITE_OPEN_READWRITE, nullptr));
        sqlite3_busy_timeout(db_, 100);
        check(sqlite3_exec(db_, R"SQL(CREATE TABLE IF NOT EXISTS group_chat_messages(
            id INTEGER PRIMARY KEY, service INTEGER NOT NULL, owner_account TEXT NOT NULL, group_number INTEGER NOT NULL,
            message_key TEXT NOT NULL, sender_account TEXT NOT NULL, nickname TEXT NOT NULL, avatar_color INTEGER NOT NULL,
            body TEXT NOT NULL, message_time INTEGER NOT NULL, UNIQUE(service,owner_account,group_number,message_key));
            CREATE INDEX IF NOT EXISTS group_chat_lookup ON group_chat_messages(service,owner_account,group_number,id);
        )SQL",
                           nullptr, nullptr, nullptr));
    }
    catch (...)
    {
        if (db_)
            sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
GroupChatHistoryLJY::~GroupChatHistoryLJY()
{
    if (db_)
        sqlite3_close(db_);
}
void GroupChatHistoryLJY::save(int service, const QString &owner, int group,
                               const GroupChatMessageLJY &message)
{
    GroupStatementLJY query;
    check(sqlite3_prepare_v2(db_,
                             "INSERT OR IGNORE INTO "
                             "group_chat_messages(service,owner_account,group_number,message_key,sender_"
                             "account,nickname,avatar_color,body,message_time) VALUES(?,?,?,?,?,?,?,?,?)",
                             -1, &query.value, nullptr));
    sqlite3_bind_int(query.value, 1, service);
    bindText(query.value, 2, owner);
    sqlite3_bind_int(query.value, 3, group);
    bindText(query.value, 4, message.id);
    bindText(query.value, 5, message.sender);
    bindText(query.value, 6, message.nickname);
    sqlite3_bind_int(query.value, 7, message.color);
    bindText(query.value, 8, message.body);
    sqlite3_bind_int64(query.value, 9, message.time);
    check(sqlite3_step(query.value));
}
std::vector<GroupChatMessageLJY> GroupChatHistoryLJY::load(int service, const QString &owner, int group,
                                                           long long before, int limit)
{
    GroupStatementLJY query;
    check(sqlite3_prepare_v2(db_,
                             "SELECT id,message_key,sender_account,nickname,avatar_color,body,message_time "
                             "FROM group_chat_messages WHERE service=? AND owner_account=? AND "
                             "group_number=? AND (?4=0 OR id<?4) ORDER BY id DESC LIMIT ?5",
                             -1, &query.value, nullptr));
    sqlite3_bind_int(query.value, 1, service);
    bindText(query.value, 2, owner);
    sqlite3_bind_int(query.value, 3, group);
    sqlite3_bind_int64(query.value, 4, before);
    sqlite3_bind_int(query.value, 5, limit);
    std::vector<GroupChatMessageLJY> result;
    int status;
    auto text = [&](int i) {
        return QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(query.value, i)));
    };
    while ((status = sqlite3_step(query.value)) == SQLITE_ROW)
    {
        GroupChatMessageLJY m;
        m.rowId = sqlite3_column_int64(query.value, 0);
        m.id = text(1);
        m.sender = text(2);
        m.nickname = text(3);
        m.color = sqlite3_column_int(query.value, 4);
        m.body = text(5);
        m.time = sqlite3_column_int64(query.value, 6);
        result.push_back(m);
    }
    check(status);
    std::reverse(result.begin(), result.end());
    return result;
}
