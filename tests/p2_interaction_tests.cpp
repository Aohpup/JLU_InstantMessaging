#include "services/DemoDataLJY.h"
#include "ui/ConsoleAppLJY.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void contains(const std::string& text, const std::string& part)
{ require(text.find(part) != std::string::npos, part.c_str()); }
void absent(const std::string& text, const std::string& part)
{ require(text.find(part) == std::string::npos, part.c_str()); }
void denied(const std::function<void()>& action, const std::string& reason)
{
    try { action(); } catch (const std::invalid_argument& e) { contains(e.what(), reason); return; }
    throw std::runtime_error("Expected rejection: " + reason);
}
std::string run(SessionManagerLJY& session, const std::string& input)
{
    std::istringstream in(input); std::ostringstream out; ConsoleAppLJY app(session, in, out);
    require(app.run() == 0 && !session.isLoggedIn(), "Menu did not exit cleanly.");
    return out.str();
}
void login(SessionManagerLJY& s, const std::string& number="10001", ServiceTypeLJY type=ServiceTypeLJY::QQ)
{ s.login(type, number, "DemoOnly123!"); }

// 无效好友在确认之前被拒绝，不能留下关系修改。
void testFriendInput()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p);
    const int before=p.friendRelationCount();
    for (const auto& number : {"10003", "10001", "99999"})
    {
        login(s); auto out=run(s, std::string("8\n3\n")+number+"\n");
        contains(out, "操作失败："); absent(out, "确认发送好友申请"); absent(out, "目标号码：");
        if (std::string(number)=="10003") contains(out, "双方已经是好友。");
    }
    require(before==p.friendRelationCount(), "Rejected friend input mutated data.");
}
// 当前服务不能重复切换，其他服务的确认或取消不受影响。
void testSwitch()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p); login(s);
    denied([&]{s.enterService(ServiceTypeLJY::QQ,true);},"当前已在该服务中");
    require(s.currentAccount().serviceType()==ServiceTypeLJY::QQ,"Self switch changed account.");
    require(!s.enterService(ServiceTypeLJY::WeChat,false),"Cancel entered service.");
    require(s.serviceState(ServiceTypeLJY::WeChat)==ServiceLoginStateLJY::PendingConfirmation,"Cancel confirmed login.");
    s.enterService(ServiceTypeLJY::WeChat,true); s.enterService(ServiceTypeLJY::QQ,true);
    auto out=run(s,"4\n1\n"); contains(out,"当前已在该服务中"); absent(out,"已进入QQ");
}
// 已有绑定优先于登录条件，从 QQ 或微信进入时给出相应账号提示。
void testBinding()
{
    for (auto type : {ServiceTypeLJY::QQ,ServiceTypeLJY::WeChat})
    {
        PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p);
        login(s,type==ServiceTypeLJY::QQ ? "10001" : "wx_20001",type);
        const auto other=type==ServiceTypeLJY::QQ ? ServiceTypeLJY::WeChat : ServiceTypeLJY::QQ;
        require(s.serviceState(other)==ServiceLoginStateLJY::PendingConfirmation,"Other service already confirmed.");
        auto out=run(s,"7\n"); contains(out,type==ServiceTypeLJY::QQ ? "QQ 账号已绑定。" : "微信账号已绑定。");
        absent(out,"请先分别用密码登录");
    }
}
// 不存在的群不会进入操作菜单或目标输入；可重选有效群继续操作。
void testGroupSelection()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p); login(s);
    auto out=run(s,"9\n9999\n"); contains(out,"当前服务中不存在该群号");
    absent(out,"目标账号号码"); absent(out,"确认执行"); absent(out,"2 查询群资料和成员");
    login(s); out=run(s,"9\n9999\n1001\n2\n1\n1002\n2\n");
    contains(out,"当前群：1001"); contains(out,"当前群：1002"); contains(out,"身份：群主");
    require(p.groupCount()==18,"Queries changed groups.");
}
// 目标输入后拒绝移除自己、管理员、不存在账号以及其他群成员。
void testKickInput()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p);
    const auto* g=p.findGroup(ServiceTypeLJY::QQ,1001); const auto before=g->members().size();
    for(const auto& number : {"10001","10002","99999"})
    {
        login(s); auto out=run(s,std::string("9\n1001\n8\n")+number+"\n");
        contains(out,"操作失败："); absent(out,"确认执行");
        require(out.find("身份：管理员")<out.find("目标账号号码"),"Member list was not shown first.");
        if(std::string(number)=="10001") contains(out,"不可移除自己");
        if(std::string(number)=="10002") contains(out,"请先取消其管理员身份再移除");
    }
    require(g->members().size()==before,"Failed kick changed members.");
    login(s,"10002"); auto out=run(s,"9\n1002\n8\n10005\n");
    contains(out,"目标账号不是该群成员"); absent(out,"确认执行");
}
// 角色拒绝原因具体，普通成员在目标输入前就被拒绝。
void testReasons()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p); GroupManagerLJY g(p);
    const auto* a=p.findAccount(ServiceTypeLJY::QQ,"10001");
    const auto* c=p.findAccount(ServiceTypeLJY::QQ,"10003");
    g.setAdministrator(a->personId(),a->accountId(),1001,c->accountId(),true);
    for(const auto& target : {"10001","10003"})
    {
        login(s,"10002"); auto out=run(s,std::string("9\n1001\n8\n")+target+"\n");
        contains(out,std::string(target)=="10001" ? "管理员不可移除群主" : "管理员不可移除管理员");
        absent(out,"确认执行");
    }
    login(s,"10004"); auto out=run(s,"9\n1001\n8\n9\n10\n");
    contains(out,"普通成员不可移除成员"); contains(out,"普通成员不可任免管理员");
    contains(out,"只有群主可以转让群主身份"); absent(out,"目标账号号码");
    login(s,"wx_20001",ServiceTypeLJY::WeChat); out=run(s,"9\n1002\n8\n9\n");
    contains(out,"微信群仅群主可以移除成员"); contains(out,"微信群不设管理员"); absent(out,"目标账号号码");
}
// 取消不改身份；实际撤销后才允许移除管理员，业务入口也执行同样规则。
void testDemoteThenKick()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p); login(s);
    const auto* g=p.findGroup(ServiceTypeLJY::QQ,1001);const auto* b=p.findAccount(ServiceTypeLJY::QQ,"10002");
    denied([&]{s.kickGroupMember(1001,"10002");},"请先取消其管理员身份");
    auto out=run(s,"9\n1001\n9\n10002\n2\n0\n");
    require(g->findMember(b->accountId())->isAdministrator(),"Cancelled demotion changed role.");
    require(out.find("身份：管理员")<out.find("目标账号号码"),"Admin selection missing members.");
    login(s); out=run(s,"9\n1001\n9\n10002\n2\n1\n8\n10002\n1\n");
    contains(out,"管理员设置已更新"); contains(out,"成员已移除");
    require(!g->findMember(b->accountId()),"Demoted member not removed.");
    login(s); out=run(s,"9\n1001\n8\n10003\n");
    require(g->findMember(p.findAccount(ServiceTypeLJY::QQ,"10003")->accountId()),"EOF committed kick.");
}
// 当前群只显示和处理自己的请求，非成员仍可选群申请或接受邀请。
void testRequestScope()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p);
    auto fresh=s.registerAccount("新同学","2000-01-01","吉林","DemoOnly123!",ServiceTypeLJY::QQ,"新同学");
    login(s,fresh.externalId());const int first=s.applyToGroup(1001);const int second=s.applyToGroup(1002);s.logout();
    login(s); auto out=run(s,"9\n1001\n5\n6\n"+std::to_string(second)+"\n");
    contains(out,"请求编号："+std::to_string(first)); absent(out,"请求编号："+std::to_string(second));
    contains(out,"当前群中不存在该请求编号"); absent(out,"处理：1 同意");
    login(s);run(s,"9\n1001\n6\n"+std::to_string(first)+"\n1\n");
    require(p.findGroup(ServiceTypeLJY::QQ,1001)->findMember(fresh.accountId()),"Approval did not join selected group.");
    require(!p.findGroup(ServiceTypeLJY::QQ,1002)->findMember(fresh.accountId()),"Wrong group request was processed.");
    login(s,"wx_20001",ServiceTypeLJY::WeChat);const int invitation=s.inviteToGroup(1002,"wx_20004");s.logout();
    login(s,"wx_20004",ServiceTypeLJY::WeChat);
    out=run(s,"9\n1002\n6\n"+std::to_string(invitation)+"\n1\n2\n");
    contains(out,"已同意，对方已入群");contains(out,"号码：wx_20004");
}
int main()
{
    int failed=0;
    auto test=[&](const char* name, void(*fn)()) {
        try { fn(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    };
    test("early friend validation",testFriendInput);test("service switch",testSwitch);
    test("existing binding priority",testBinding);test("group selection",testGroupSelection);
    test("kick input and member listing",testKickInput);test("specific role reasons",testReasons);
    test("demote before kick and cancellation",testDemoteThenKick);test("selected group requests",testRequestScope);
    std::cout<<(8-failed)<<"/8 interaction scenario groups passed.\n";return failed?1:0;
}
