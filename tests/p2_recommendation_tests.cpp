#include "ui/ConsoleAppLJY.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

// 失败即报告原因，避免只观察输出而漏掉数据变化。
void require(bool condition, const char* reason)
{
    if (!condition) throw std::runtime_error(reason);
}

// 错误输入和不满足条件的业务必须明确拒绝。
void rejected(const std::function<void()>& action)
{
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid operation accepted.");
}

// 虚构人员可故意同名，检验查询依据账号归属而非文字匹配。
int person(AccountServiceLJY& accounts, const std::string& name = "同名同学")
{
    return accounts.createPerson(name, "2000-01-01", "长春");
}

// 通过业务接口为测试人员开通三项服务并绑定微信。
void openAll(AccountServiceLJY& accounts, int id)
{
    const int qq = accounts.openService(id, ServiceTypeLJY::QQ, "同名昵称");
    const int wx = accounts.openService(id, ServiceTypeLJY::WeChat, "同名昵称");
    accounts.openService(id, ServiceTypeLJY::Weibo, "同名昵称");
    accounts.bindWeChatToQQ(id, wx, qq);
}

// 获取业务已创建账号的内部编号，不依赖发号偶然顺序。
int accountId(const PlatformLJY& platform, int id, ServiceTypeLJY type)
{
    return platform.findAccountForPerson(id, type)->accountId();
}

// 结果必须恰好包含指定账号，不能用同名账号替代。
void only(const std::vector<ServiceAccountLJY>& accounts, int expected)
{
    require(accounts.size() == 1 && accounts.front().accountId() == expected, "Wrong account set.");
}

// 检查未入选原因，确保过滤不是静默丢弃。
bool explains(const FriendQueryResultLJY& result, const std::string& text)
{
    for (const auto& reason : result.skippedReasons())
        if (reason.find(text) != std::string::npos) return true;
    return false;
}

// 同服务交集包括双方共同连接的人，双方自身不在交集中，拒绝跨服务及伪造身份。
void testSameService()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = person(accounts), b = person(accounts), c = person(accounts), d = person(accounts);
    openAll(accounts, a); openAll(accounts, b); openAll(accounts, c); openAll(accounts, d);
    const int aq = accountId(platform, a, ServiceTypeLJY::QQ), bq = accountId(platform, b, ServiceTypeLJY::QQ);
    const int cq = accountId(platform, c, ServiceTypeLJY::QQ), dq = accountId(platform, d, ServiceTypeLJY::QQ);
    friends.addFriend(a, aq, cq); friends.addFriend(a, aq, dq); friends.addFriend(b, bq, cq);
    only(friends.commonFriends(a, aq, bq), cq);
    friends.addFriend(a, aq, bq);
    only(friends.commonFriends(a, aq, bq), cq);
    only(friends.commonFriends(b, bq, aq), cq);
    rejected([&] { friends.commonFriends(a, aq, aq); });
    rejected([&] { friends.commonFriends(a, aq, 9999); });
    rejected([&] { friends.commonFriends(b, aq, bq); });
    rejected([&] { friends.commonFriends(a, aq, accountId(platform, b, ServiceTypeLJY::WeChat)); });
    friends.removeFriend(b, bq, cq);
    require(friends.commonFriends(a, aq, bq).empty(), "Removed edge remained in intersection.");
}

// 六种有向服务组合都使用相同映射规则，查询不写数据，确认添加后转入交集。
void testSixDirections()
{
    const ServiceTypeLJY types[] = {ServiceTypeLJY::QQ, ServiceTypeLJY::WeChat, ServiceTypeLJY::Weibo};
    for (auto source : types) for (auto target : types)
    {
        if (source == target) continue;
        PlatformLJY platform;
        AccountServiceLJY accounts(platform);
        FriendManagerLJY friends(platform);
        const int a = person(accounts), b = person(accounts);
        openAll(accounts, a); openAll(accounts, b);
        const int as = accountId(platform, a, source), bs = accountId(platform, b, source);
        const int at = accountId(platform, a, target), bt = accountId(platform, b, target);
        friends.addFriend(a, as, bs);
        friends.updateRemark(a, as, bs, "源服务备注");
        only(friends.friendRecommendations(a, at, source).accounts(), bt);
        require(friends.crossServiceCommonFriends(a, at, source).accounts().empty() &&
                platform.friendRelationCount() == 1, "Read query mutated relationships.");
        friends.addRecommendedFriend(a, at, source, bt);
        only(friends.friends(b, bt), at);
        only(friends.crossServiceCommonFriends(a, at, source).accounts(), bt);
        require(friends.friendRecommendations(a, at, source).accounts().empty(), "Existing friend recommended.");
        require(friends.friendRelation(a, as, bs).remarkFor(as) == "源服务备注" && friends.friendRelation(a, at, bt).remarkFor(at).empty(),
                "Recommendation copied or changed source remark.");
        rejected([&] { friends.addRecommendedFriend(a, at, source, bt); });
    }
}

// 本人须开通两侧账号，涉及微信须先绑定，不能以其他人员的源账号绕过。
void testActorGuards()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = person(accounts), b = person(accounts);
    const int aq = accounts.openService(a, ServiceTypeLJY::QQ, "A");
    rejected([&] { friends.friendRecommendations(a, aq, ServiceTypeLJY::Weibo); });
    rejected([&] { friends.friendRecommendations(a, aq, ServiceTypeLJY::QQ); });
    rejected([&] { friends.friendRecommendations(a, aq, static_cast<ServiceTypeLJY>(99)); });
    rejected([&] { friends.friendRecommendations(b, aq, ServiceTypeLJY::Weibo); });
    rejected([&] { friends.crossServiceCommonFriends(a, 999, ServiceTypeLJY::QQ); });
    const int aw = accounts.openService(a, ServiceTypeLJY::WeChat, "A");
    rejected([&] { friends.friendRecommendations(a, aw, ServiceTypeLJY::QQ); });
    rejected([&] { friends.crossServiceCommonFriends(a, aq, ServiceTypeLJY::WeChat); });
    accounts.bindWeChatToQQ(a, aw, aq);
    require(friends.friendRecommendations(a, aw, ServiceTypeLJY::QQ).accounts().empty(), "Empty list failed.");
}

// 混合场景分别排除未开通、未绑定及已有好友，同名的不同人员不能错误合并。
void testFilteringAndIdentity()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = person(accounts), b = person(accounts), c = person(accounts);
    const int d = person(accounts), e = person(accounts), f = person(accounts);
    openAll(accounts, a); openAll(accounts, b); openAll(accounts, e); openAll(accounts, f);
    accounts.openService(c, ServiceTypeLJY::QQ, "同名昵称");
    accounts.openService(d, ServiceTypeLJY::QQ, "同名昵称");
    accounts.openService(d, ServiceTypeLJY::WeChat, "同名昵称");
    const int aq = accountId(platform, a, ServiceTypeLJY::QQ), aw = accountId(platform, a, ServiceTypeLJY::WeChat);
    const int bw = accountId(platform, b, ServiceTypeLJY::WeChat), ew = accountId(platform, e, ServiceTypeLJY::WeChat);
    const int fw = accountId(platform, f, ServiceTypeLJY::WeChat);
    for (int peer : {b, c, d, e, f}) friends.addFriend(a, aq, accountId(platform, peer, ServiceTypeLJY::QQ));
    friends.addFriend(a, aw, ew);
    const auto recommended = friends.friendRecommendations(a, aw, ServiceTypeLJY::QQ);
    require(recommended.accounts().size() == 2 && recommended.accounts()[0].accountId() == bw &&
            recommended.accounts()[1].accountId() == fw, "Names merged distinct people or unmapped peer leaked.");
    require(recommended.skippedReasons().size() == 3 && explains(recommended, "未开通目标服务") &&
            explains(recommended, "缺少有效的微信与 QQ 绑定") && explains(recommended, "已经是当前服务好友"),
            "Missing exclusion reasons.");
    only(friends.crossServiceCommonFriends(a, aw, ServiceTypeLJY::QQ).accounts(), ew);
    // 手动添加的未绑定好友仍不算跨服务共同人员。
    const int dw = accountId(platform, d, ServiceTypeLJY::WeChat);
    // 直接提交未绑定对象也必须拒绝，不能仅靠推荐列表过滤。
    rejected([&] { friends.addRecommendedFriend(a, aw, ServiceTypeLJY::QQ, dw); });
    require(!platform.findFriendRelation(aw, dw), "Unbound recommendation created a relation.");
    friends.addFriend(a, aw, dw);
    only(friends.crossServiceCommonFriends(a, aw, ServiceTypeLJY::QQ).accounts(), ew);
    accounts.bindWeChatToQQ(d, dw, accountId(platform, d, ServiceTypeLJY::QQ));
    require(friends.crossServiceCommonFriends(a, aw, ServiceTypeLJY::QQ).accounts().size() == 2,
            "Valid new binding not reflected.");
}

// 微信与微博不能仅凭人员归属直接关联，必须经过已绑定的 QQ 及共享号码。
void testWeChatWeiboBridge()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = person(accounts), b = person(accounts);
    openAll(accounts, a);
    const int bw = accounts.openService(b, ServiceTypeLJY::WeChat, "B");
    const int bb = accounts.openService(b, ServiceTypeLJY::Weibo, "B");
    const int aw = accountId(platform, a, ServiceTypeLJY::WeChat), ab = accountId(platform, a, ServiceTypeLJY::Weibo);
    friends.addFriend(a, aw, bw);
    auto result = friends.friendRecommendations(a, ab, ServiceTypeLJY::WeChat);
    require(result.accounts().empty() && explains(result, "缺少有效"), "Missing QQ bypassed binding.");
    const int bq = accounts.openService(b, ServiceTypeLJY::QQ, "B");
    require(platform.findAccount(bq)->externalId() == platform.findAccount(bb)->externalId(), "Shared number lost.");
    require(friends.friendRecommendations(a, ab, ServiceTypeLJY::WeChat).accounts().empty(), "Unbound QQ accepted.");
    accounts.bindWeChatToQQ(b, bw, bq);
    only(friends.friendRecommendations(a, ab, ServiceTypeLJY::WeChat).accounts(), bb);
}

// 旧推荐不能在源关系删除后提交，任意账号、本人及重复添加均不应改变关系。
void testRecheckBeforeCommit()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const int a = person(accounts), b = person(accounts), c = person(accounts);
    openAll(accounts, a); openAll(accounts, b); openAll(accounts, c);
    const int aq = accountId(platform, a, ServiceTypeLJY::QQ), ab = accountId(platform, a, ServiceTypeLJY::Weibo);
    const int bq = accountId(platform, b, ServiceTypeLJY::QQ), bb = accountId(platform, b, ServiceTypeLJY::Weibo);
    friends.addFriend(a, aq, bq);
    const auto snapshot = friends.friendRecommendations(a, ab, ServiceTypeLJY::QQ);
    only(snapshot.accounts(), bb);
    rejected([&] { friends.addRecommendedFriend(a, ab, ServiceTypeLJY::QQ, ab); });
    rejected([&] { friends.addRecommendedFriend(a, ab, ServiceTypeLJY::QQ, bq); });
    rejected([&] { friends.addRecommendedFriend(a, ab, ServiceTypeLJY::QQ, accountId(platform, c, ServiceTypeLJY::Weibo)); });
    friends.removeFriend(a, aq, bq);
    rejected([&] { friends.addRecommendedFriend(a, ab, ServiceTypeLJY::QQ, snapshot.accounts()[0].accountId()); });
    require(platform.friendRelationCount() == 0, "Stale recommendation committed.");
    friends.addFriend(a, aq, bq);
    friends.addFriend(a, ab, bb);
    rejected([&] { friends.addRecommendedFriend(a, ab, ServiceTypeLJY::QQ, bb); });
    require(platform.friendRelationCount() == 2, "Duplicate commit mutated data.");
}

// 源服务可处于待确认状态；当前目标账号须登录，注销后所有新入口都受保护。
void testSessionAccess()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const auto a = session.registerAccount("测试甲", "2000-01-01", "长春", "DemoOnly123!", ServiceTypeLJY::QQ, "A");
    const int b = person(accounts); openAll(accounts, b);
    rejected([&] { session.commonFriends("10002"); });
    rejected([&] { session.friendRecommendations(ServiceTypeLJY::QQ); });
    rejected([&] { session.crossServiceCommonFriends(ServiceTypeLJY::QQ); });
    rejected([&] { session.addRecommendedFriend(ServiceTypeLJY::QQ, "10002"); });
    session.login(ServiceTypeLJY::QQ, a.externalId(), "DemoOnly123!");
    session.openService(ServiceTypeLJY::Weibo, "AB");
    friends.addFriend(a.personId(), a.accountId(), accountId(platform, b, ServiceTypeLJY::QQ));
    session.logout();
    session.login(ServiceTypeLJY::Weibo, a.externalId(), "DemoOnly123!");
    require(session.serviceState(ServiceTypeLJY::QQ) == ServiceLoginStateLJY::PendingConfirmation,
            "Source was unexpectedly logged in.");
    const auto& target = *platform.findAccountForPerson(b, ServiceTypeLJY::Weibo);
    only(session.friendRecommendations(ServiceTypeLJY::QQ).accounts(), target.accountId());
    session.addRecommendedFriend(ServiceTypeLJY::QQ, target.externalId());
    only(session.crossServiceCommonFriends(ServiceTypeLJY::QQ).accounts(), target.accountId());
    rejected([&] { session.addRecommendedFriend(ServiceTypeLJY::QQ, target.externalId()); });
    session.enterService(ServiceTypeLJY::QQ, true);
    require(session.friendRecommendations(ServiceTypeLJY::Weibo).accounts().empty(), "Current service ignored.");
    session.logout();
    rejected([&] { session.commonFriends(target.externalId()); });
    rejected([&] { session.friendRecommendations(ServiceTypeLJY::QQ); });
    rejected([&] { session.crossServiceCommonFriends(ServiceTypeLJY::QQ); });
    rejected([&] { session.addRecommendedFriend(ServiceTypeLJY::QQ, target.externalId()); });
}

// 菜单必须展示交集、过滤理由，确认前不提交；取消、非法序号和输入结束不会误加。
void testMenu()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    const auto a = session.registerAccount("测试甲", "2000-01-01", "长春", "DemoOnly123!", ServiceTypeLJY::QQ, "A");
    const int b = person(accounts), c = person(accounts);
    openAll(accounts, b); openAll(accounts, c);
    const int ab = accounts.openService(a.personId(), ServiceTypeLJY::Weibo, "AB");
    const int bq = accountId(platform, b, ServiceTypeLJY::QQ), cq = accountId(platform, c, ServiceTypeLJY::QQ);
    const int bb = accountId(platform, b, ServiceTypeLJY::Weibo), cb = accountId(platform, c, ServiceTypeLJY::Weibo);
    friends.addFriend(a.personId(), a.accountId(), bq);
    friends.addFriend(a.personId(), a.accountId(), cq);
    friends.addFriend(b, bq, cq);
    friends.addFriend(a.personId(), ab, cb);
    const auto run = [&](const std::string& script) {
        std::istringstream input(script); std::ostringstream output;
        ConsoleAppLJY app(session, input, output);
        require(app.run() == 0 && !session.isLoggedIn(), "Menu did not exit and logout.");
        return output.str();
    };
    session.login(ServiceTypeLJY::QQ, a.externalId(), "DemoOnly123!");
    const auto same = run("8\n6\n" + platform.findAccount(bq)->externalId() + "\n0\n0\n0\n");
    require(same.find("同服务共同好友：1 人") != std::string::npos, "Same-service menu missing.");
    for (const std::string tail : {"8\n0\n", "8\n1\n0\n", "8\n1\n1\n0\n", "8\n1\n1\n"})
    {
        session.login(ServiceTypeLJY::Weibo, a.externalId(), "DemoOnly123!");
        run("8\n" + tail);
        require(!platform.findFriendRelation(ab, bb), "Cancel or EOF added a recommendation.");
    }
    session.login(ServiceTypeLJY::Weibo, a.externalId(), "DemoOnly123!");
    const auto output=run("8\n7\n1\n8\n1\n1\n1\n0\n0\n0\n");
    require(output.find("跨服务共同好友：1 人")!=std::string::npos && output.find("推荐申请已发送")!=std::string::npos && !platform.findFriendRelation(ab,bb),
        "Recommendation did not wait for approval.");
    friends.handleRequest(b,bb,ab,true);
    session.login(ServiceTypeLJY::Weibo,a.externalId(),"DemoOnly123!");
    const auto approved=run("8\n7\n1\n8\n1\n0\n0\n0\n");
    require(approved.find("跨服务共同好友：2 人")!=std::string::npos && approved.find("可推荐好友：0 人")!=std::string::npos,
        "Approved recommendation not reflected in query.");
    only(friends.friends(b, bb), ab);
    session.login(ServiceTypeLJY::Weibo, a.externalId(), "DemoOnly123!");
    const auto error = run("8\n8\n3\n7\n2\n6\n999999\n0\n0\n0\n");
    require(error.find("请选择与当前服务不同") != std::string::npos && error.find("本人尚未开通") != std::string::npos &&
            error.find("当前服务中不存在该号码") != std::string::npos, "Errors failed to stay in friend menu.");
}

int main()
{
    int failed = 0;
    const auto run = [&](const char* name, void (*test)()) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    run("same-service intersection", testSameService);
    run("six mapping directions", testSixDirections);
    run("actor and source guards", testActorGuards);
    run("filtering and same-name identity", testFilteringAndIdentity);
    run("WeChat-Weibo QQ bridge", testWeChatWeiboBridge);
    run("recheck before commit", testRecheckBeforeCommit);
    run("session access", testSessionAccess);
    run("recommendation menus", testMenu);
    std::cout << (8 - failed) << "/8 recommendation scenario groups passed.\n";
    return failed == 0 ? 0 : 1;
}
