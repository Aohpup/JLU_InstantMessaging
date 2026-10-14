#include "storage/SqliteStorageLJY.h"
#include "services/DemoDataLJY.h"
#include "services/SessionManagerLJY.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

// 只清理本测试固定目录内的单个验证文件，不接触正式数据库。
std::filesystem::path fresh(const char* name)
{
    const auto path = std::filesystem::path(JLU_STORAGE_TEST_DIR) / name;
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    return path;
}

// 用独立只读连接核对已提交的磁盘记录，不调用尚未实现的加载接口。
std::vector<std::string> query(const std::filesystem::path& path, const std::string& sql)
{
    sqlite3* db = nullptr; sqlite3_stmt* statement = nullptr;
    try
    {
        require(sqlite3_open_v2(path.u8string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "Inspection open failed.");
        require(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK, "Inspection prepare failed.");
        std::vector<std::string> rows;
        int result;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW)
        {
            std::string row;
            for (int i=0; i<sqlite3_column_count(statement); ++i)
            {
                if (i) row += '|';
                const auto* text = sqlite3_column_text(statement, i);
                row += text ? std::string(reinterpret_cast<const char*>(text), sqlite3_column_bytes(statement, i)) : "NULL";
            }
            rows.push_back(row);
        }
        require(result == SQLITE_DONE, "Inspection query failed.");
        sqlite3_finalize(statement); sqlite3_close(db);
        return rows;
    }
    catch (...) { if (statement) sqlite3_finalize(statement); if (db) sqlite3_close(db); throw; }
}
std::string scalar(const std::filesystem::path& path, const std::string& sql)
{
    const auto rows = query(path, sql);
    require(rows.size()==1, "Expected one query row.");
    return rows.front();
}

// 仅测试用来制造保存中途失败，生产代码不包含测试触发器或故障开关。
void execute(const std::filesystem::path& path, const char* sql)
{
    sqlite3* db = nullptr;
    const int opened = sqlite3_open_v2(path.u8string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr);
    const int result = opened == SQLITE_OK ? sqlite3_exec(db, sql, nullptr, nullptr, nullptr) : opened;
    if (db) sqlite3_close(db);
    require(result == SQLITE_OK, "Test setup SQL failed.");
}
std::string snapshot(const std::filesystem::path& path)
{
    std::string result;
    for (const char* table : {"persons","service_accounts","credentials","account_bindings","friendships",
                             "chat_groups","group_members","group_requests","id_state"})
        for (const auto& row : query(path, std::string("SELECT * FROM ")+table+" ORDER BY 1,2;"))
            result += std::string(table)+":"+row+"\n";
    return result;
}
void denied(const std::function<void()>& action)
{
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected failure.");
}

void testSave()
{
    const auto path = fresh("save.db");
    PlatformLJY platform; loadDemoDataLJY(platform); SessionManagerLJY session(platform);
    GroupManagerLJY groups(platform);
    const auto& a=*platform.findAccount(ServiceTypeLJY::QQ,"10001");
    const auto& b=*platform.findAccount(ServiceTypeLJY::QQ,"10002");
    const auto& c=*platform.findAccount(ServiceTypeLJY::QQ,"10003");
    const auto& d=*platform.findAccount(ServiceTypeLJY::QQ,"10004");
    const auto& wx=*platform.findAccount(ServiceTypeLJY::WeChat,"wx_20001");
    const auto& wf=*platform.findAccount(ServiceTypeLJY::WeChat,"wx_20004");
    session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
    session.updateFriendRemark("10002","同桌 O'Brien"); session.logout();
    session.login(ServiceTypeLJY::QQ,"10002","DemoOnly123!");
    session.updateFriendRemark("10001","好友甲"); session.logout();
    const int child=groups.createSubgroup(a.personId(),a.accountId(),1001,2001,"讨论 ' A",b.accountId(),{a.accountId()});
    const int childRequest=groups.apply(d.personId(),d.accountId(),2001);
    groups.leave(c.personId(),c.accountId(),1001);
    groups.apply(c.personId(),c.accountId(),1001);
    groups.leave(wf.personId(),wf.accountId(),1001);
    groups.invite(wx.personId(),wx.accountId(),1001,wf.accountId());
    {
        SqliteStorageLJY storage(path.u8string()); storage.save(platform);
    }
    require(scalar(path,"SELECT count(*) FROM persons;")=="6", "Person count changed.");
    require(scalar(path,"SELECT count(*) FROM service_accounts;")=="12", "Account count changed.");
    require(scalar(path,"SELECT count(*) FROM credentials WHERE password='DemoOnly123!';")=="6", "Credentials not saved.");
    require(scalar(path,"SELECT count(*) FROM account_bindings;")=="3", "Bindings not saved.");
    require(scalar(path,"SELECT count(*) FROM friendships;")=="6", "Friend pairs changed.");
    require(scalar(path,"SELECT first_remark||'|'||second_remark FROM friendships WHERE first_account_id="+
        std::to_string(a.accountId())+" AND second_account_id="+std::to_string(b.accountId())+";")=="同桌 O'Brien|好友甲",
        "Independent quoted remarks changed.");
    require(scalar(path,"SELECT count(*) FROM chat_groups;")=="19", "Groups missing.");
    require(scalar(path,"SELECT count(*) FROM chat_groups WHERE parent_group_id IS NULL;")=="18", "Top-level NULL mapping failed.");
    require(query(path,"SELECT account_id FROM group_members WHERE group_id="+std::to_string(child)+" ORDER BY member_order;") ==
        std::vector<std::string>{std::to_string(b.accountId()),std::to_string(a.accountId())}, "Member order changed.");
    require(scalar(path,"SELECT count(*) FROM group_requests;")=="3", "Requests missing.");
    require(scalar(path,"SELECT count(*) FROM group_requests WHERE type=1;")=="1", "Invitation missing.");
    require(query(path,"PRAGMA foreign_key_check;").empty(), "Saved foreign keys invalid.");
    const auto original=snapshot(path);
    {
        SqliteStorageLJY storage(path.u8string());
        require(snapshot(path)==original,"Opening existing file changed saved data.");
        groups.switchMode(a.personId(),a.accountId(),1001,GroupStyleLJY::WeChat);
        storage.save(platform);
    }
    require(scalar(path,"SELECT count(*) FROM chat_groups;")=="19", "Retained subgroup missing on disk.");
    require(scalar(path,"SELECT count(*) FROM group_members WHERE group_id="+std::to_string(child)+";")=="2", "Retained child members missing.");
    require(scalar(path,"SELECT count(*) FROM group_requests WHERE request_id="+std::to_string(childRequest)+";")=="1", "Retained child request missing.");
    require(scalar(path,"SELECT last_group_id FROM id_state;")==std::to_string(child), "Counter moved backwards.");
    require(scalar(path,"SELECT is_administrator FROM group_members WHERE group_id="+
        std::to_string(platform.findGroup(ServiceTypeLJY::QQ,1001)->groupId())+" AND account_id="+std::to_string(b.accountId())+";")=="1",
        "Paused administrator flag lost.");
    require(platform.personCount()==6 && platform.groupCount()==19,"Saving changed platform memory.");
}

void testRollback()
{
    const auto path=fresh("rollback.db");
    PlatformLJY platform; loadDemoDataLJY(platform); SessionManagerLJY session(platform);
    GroupManagerLJY groups(platform);
    const auto& c=*platform.findAccount(ServiceTypeLJY::QQ,"10003");
    groups.leave(c.personId(),c.accountId(),1001); groups.apply(c.personId(),c.accountId(),1001);
    SqliteStorageLJY storage(path.u8string()); storage.save(platform);
    const auto before=snapshot(path);
    session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
    session.updateNickname("修改后 ' 昵称");
    execute(path,"CREATE TRIGGER fail_save BEFORE INSERT ON group_requests BEGIN SELECT RAISE(ABORT,'forced late save failure'); END;");
    std::string error;
    try { storage.save(platform); } catch (const std::exception& e) { error=e.what(); }
    require(error.find("forced late save failure")!=std::string::npos,"Late save did not report error.");
    require(snapshot(path)==before,"Failed transaction left partial data.");
    require(session.currentAccount().nickname()=="修改后 ' 昵称", "Failure lost unsaved memory data.");
    execute(path,"DROP TRIGGER fail_save;");
    storage.save(platform);
    require(scalar(path,"SELECT nickname FROM service_accounts WHERE service=0 AND external_id='10001';")=="修改后 ' 昵称",
        "Retry did not commit.");
    PlatformLJY empty; storage.save(empty);
    require(scalar(path,"SELECT count(*) FROM persons;")=="0" && scalar(path,"SELECT count(*) FROM chat_groups;")=="0",
        "Explicit empty platform not saved.");
    require(scalar(path,"SELECT last_person_id||'|'||last_shared_number||'|'||last_group_id FROM id_state;")=="0|10000|0",
        "Empty platform counters incorrect.");
}

void testOpenFailure()
{
    const auto invalid=fresh("invalid.db");
    const std::string original="This is not a SQLite database.";
    { std::ofstream out(invalid,std::ios::binary); out<<original; }
    denied([&]{SqliteStorageLJY storage(invalid.u8string());});
    std::ifstream input(invalid,std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    require(after==original,"Invalid existing file was overwritten.");
    denied([&]{SqliteStorageLJY storage("");});
}
int main()
{
    int failed=0;
    auto test=[&](const char* name,void(*action)()) {
        try { action(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& error) { ++failed; std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n'; }
    };
    test("complete save, reopen and retained subgroup",testSave);
    test("late failure rollback and retry",testRollback);
    test("invalid file remains unchanged",testOpenFailure);
    std::cout<<(3-failed)<<"/3 storage save scenario groups passed.\n";
    return failed?1:0;
}
