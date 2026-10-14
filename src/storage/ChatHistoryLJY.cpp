#include "storage/ChatHistoryLJY.h"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

ChatHistoryLJY::ChatHistoryLJY(const std::string& path)
{
    try
    {
        // 主程序已创建平台库；这里不隐式新建错路径下的另一个数据库。
        check(sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr));
        check(sqlite3_busy_timeout(db_, 100));
        bool legacy = false;
        prepare("PRAGMA table_info(chat_messages)");
        while (sqlite3_step(statement_) == SQLITE_ROW)
            if (std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement_, 1))) == "owner_qq") legacy = true;
        finish();
        check(sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr));
        if (legacy) check(sqlite3_exec(db_, "ALTER TABLE chat_messages RENAME TO chat_messages_qq_old", nullptr, nullptr, nullptr));
        check(sqlite3_exec(db_, R"SQL(
            CREATE TABLE IF NOT EXISTS chat_messages (
                id INTEGER PRIMARY KEY,
                service INTEGER NOT NULL CHECK(service BETWEEN 0 AND 2),
                owner_account TEXT NOT NULL,
                peer_account TEXT NOT NULL,
                message_key TEXT NOT NULL,
                direction INTEGER NOT NULL CHECK(direction IN (0,1)),
                body TEXT NOT NULL,
                message_time INTEGER NOT NULL,
                local_time INTEGER NOT NULL,
                delivery_state INTEGER NOT NULL CHECK(delivery_state BETWEEN 0 AND 3),
                UNIQUE(service, owner_account, message_key)
            );

        )SQL", nullptr, nullptr, nullptr));
        if (legacy) check(sqlite3_exec(db_, R"SQL(
            INSERT INTO chat_messages SELECT id,0,owner_qq,peer_qq,message_key,direction,body,message_time,local_time,delivery_state FROM chat_messages_qq_old;
            DROP TABLE chat_messages_qq_old;
        )SQL", nullptr, nullptr, nullptr));
        check(sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS chat_service_owner_peer_id ON chat_messages(service,owner_account,peer_account,id); COMMIT", nullptr, nullptr, nullptr));
    }
    catch (...) { finish(); if (db_) { sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); sqlite3_close(db_); } db_ = nullptr; throw; }
}
ChatHistoryLJY::~ChatHistoryLJY() { finish(); if (db_) sqlite3_close(db_); }
void ChatHistoryLJY::check(int result) const
{
    if (result != SQLITE_OK) throw std::runtime_error(std::string("聊天记录数据库：") + (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(result)));
}
void ChatHistoryLJY::finish() { if (statement_) sqlite3_finalize(statement_); statement_ = nullptr; }
void ChatHistoryLJY::prepare(const char* sql)
{
    finish(); check(sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr));
}
void ChatHistoryLJY::bind(int index, const std::string& value)
{
    check(sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
}
void ChatHistoryLJY::save(const std::string& owner, const ChatMessageLJY& message)
{
    if (owner.empty() || message.id.empty() || (owner != message.senderAccount && owner != message.receiverAccount))
        throw std::invalid_argument("聊天记录缺少有效的所属账号或消息编号。");
    const bool outgoing = owner == message.senderAccount;
    try
    {
        prepare(R"SQL(INSERT INTO chat_messages(owner_account,peer_account,message_key,direction,body,message_time,local_time,delivery_state,service)
            VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(service,owner_account,message_key) DO UPDATE SET delivery_state=excluded.delivery_state)SQL");
        bind(1, owner); bind(2, outgoing ? message.receiverAccount : message.senderAccount); bind(3, message.id);
        check(sqlite3_bind_int(statement_, 4, outgoing ? 1 : 0)); bind(5, message.body);
        check(sqlite3_bind_int64(statement_, 6, message.time));
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        check(sqlite3_bind_int64(statement_, 7, now));
        check(sqlite3_bind_int(statement_, 8, static_cast<int>(message.delivery)));
        check(sqlite3_bind_int(statement_, 9, static_cast<int>(message.service)));
        const int result = sqlite3_step(statement_); if (result != SQLITE_DONE) check(result);
        finish();
    }
    catch (...) { finish(); throw; }
}
std::vector<StoredChatMessageLJY> ChatHistoryLJY::load(const std::string& owner, const std::string& peer, long long beforeId, int limit, ServiceTypeLJY service)
{
    std::vector<StoredChatMessageLJY> rows;
    try
    {
        prepare(R"SQL(SELECT id,message_key,direction,body,message_time,delivery_state FROM chat_messages
            WHERE owner_account=? AND peer_account=? AND (?=0 OR id<?) AND service=?6 ORDER BY id DESC LIMIT ?5)SQL");
        bind(1, owner); bind(2, peer);
        check(sqlite3_bind_int64(statement_, 3, beforeId)); check(sqlite3_bind_int64(statement_, 4, beforeId));
        check(sqlite3_bind_int(statement_, 5, limit));
        check(sqlite3_bind_int(statement_, 6, static_cast<int>(service)));
        auto text = [this](int column) {
            const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(statement_, column));
            return value ? std::string(value, sqlite3_column_bytes(statement_, column)) : std::string();
        };
        int result;
        while ((result = sqlite3_step(statement_)) == SQLITE_ROW)
        {
            StoredChatMessageLJY row; row.rowId = sqlite3_column_int64(statement_, 0);
            auto& m = row.message; m.service = service; m.id = text(1); const bool outgoing = sqlite3_column_int(statement_, 2) != 0;
            m.senderAccount = outgoing ? owner : peer; m.receiverAccount = outgoing ? peer : owner;
            m.body = text(3); m.time = sqlite3_column_int64(statement_, 4);
            m.delivery = static_cast<ChatDeliveryLJY>(sqlite3_column_int(statement_, 5));
            if (m.delivery == ChatDeliveryLJY::Sending) m.delivery = ChatDeliveryLJY::Unknown;
            rows.push_back(std::move(row));
        }
        if (result != SQLITE_DONE) check(result);
        finish(); std::reverse(rows.begin(), rows.end()); return rows;
    }
    catch (...) { finish(); throw; }
}
