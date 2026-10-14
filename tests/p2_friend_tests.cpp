#include "services/FriendManagerLJY.h"
#include "ui/ConsoleAppLJY.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

// 断言失败时保留原因，测试进程返回失败。
void require(bool value, const char* reason)
{
    if (!value) throw std::runtime_error(reason);
}

// 输入和身份不合法时必须拒绝操作。
void rejected(const std::function<void()>& action)
{
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid operation accepted.");
}

// 为底层测试创建两个同服务账号，返回账号内部编号。
std::pair<int, int> createPair(PlatformLJY& platform, ServiceTypeLJY type = ServiceTypeLJY::QQ)
{
    AccountServiceLJY accounts(platform);
    const int a = accounts.createPerson("Alice", "2000-01-01", "City A");
    const int b = accounts.createPerson("Bob", "2001-01-01", "City B");
    return {accounts.openService(a, type, "A"), accounts.openService(b, type, "B")};
}

// 验证关系顺序归一化和备注读取的账号边界。
void testModel()
{
    FriendRelationLJY relation(9, 2);
    require(relation.firstAccountId() == 2 && relation.secondAccountId() == 9, "Noncanonical pair.");
    require(relation.otherAccountId(2) == 9 && relation.otherAccountId(9) == 2, "Wrong peer.");
    require(relation.remarkFor(2).empty() && relation.remarkFor(9).empty(), "Unexpected initial remarks.");
    rejected([&] { relation.otherAccountId(3); });
    rejected([&] { relation.remarkFor(3); });
    rejected([] { FriendRelationLJY invalid(1, 1); });
    rejected([] { FriendRelationLJY invalid(0, 1); });
}

// 验证一条关系可从双方读取，重复、反向重复和越权添加均被拒绝。
void testAddAndGuards()
{
    PlatformLJY platform;
    const auto ids = createPair(platform);
    FriendManagerLJY friends(platform);
    const int a = platform.findAccount(ids.first)->personId();
    const int b = platform.findAccount(ids.second)->personId();
    rejected([&] { friends.addFriend(a, ids.first, ids.first); });
    rejected([&] { friends.addFriend(a, ids.first, 999); });
    rejected([&] { friends.addFriend(b, ids.first, ids.second); });
    require(platform.friendRelationCount() == 0, "Invalid add left a relation.");
    friends.addFriend(a, ids.first, ids.second);
    require(platform.friendRelationCount() == 1 && friends.friends(a, ids.first).size() == 1 &&
            friends.friends(b, ids.second).size() == 1, "Relation not bidirectional.");
    require(platform.findFriendRelation(ids.first, ids.second) == platform.findFriendRelation(ids.second, ids.first),
            "Reverse lookup returned a different relation.");
    rejected([&] { friends.addFriend(a, ids.first, ids.second); });
    rejected([&] { friends.addFriend(b, ids.second, ids.first); });
    rejected([&] { friends.friends(b, ids.first); });
    require(platform.friendRelationCount() == 1, "Duplicates created extra relation.");
}

// 验证双方备注独立、资料只读以及删除后双方备注一并清除。
void testRemarksAndRemoval()
{
    PlatformLJY platform;
    const auto ids = createPair(platform);
    FriendManagerLJY friends(platform);
    const int a = platform.findAccount(ids.first)->personId();
    const int b = platform.findAccount(ids.second)->personId();
    friends.addFriend(a, ids.first, ids.second);
    friends.updateRemark(a, ids.first, ids.second, "同学乙");
    friends.updateRemark(b, ids.second, ids.first, "同学甲");
    rejected([&] { friends.updateRemark(a, ids.first, ids.second, std::string(121, 'x')); });
    rejected([&] { friends.updateRemark(a, ids.first, ids.second, " \t "); });
    rejected([&] { friends.updateRemark(a, ids.first, ids.second, "   "); });
    require(friends.friendRelation(a, ids.first, ids.second).remarkFor(ids.first) == "同学乙" &&
            friends.friendRelation(b, ids.second, ids.first).remarkFor(ids.second) == "同学甲", "Remarks not independent or failure changed state.");
    require(platform.findAccount(ids.second)->nickname() == "B" &&
            platform.findPerson(b)->name() == "Bob", "Remark modified profile.");
    friends.updateRemark(a, ids.first, ids.second, "");
    require(friends.friendRelation(a, ids.first, ids.second).remarkFor(ids.first).empty() &&
            friends.friendRelation(b, ids.second, ids.first).remarkFor(ids.second) == "同学甲", "Clearing one remark changed the other.");
    friends.updateRemark(a, ids.first, ids.second, std::string(120, 'x'));
    rejected([&] { friends.removeFriend(b, ids.first, ids.second); });
    friends.removeFriend(b, ids.second, ids.first);
    require(platform.friendRelationCount() == 0 && friends.friends(a, ids.first).empty() &&
            friends.friends(b, ids.second).empty(), "Deletion was not bilateral.");
    rejected([&] { friends.friendRelation(a, ids.first, ids.second); });
    rejected([&] { friends.updateRemark(a, ids.first, ids.second, "stale"); });
    rejected([&] { friends.removeFriend(a, ids.first, ids.second); });
    friends.addFriend(a, ids.first, ids.second);
    require(friends.friendRelation(a, ids.first, ids.second).remarkFor(ids.first).empty() &&
            friends.friendRelation(b, ids.second, ids.first).remarkFor(ids.second).empty(), "Deleted remarks returned after re-add.");
}

// 即使 QQ 和微博共享显示号码，好友关系仍按服务账号分别保存。
void testServiceIsolation()
{
    PlatformLJY platform;
    const auto qq = createPair(platform);
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = platform.findAccount(qq.first)->personId();
    const int b = platform.findAccount(qq.second)->personId();
    for (auto type : {ServiceTypeLJY::Weibo, ServiceTypeLJY::WeChat})
    {
        const int first = accounts.openService(a, type, "First");
        const int second = accounts.openService(b, type, "Second");
        rejected([&] { friends.addFriend(a, qq.first, second); });
        friends.addFriend(a, first, second);
    }
    require(friends.friends(a, qq.first).empty(), "Other service leaked into QQ.");
    friends.addFriend(a, qq.first, qq.second);
    friends.removeFriend(a, qq.first, qq.second);
    require(platform.friendRelationCount() == 2, "Deleting QQ removed other service relations.");
}

// 通过会话核对匿名保护、当前服务隔离及双方查询。
void testSession()
{
    PlatformLJY platform;
    SessionManagerLJY a(platform), b(platform);
    const auto first = a.registerAccount("Alice", "2000-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "A");
    const auto second = b.registerAccount("Bob", "2001-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "B");
    rejected([&] { a.addFriend(second.externalId()); });
    rejected([&] { a.friends(); });
    rejected([&] { a.friendPerson(second.externalId()); });
    a.login(ServiceTypeLJY::QQ, first.externalId(), "TestOnly123!");
    b.login(ServiceTypeLJY::QQ, second.externalId(), "TestOnly123!");
    a.addFriend(second.externalId());
    a.updateFriendRemark(second.externalId(), "甲的备注");
    b.updateFriendRemark(first.externalId(), "乙的备注");
    require(a.friendRemark(second.externalId()) == "甲的备注" && b.friendRemark(first.externalId()) == "乙的备注",
            "Session selected wrong remark owner.");
    a.openService(ServiceTypeLJY::Weibo, "Weibo A");
    b.openService(ServiceTypeLJY::Weibo, "Weibo B");
    a.enterService(ServiceTypeLJY::Weibo, true);
    require(a.friends().empty(), "Friend list ignored current service.");
    rejected([&] { a.friendPerson(second.externalId()); });
    a.enterService(ServiceTypeLJY::QQ, true);
    a.logout();
    rejected([&] { a.removeFriend(second.externalId()); });
    rejected([&] { a.updateFriendRemark(second.externalId(), "bad"); });
    rejected([&] { a.contactAccount(second.externalId()); });
    require(b.friends().size() == 1, "Other session logout removed friends.");
}

// 驱动真实菜单，检查取消、错误、重复添加、备注、删除及输入结束。
void testMenu()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    session.registerAccount("Alice", "2000-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "A");
    session.registerAccount("Bob", "2001-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "B");
    const auto run = [&](const std::string& script) {
        std::istringstream input(script);
        std::ostringstream output;
        ConsoleAppLJY app(session, input, output);
        require(app.run() == 0, "Menu exit failed.");
        return output.str();
    };
    session.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
    const auto pending = run("8\nabc\n1\n2\n10002\n3\n10002\n0\n3\n10002\n1\n3\n10002\n1\n0\n0\n0\n");
    require(pending.find("已取消添加好友")!=std::string::npos && pending.find("申请已发送")!=std::string::npos && platform.friendRelationCount()==0,
        "Request menu created friendship before approval.");
    session.login(ServiceTypeLJY::QQ,"10002","TestOnly123!");
    const auto accepted=run("8\n9\n10001\n1\n0\n0\n0\n");
    require(accepted.find("已同意申请")!=std::string::npos,"Recipient menu approval failed.");
    session.login(ServiceTypeLJY::QQ,"10001","TestOnly123!");
    const auto output=run("8\n3\n10002\n4\n10002\n同学乙\n2\n10002\n5\n10002\n0\n1\n0\n0\n0\n");
    require(output.find("双方已经是好友")!=std::string::npos && output.find("已取消删除好友")!=std::string::npos &&
        output.find("我的备注：同学乙")!=std::string::npos,"Approved friend operations missing.");
    require(platform.friendRelationCount() == 1 && !session.isLoggedIn(), "Menu cancellation changed relationship.");
    session.login(ServiceTypeLJY::QQ, "10002", "TestOnly123!");
    const auto removed = run("8\n1\n5\n10001\n1\n1\n0\n0\n0\n");
    require(removed.find("我的备注：未设置") != std::string::npos && removed.find("好友已删除") != std::string::npos &&
            platform.friendRelationCount() == 0, "Reverse menu deletion failed.");
    for (const auto& tail : {std::string("3\n10002\n"), std::string("3\n0\n0\n0\n0\n")})
    {
        session.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
        run("8\n" + tail);
        require(platform.friendRelationCount() == 0 && !session.isLoggedIn(), "Cancelled add or EOF committed relationship.");
    }
    session.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
    session.addFriend("10002");
    run("8\n5\n10002\n");
    require(platform.friendRelationCount() == 1 && !session.isLoggedIn(), "EOF confirmed deletion.");
}

int main()
{
    int failed = 0;
    const auto run = [&](const char* name, void (*test)()) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    run("relation model", testModel);
    run("add and guards", testAddAndGuards);
    run("independent remarks and removal", testRemarksAndRemoval);
    run("service isolation", testServiceIsolation);
    run("session identity", testSession);
    run("friend menu", testMenu);
    std::cout << (6 - failed) << "/6 friend scenario groups passed.\n";
    return failed == 0 ? 0 : 1;
}
