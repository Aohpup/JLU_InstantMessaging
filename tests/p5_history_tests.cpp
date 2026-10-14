#include "storage/ChatHistoryLJY.h"
#include "storage/SqliteStorageLJY.h"
#include "services/DemoDataLJY.h"
#include "services/PlatformLJY.h"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
int main(int argc, char** argv)
{
    try
    {
        check(argc == 2, "Expected isolated database path");
        const std::string path = argv[1]; std::filesystem::remove(std::filesystem::u8path(path));
        PlatformLJY platform; loadDemoDataLJY(platform); SqliteStorageLJY storage(path); storage.save(platform);
        {
            ChatHistoryLJY history(path);
            for (int i = 1; i <= 205; ++i)
            {
                ChatMessageLJY m{"page-" + std::to_string(i), "10001", "10002", "中文 ' 引号\n" + std::to_string(i), 300-i, ChatDeliveryLJY::Received};
                history.save("10001", m);
            }
            auto recent = history.load("10001", "10002");
            check(recent.size() == 100 && recent.front().message.id == "page-106" && recent.back().message.id == "page-205", "Recent page order");
            auto previous = history.load("10001", "10002", recent.front().rowId);
            check(previous.size() == 100 && previous.front().message.id == "page-6" && previous.back().message.id == "page-105", "Earlier page");
            auto first = history.load("10001", "10002", previous.front().rowId);
            check(first.size() == 5 && first.front().message.body == "中文 ' 引号\n1", "First page and UTF8 roundtrip");
            ChatMessageLJY m{"ack", "10001", "10002", "确认状态", 500, ChatDeliveryLJY::Sending};
            history.save("10001", m); m.delivery = ChatDeliveryLJY::Received; history.save("10001", m);
            history.save("10002", m);
            check(history.load("10002", "10001").size() == 1, "Owner separation");
            check(history.load("10001", "10003").empty(), "Peer separation");
            const auto ack = history.load("10001", "10002", 0, 300);
            check(ack.size() == 206 && ack.back().message.delivery == ChatDeliveryLJY::Received, "ACK upsert must not duplicate");
            storage.save(platform);
            check(history.load("10001", "10002", 0, 300).size() == 206, "Platform save deleted chat history");
            PlatformLJY restored; storage.load(restored); check(restored.accountCount() == platform.accountCount(), "History damaged accounts");
            sqlite3* lock = nullptr; check(sqlite3_open(path.c_str(), &lock) == SQLITE_OK, "Open lock");
            check(sqlite3_exec(lock, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "Hold lock");
            m.id = "locked"; bool failed = false;
            try { history.save("10001", m); } catch (const std::exception&) { failed = true; }
            check(failed, "Write lock was ignored");
            sqlite3_exec(lock, "ROLLBACK", nullptr, nullptr, nullptr); sqlite3_close(lock);
            history.save("10001", m);
            m.id = "interrupted"; m.delivery = ChatDeliveryLJY::Sending; history.save("10001", m);
        }
        ChatHistoryLJY reopened(path);
        const auto rows = reopened.load("10001", "10002", 0, 300);
        check(rows.size() == 208 && rows.back().message.delivery == ChatDeliveryLJY::Unknown, "Restart should show unknown without resend");
        auto wb = rows.front().message; wb.service = ServiceTypeLJY::Weibo; wb.body = "微博独立历史";
        reopened.save("10001", wb);
        check(reopened.load("10001", "10002", 0, 100, ServiceTypeLJY::Weibo).size() == 1, "Service isolation");
        check(reopened.load("10001", "10002", 0, 300).size() == 208, "Weibo changed QQ rows");
        const auto oldPath = path + ".legacy"; std::filesystem::remove(std::filesystem::u8path(oldPath));
        sqlite3* old = nullptr; check(sqlite3_open(oldPath.c_str(), &old) == SQLITE_OK, "Create legacy fixture");
        check(sqlite3_exec(old, R"SQL(CREATE TABLE chat_messages(id INTEGER PRIMARY KEY,owner_qq TEXT,peer_qq TEXT,message_key TEXT,direction INTEGER,body TEXT,message_time INTEGER,local_time INTEGER,delivery_state INTEGER,UNIQUE(owner_qq,message_key));
            INSERT INTO chat_messages VALUES(7,'10001','10002','old',1,'原来QQ记录',123,456,1);)SQL",nullptr,nullptr,nullptr)==SQLITE_OK,"Legacy fixture");
        sqlite3_close(old);
        { ChatHistoryLJY migrated(oldPath); const auto oldRows=migrated.load("10001","10002");
          check(oldRows.size()==1 && oldRows[0].rowId==7 && oldRows[0].message.body=="原来QQ记录", "Migration lost legacy QQ data");
          check(migrated.load("10001","10002",0,100,ServiceTypeLJY::Weibo).empty(),"Legacy QQ assigned to Weibo"); }
        { ChatHistoryLJY again(oldPath); check(again.load("10001","10002").size()==1,"Migration not idempotent"); }
        std::cout << "PASS: pagination, UTF8, ACK upsert, owner/peer isolation, platform save/load compatibility, locked write retry, restart unknown\n";
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
