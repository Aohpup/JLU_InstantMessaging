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

// 用独立只读连接核对已提交的十张业务表，比较保存和读取前后的完整数据。
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

// 仅测试修改临时库，用于验证读取失败后的状态。
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
                             "chat_groups","group_members","group_requests","friend_requests","id_state"})
        // revision 是每次提交递增的同步元数据，不属于平台往返的业务内容。
        for (const auto& row : query(path, std::string(table)=="id_state"
            ? "SELECT singleton,last_person_id,last_account_id,last_shared_number,last_wechat_number,last_group_id,last_group_request_id FROM id_state;"
            : std::string("SELECT * FROM ")+table+" ORDER BY 1,2;"))
            result += std::string(table)+":"+row+"\n";
    return result;
}
void denied(const std::function<void()>& action)
{
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected failure.");
}

// 保存后销毁连接和原平台，再从新连接恢复，验证对象能继续参与原业务。
void testRoundTrip()
{
    const auto path = fresh("roundtrip.db"), copy = fresh("roundtrip_copy.db");
    int child = 0, childRequest = 0, invitation = 0;
    std::string unboundQQ;
    {
        PlatformLJY source; loadDemoDataLJY(source); SessionManagerLJY session(source);
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        session.updateFriendRemark("10002", "同桌 O'Brien"); session.logout();
        session.login(ServiceTypeLJY::QQ, "10002", "DemoOnly123!");
        session.updateFriendRemark("10001", "好友甲"); session.logout();
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        child = session.createSubgroup(1001, 2001, "讨论 ' A", "10002", {"10001"}); session.logout();
        session.login(ServiceTypeLJY::QQ, "10004", "DemoOnly123!");
        childRequest = session.applyToGroup(2001); session.logout();
        session.login(ServiceTypeLJY::QQ, "10003", "DemoOnly123!");
        session.leaveGroup(1001);
        const int accepted = session.applyToGroup(1001); session.logout();
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        session.handleGroupRequest(accepted, true); session.logout();
        session.login(ServiceTypeLJY::WeChat, "wx_20004", "DemoOnly123!");
        session.leaveGroup(1001); session.logout();
        session.login(ServiceTypeLJY::WeChat, "wx_20001", "DemoOnly123!");
        invitation = session.inviteToGroup(1001, "wx_20004"); session.logout();
        // 此人两个服务都曾密码登录但尚未绑定，用于确认历史不会跨重启保留。
        const auto user = session.registerAccount("新同学", "2002-03-04", "长春", "TestPass123!", ServiceTypeLJY::QQ, "同学QQ");
        unboundQQ = user.externalId();
        session.login(ServiceTypeLJY::QQ, unboundQQ, "TestPass123!");
        const int wx = session.openService(ServiceTypeLJY::WeChat, "同学微信"); session.logout();
        session.login(ServiceTypeLJY::WeChat, source.findAccount(wx)->externalId(), "TestPass123!");
        SqliteStorageLJY storage(path.u8string()); storage.save(source);
    }
    const auto original = snapshot(path);
    PlatformLJY restored;
    { SqliteStorageLJY storage(path.u8string()); storage.load(restored); }
    { SqliteStorageLJY storage(copy.u8string()); storage.save(restored); }
    require(snapshot(copy) == original && snapshot(path) == original, "Nine-table round trip changed saved data.");
    const auto* subgroup = restored.findGroup(child);
    const int b = restored.findAccount(ServiceTypeLJY::QQ, "10002")->accountId();
    const int a = restored.findAccount(ServiceTypeLJY::QQ, "10001")->accountId();
    require(subgroup && subgroup->members().size() == 2 && subgroup->members()[0].accountId() == b &&
        subgroup->members()[1].accountId() == a, "Owner duplicated or member order lost.");
    SessionManagerLJY session(restored);
    require(!session.isLoggedIn(), "Session restored as logged in.");
    session.login(ServiceTypeLJY::QQ, unboundQQ, "TestPass123!");
    denied([&] { session.bindWeChatToQQ(); });
    session.enterService(ServiceTypeLJY::WeChat, true); session.bindWeChatToQQ(); session.logout();
    session.login(ServiceTypeLJY::QQ, "10002", "DemoOnly123!");
    require(session.friendRemark("10001") == "好友甲", "Remark not usable after loading.");
    session.handleGroupRequest(childRequest, true); session.updateNickname("恢复后昵称"); session.logout();
    session.login(ServiceTypeLJY::WeChat, "wx_20004", "DemoOnly123!");
    session.handleGroupRequest(invitation, true); session.logout();
    const auto added = session.registerAccount("后续同学", "2003-04-05", "吉林", "TestPass123!", ServiceTypeLJY::QQ, "新增QQ");
    require(added.personId() == std::stoi(scalar(path, "SELECT last_person_id FROM id_state;")) + 1 &&
        added.accountId() == std::stoi(scalar(path, "SELECT last_account_id FROM id_state;")) + 1 &&
        added.externalId() == std::to_string(std::stoi(scalar(path, "SELECT last_shared_number FROM id_state;")) + 1),
        "Person, account or shared counter not restored.");
    session.login(ServiceTypeLJY::QQ, added.externalId(), "TestPass123!");
    const int wx = session.openService(ServiceTypeLJY::WeChat, "新增微信");
    const int wb = session.openService(ServiceTypeLJY::Weibo, "新增微博");
    require(restored.findAccount(wx)->externalId() == "wx_" + std::to_string(std::stoi(scalar(path, "SELECT last_wechat_number FROM id_state;")) + 1) &&
        restored.findAccount(wb)->externalId() == added.externalId(), "WeChat or shared Weibo number not continued.");
    { SqliteStorageLJY storage(copy.u8string()); storage.save(restored); }
    require(scalar(copy, "SELECT nickname FROM service_accounts WHERE service=0 AND external_id='10002';") == "恢复后昵称",
        "Restored platform could not be modified and saved.");
}

void testRetainedSubgroup()
{
    const auto path = fresh("retained.db");
    int retainedGroup = 0, retainedRequest = 0;
    {
        PlatformLJY source; loadDemoDataLJY(source); SessionManagerLJY session(source);
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        retainedGroup = session.createSubgroup(1001, 2001, "临时子群", "10002", {"10001"}); session.logout();
        session.login(ServiceTypeLJY::QQ, "10004", "DemoOnly123!");
        retainedRequest = session.applyToGroup(2001); session.logout();
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        session.switchGroupMode(1001, GroupStyleLJY::WeChat);
        SqliteStorageLJY storage(path.u8string()); storage.save(source);
    }
    PlatformLJY restored;
    { SqliteStorageLJY storage(path.u8string()); storage.load(restored); }
    const auto* parent = restored.findGroup(ServiceTypeLJY::QQ, 1001);
    const int b = restored.findAccount(ServiceTypeLJY::QQ, "10002")->accountId();
    require(restored.findGroup(retainedGroup) && parent->style() == GroupStyleLJY::WeChat &&
        parent->members().size() == 5 && parent->findMember(b)->isAdministrator(), "Retained child or parent state restored incorrectly.");
    SessionManagerLJY session(restored); session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
    require(session.subgroups(1001).size()==1 && query(path, "SELECT request_id FROM group_requests WHERE group_number=2001;").size()==1, "Retained subgroup or requests missing.");
    session.switchGroupMode(1001, GroupStyleLJY::QQ);
    require(session.subgroups(1001).size()==1, "Switching back lost child.");
    const int newGroup = session.createSubgroup(1001, 2002, "新增子群", "10002", {"10001"}); session.logout();
    session.login(ServiceTypeLJY::QQ, "10004", "DemoOnly123!");
    const int newRequest = session.applyToGroup(2002);
    require(newGroup == retainedGroup + 1 && newRequest == retainedRequest + 1, "Group or request IDs not continued.");
    { SqliteStorageLJY storage(path.u8string()); storage.save(restored); }
    require(query(path, "PRAGMA foreign_key_check;").empty(), "Continued operations saved invalid references.");
}

void testLoadFailure()
{
    const auto path = fresh("load_failure.db"), memoryCopy = fresh("memory_copy.db");
    PlatformLJY original; loadDemoDataLJY(original);
    SqliteStorageLJY storage(path.u8string()); storage.save(original);
    const auto valid = snapshot(path);
    PlatformLJY target;
    AccountServiceLJY accounts(target);
    accounts.registerAccount("保留内存", "2000-01-01", "吉林", "KeepPass123!", ServiceTypeLJY::QQ, "原昵称");
    SqliteStorageLJY inspector(memoryCopy.u8string()); inspector.save(target);
    const auto targetBefore = snapshot(memoryCopy);
    // 外键允许的错误归属，以及落后的发号状态，各验证一次失败保留和修复重试。
    for (const char* sql : {
        "UPDATE account_bindings SET qq_account_id=(SELECT account_id FROM service_accounts WHERE service=0 AND external_id='10003') WHERE wechat_account_id=(SELECT account_id FROM service_accounts WHERE service=1 AND external_id='wx_20001');",
        "UPDATE id_state SET last_account_id=0;"})
    {
        execute(path, sql); const auto damaged = snapshot(path);
        denied([&] { storage.load(target); });
        inspector.save(target);
        require(snapshot(memoryCopy) == targetBefore && snapshot(path) == damaged, "Failed loading changed target or disk data.");
        storage.save(original);
    }
    storage.load(target); inspector.save(target);
    require(snapshot(memoryCopy) == valid, "Loading could not retry after repair.");
    const auto emptyPath = fresh("empty.db");
    SqliteStorageLJY empty(emptyPath.u8string()); empty.load(target);
    require(target.personCount() == 0 && target.accountCount() == 0 && target.groupCount() == 0, "Empty database load failed.");
}
int main()
{
    int failed = 0;
    auto test = [&](const char* name, void(*action)()) {
        try { action(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    test("ten-table round trip and continued business", testRoundTrip);
    test("retained subgroup and continued counters", testRetainedSubgroup);
    test("failed load preserves target, repair retry and empty database", testLoadFailure);
    std::cout << (3 - failed) << "/3 storage load scenario groups passed.\n";
    return failed ? 1 : 0;
}
