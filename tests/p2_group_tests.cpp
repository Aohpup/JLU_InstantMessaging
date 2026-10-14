#include "services/DemoDataLJY.h"
#include "ui/ConsoleAppLJY.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

// 失败时说明业务不满足的条件。
void require(bool value, const char* reason)
{ if (!value) throw std::runtime_error(reason); }
// 非法数据须在修改前被拒绝。
void rejected(const std::function<void()>& action)
{
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid operation accepted.");
}

// 群自动包含群主，编号、名称及模式须有效。
void testModels()
{
    GroupLJY group(7, ServiceTypeLJY::QQ, 1001, "课程群", 3, GroupStyleLJY::QQ);
    require(group.groupId() == 7 && group.number() == 1001 && group.ownerAccountId() == 3,
            "Identifiers confused.");
    require(group.members().size() == 1 && group.findMember(3) && !group.findMember(9), "Owner missing.");
    require(group.parentGroupId() == 0 && group.style() == GroupStyleLJY::QQ, "Wrong initial mode.");
    rejected([] { GroupMemberLJY member(0); });
    rejected([] { GroupLJY g(0, ServiceTypeLJY::QQ, 1001, "G", 1, GroupStyleLJY::QQ); });
    rejected([] { GroupLJY g(1, ServiceTypeLJY::QQ, -1, "G", 1, GroupStyleLJY::QQ); });
    rejected([] { GroupLJY g(1, ServiceTypeLJY::QQ, 1001, "  ", 1, GroupStyleLJY::QQ); });
    rejected([] { GroupLJY g(1, ServiceTypeLJY::QQ, 1001, "G", 0, GroupStyleLJY::QQ); });
    rejected([] { GroupLJY g(1, static_cast<ServiceTypeLJY>(99), 1001, "G", 1, GroupStyleLJY::QQ); });
    rejected([] { GroupLJY g(1, ServiceTypeLJY::QQ, 1001, "G", 1, static_cast<GroupStyleLJY>(99)); });
}

// 对同一群使用两个基类引用，权限变化而群主、成员和管理员标记不变。
void testPolicies()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    GroupManagerLJY groups(platform);
    std::vector<int> ids;
    for (int i = 0; i < 5; ++i)
    {
        int person = accounts.createPerson("同学", "2000-01-01", "长春");
        ids.push_back(accounts.openService(person, ServiceTypeLJY::QQ, "昵称"));
    }
    int id = groups.createGroup(platform.findAccount(ids[0])->personId(), ids[0], 1001, "群",
        {GroupMemberLJY(ids[1],true), GroupMemberLJY(ids[2],true), GroupMemberLJY(ids[3])});
    const auto& group = *platform.findGroup(id);
    const GroupPolicyLJY& qq = groups.policy(GroupStyleLJY::QQ);
    const GroupPolicyLJY& wx = groups.policy(GroupStyleLJY::WeChat);
    require(qq.canApply(group,ids[4]) && !qq.canApply(group,ids[0]) && !qq.canApply(group,0), "QQ application.");
    require(!wx.canApply(group,ids[4]) && wx.canInvite(group,ids[3]) && !wx.canInvite(group,ids[4]), "WX invitation.");
    require(!qq.canInvite(group,ids[3]) && qq.canApprove(group,ids[0]) && qq.canApprove(group,ids[1]) &&
            !qq.canApprove(group,ids[3]) && !wx.canApprove(group,ids[0]), "Approval rights.");
    require(!qq.canKick(group,ids[0],ids[1]) && qq.canKick(group,ids[0],ids[3]) && qq.canKick(group,ids[1],ids[3]), "QQ kick rights.");
    require(!qq.canKick(group,ids[1],ids[2]) && !qq.canKick(group,ids[1],ids[0]) &&
            !qq.canKick(group,ids[0],ids[0]) && !qq.canKick(group,ids[4],ids[3]), "QQ illegal kick.");
    require(wx.canKick(group,ids[0],ids[1]) && !wx.canKick(group,ids[1],ids[3]) &&
            !wx.canKick(group,ids[0],ids[4]), "WX kick rights.");
    require(qq.canSetAdministrator(group,ids[0]) && !qq.canSetAdministrator(group,ids[1]) &&
            !wx.canSetAdministrator(group,ids[0]), "Admin assignment.");
    require(qq.canCreateSubgroup(group,ids[0]) && !qq.canCreateSubgroup(group,ids[1]) &&
            !wx.canCreateSubgroup(group,ids[0]), "Subgroup rules.");
    require(group.members().size() == 4 && group.findMember(ids[1])->isAdministrator() &&
            group.ownerAccountId() == ids[0] && group.style() == GroupStyleLJY::QQ,
            "Read-only strategy changed data.");
    rejected([&] { groups.policy(static_cast<GroupStyleLJY>(99)); });
}

// 同号群分服务保存，非法初始成员和非成员读取不能改变平台。
void testContainer()
{
    PlatformLJY platform;
    AccountServiceLJY accounts(platform);
    GroupManagerLJY groups(platform);
    int a = accounts.createPerson("A", "2000-01-01", "A"), b = accounts.createPerson("B", "2000-01-01", "B");
    int aq = accounts.openService(a,ServiceTypeLJY::QQ,"AQ"), bq = accounts.openService(b,ServiceTypeLJY::QQ,"BQ");
    int aw = accounts.openService(a,ServiceTypeLJY::WeChat,"AW"), bw = accounts.openService(b,ServiceTypeLJY::WeChat,"BW");
    int ab = accounts.openService(a,ServiceTypeLJY::Weibo,"AB");
    rejected([&] { groups.createGroup(b,aq,1001,"G"); });
    rejected([&] { groups.createGroup(a,aq,1001,"G",{GroupMemberLJY(bw)}); });
    rejected([&] { groups.createGroup(a,aq,1001,"G",{GroupMemberLJY(99999)}); });
    rejected([&] { groups.createGroup(a,aq,1001,"G",{GroupMemberLJY(aq)}); });
    rejected([&] { groups.createGroup(a,aq,1001,"G",{GroupMemberLJY(bq),GroupMemberLJY(bq)}); });
    rejected([&] { groups.createGroup(a,aw,1001,"G",{GroupMemberLJY(bw,true)}); });
    require(platform.groupCount() == 0, "Bad creation left a partial group.");
    int q = groups.createGroup(a,aq,1001,"QQ群",{GroupMemberLJY(bq,true)});
    int w = groups.createGroup(a,aw,1001,"微信群");
    int wb = groups.createGroup(a,ab,1001,"微博群");
    require(q != w && q != wb && w != wb && platform.groupCount() == 3, "Internal IDs not unique.");
    require(platform.findGroup(ServiceTypeLJY::QQ,1001)->groupId() == q &&
            platform.findGroup(ServiceTypeLJY::WeChat,1001)->groupId() == w &&
            platform.findGroup(wb)->style() == GroupStyleLJY::QQ, "Service lookup or default style failed.");
    require(groups.groups(b,bq).size() == 1 && groups.groups(b,bw).empty(), "Membership crossed services.");
    require(groups.memberGroup(b,bq,1001).groupId() == q, "Valid member blocked.");
    rejected([&] { groups.memberGroup(b,bw,1001); });
    rejected([&] { groups.memberGroup(b,aq,1001); });
    rejected([&] { groups.memberGroup(a,aq,1002); });
    rejected([&] { groups.groups(a,999); });
    rejected([&] { groups.createGroup(a,aq,1001,"duplicate"); });
    require(platform.groupCount() == 3 && platform.findGroup(q)->name() == "QQ群", "Rejected action changed data.");
}

// 演示必须恰有十八个有效群，重复加载不覆盖，空平台构造仍保持原行为。
void testDemo()
{
    PlatformLJY platform;
    require(platform.personCount() == 0 && platform.groupCount() == 0, "Constructor loaded demo implicitly.");
    loadDemoDataLJY(platform);
    require(platform.personCount() == 6 && platform.accountCount() == 12 && platform.groupCount() == 18,
            "Wrong demo size.");
    std::set<int> uniqueIds;
    for (auto type : {ServiceTypeLJY::QQ,ServiceTypeLJY::WeChat,ServiceTypeLJY::Weibo})
        for (int number = 1001; number <= 1006; ++number)
        {
            const auto* group = platform.findGroup(type,number);
            require(group && group->findMember(group->ownerAccountId()) && group->parentGroupId() == 0, "Invalid demo owner.");
            require(uniqueIds.insert(group->groupId()).second, "Duplicate internal group ID.");
            for (const auto& member : group->members())
            {
                const auto* account = platform.findAccount(member.accountId());
                require(account && account->serviceType() == type, "Wrong member service.");
                require(type != ServiceTypeLJY::WeChat || !member.isAdministrator(), "WX demo contains admin.");
            }
        }
    std::ostringstream out;
    showDemoAccountsLJY(platform,out);
    for (const auto* label : {"演示A","演示F","DemoOnly123!","未开通"})
        require(out.str().find(label) != std::string::npos, "Missing demo account guide.");
    AccountServiceLJY accounts(platform);
    const auto* owner = platform.findAccount(platform.findGroup(ServiceTypeLJY::QQ,1001)->ownerAccountId());
    require(accounts.authenticate(ServiceTypeLJY::QQ,owner->externalId(),"DemoOnly123!") == owner->personId(), "Demo login failed.");
    rejected([&] { loadDemoDataLJY(platform); });
    require(platform.groupCount() == 18 && platform.personCount() == 6, "Reload changed data.");
    PlatformLJY occupied;
    AccountServiceLJY real(occupied);
    int person = real.createPerson("保留", "2000-01-01", "吉林");
    rejected([&] { loadDemoDataLJY(occupied); });
    require(occupied.personCount() == 1 && occupied.findPerson(person)->name() == "保留" && occupied.groupCount() == 0,
            "Existing user overwritten.");
}

// 会话确定当前服务，群资料和成员通过一次查询返回；注销及新注册非成员都不能读取。
void testSession()
{
    PlatformLJY platform; loadDemoDataLJY(platform);
    SessionManagerLJY session(platform);
    std::vector<ServiceAccountLJY> members;
    rejected([&] { session.groups(); }); rejected([&] { session.group(1001,members); });
    const auto* a = platform.findAccount(platform.findGroup(ServiceTypeLJY::QQ,1001)->ownerAccountId());
    session.login(ServiceTypeLJY::QQ,a->externalId(),"DemoOnly123!");
    for (auto type : {ServiceTypeLJY::QQ,ServiceTypeLJY::WeChat,ServiceTypeLJY::Weibo})
    {
        if (session.currentAccount().serviceType() != type) session.enterService(type,true);
        require(session.groups().size() == 6, "A should be in six groups per service.");
        const auto& group = session.group(1001,members);
        require(group.serviceType() == type && members.size() == group.members().size(), "Wrong service or member list.");
        for (const auto& member : members) require(member.serviceType() == type, "Member list leaked across service.");
    }
    session.logout();
    rejected([&] { session.groups(); }); rejected([&] { session.group(1001,members); });
    const auto fresh = session.registerAccount("新用户","2000-01-01","吉林","DemoOnly123!",ServiceTypeLJY::QQ,"New");
    session.login(ServiceTypeLJY::QQ,fresh.externalId(),"DemoOnly123!");
    require(session.groups().empty(), "New user automatically joined demo group.");
    rejected([&] { session.group(1001,members); });
}

// 输入格式错误可重输，取消和输入结束不改变数据；群查询不会新增管理操作。
void testMenu()
{
    PlatformLJY platform; loadDemoDataLJY(platform);
    SessionManagerLJY session(platform);
    const auto* a = platform.findAccount(platform.findGroup(ServiceTypeLJY::QQ,1001)->ownerAccountId());
    const auto run = [&](const std::string& text) {
        std::istringstream input(text); std::ostringstream output;
        ConsoleAppLJY app(session,input,output);
        require(app.run() == 0 && !session.isLoggedIn(), "Menu did not cleanly exit.");
        return output.str();
    };
    session.login(ServiceTypeLJY::QQ,a->externalId(),"DemoOnly123!");
    auto out = run("9\n11wds\n1001\n2\n1\n9999\n0\n4\n2\n1\n9\n1001\n2\n0\n0\n0\n");
    for (const auto* text : {"QQ演示群1006","身份：群主","身份：管理员","身份：普通成员",
         "请输入 0 至","当前服务中不存在该群号","所属服务：微信","群管理（微信）"})
        require(out.find(text) != std::string::npos, text);
    require(platform.groupCount() == 18, "Read-only menu changed groups.");
    session.login(ServiceTypeLJY::QQ,a->externalId(),"DemoOnly123!");
    run("9\n");
    require(platform.groupCount() == 18, "EOF changed groups.");
}

int main()
{
    int failed=0;
    const auto run=[&](const char* name,void (*test)()) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& error) { ++failed; std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n'; }
    };
    run("group models",testModels); run("policy polymorphism",testPolicies);
    run("container and service isolation",testContainer); run("demo initialization",testDemo);
    run("session and member access",testSession); run("group menus",testMenu);
    std::cout<<(6-failed)<<"/6 group scenario groups passed.\n";
    return failed?1:0;
}
