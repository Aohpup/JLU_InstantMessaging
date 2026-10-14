#include "services/DemoDataLJY.h"
#include "ui/ConsoleAppLJY.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

void require(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
// 所有被拒绝操作均由后续断言确认未改变成员或请求。
void rejected(const std::function<void()>& action)
{
    try { action(); } catch(const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid operation accepted.");
}
int person(const PlatformLJY& p,int account) { return p.findAccount(account)->personId(); }
// 前三人入群：群主、管理员或成员、普通成员；后两人用于申请和邀请。
std::vector<int> setup(PlatformLJY& p,ServiceTypeLJY type)
{
    AccountServiceLJY accounts(p); GroupManagerLJY groups(p);
    std::vector<int> ids;
    for (int i=0;i<5;++i)
    {
        int id=accounts.createPerson("同学","2000-01-01","长春");
        ids.push_back(accounts.openService(id,type,"昵称"));
    }
    groups.createGroup(person(p,ids[0]),ids[0],1001,"测试群",
        {GroupMemberLJY(ids[1],type!=ServiceTypeLJY::WeChat),GroupMemberLJY(ids[2])});
    return ids;
}
GroupRequestStateLJY state(GroupManagerLJY& g,const PlatformLJY& p,int viewer,int id)
{
    for (const auto& request:g.requests(person(p,viewer),viewer)) if(request.id()==id) return request.state();
    throw std::runtime_error("Request not visible.");
}
void testModel()
{
    GroupRequestLJY r(1,ServiceTypeLJY::QQ,1001,GroupRequestTypeLJY::Application,"10001","10001");
    require(r.state()==GroupRequestStateLJY::Pending && r.id()==1 && r.groupNumber()==1001,"Request initial state.");
    rejected([] { GroupRequestLJY r(0,ServiceTypeLJY::QQ,1001,GroupRequestTypeLJY::Application,"a","a"); });
    rejected([] { GroupRequestLJY r(1,ServiceTypeLJY::QQ,1001,GroupRequestTypeLJY::Application,"a","b"); });
    rejected([] { GroupRequestLJY r(1,ServiceTypeLJY::QQ,1001,GroupRequestTypeLJY::Invitation,"a","a"); });
    rejected([] { GroupRequestLJY r(1,static_cast<ServiceTypeLJY>(99),1001,GroupRequestTypeLJY::Invitation,"a","b"); });
}
void testApplications()
{
    PlatformLJY p;auto a=setup(p,ServiceTypeLJY::QQ);GroupManagerLJY g(p);
    const auto* group=p.findGroup(ServiceTypeLJY::QQ,1001);
    int r=g.apply(person(p,a[3]),a[3],1001);
    require(!group->findMember(a[3]),"Application joined before approval.");
    rejected([&]{g.apply(person(p,a[3]),a[3],1001);});
    rejected([&]{g.apply(person(p,a[0]),a[0],1001);});
    rejected([&]{g.handleRequest(person(p,a[2]),a[2],r,true);});
    rejected([&]{g.handleRequest(person(p,a[3]),a[3],r,true);});
    require(g.requests(person(p,a[4]),a[4]).empty(),"Unrelated user saw request.");
    g.handleRequest(person(p,a[1]),a[1],r,false);
    require(state(g,p,a[3],r)==GroupRequestStateLJY::Rejected && !group->findMember(a[3]),"Reject mutated members.");
    r=g.apply(person(p,a[3]),a[3],1001);
    g.setAdministrator(person(p,a[0]),a[0],1001,a[1],false);
    rejected([&]{g.handleRequest(person(p,a[1]),a[1],r,true);});
    g.handleRequest(person(p,a[0]),a[0],r,true);
    require(group->findMember(a[3]) && !group->findMember(a[3])->isAdministrator(),"Accepted applicant role incorrect.");
    rejected([&]{g.handleRequest(person(p,a[0]),a[0],r,true);});
    require(state(g,p,a[3],r)==GroupRequestStateLJY::Accepted && group->members().size()==4,"Double approval changed data.");
}
void testInvitations()
{
    PlatformLJY p;auto a=setup(p,ServiceTypeLJY::WeChat);GroupManagerLJY g(p);
    const auto* group=p.findGroup(ServiceTypeLJY::WeChat,1001);
    rejected([&]{g.apply(person(p,a[3]),a[3],1001);});
    rejected([&]{g.invite(person(p,a[4]),a[4],1001,a[3]);});
    int r=g.invite(person(p,a[1]),a[1],1001,a[3]);
    require(!group->findMember(a[3]),"Invitation joined before acceptance.");
    rejected([&]{g.invite(person(p,a[0]),a[0],1001,a[3]);});
    rejected([&]{g.handleRequest(person(p,a[1]),a[1],r,true);});
    rejected([&]{g.handleRequest(person(p,a[0]),a[0],r,true);});
    g.handleRequest(person(p,a[3]),a[3],r,false);
    require(!group->findMember(a[3]),"Rejected invitation joined.");
    r=g.invite(person(p,a[1]),a[1],1001,a[3]);
    g.leave(person(p,a[1]),a[1],1001);
    require(state(g,p,a[3],r)==GroupRequestStateLJY::Cancelled,"Inviter leaving did not invalidate request.");
    rejected([&]{g.handleRequest(person(p,a[3]),a[3],r,true);});
    // 邀请人重新入群也不能恢复旧邀请。
    int back=g.invite(person(p,a[0]),a[0],1001,a[1]);
    g.handleRequest(person(p,a[1]),a[1],back,true);
    rejected([&]{g.handleRequest(person(p,a[3]),a[3],r,true);});
    int next=g.invite(person(p,a[1]),a[1],1001,a[3]);
    g.handleRequest(person(p,a[3]),a[3],next,true);
    require(group->findMember(a[3]) && !group->findMember(a[3])->isAdministrator(),"WX accepted role incorrect.");
    int kicked=g.invite(person(p,a[1]),a[1],1001,a[4]);
    g.kick(person(p,a[0]),a[0],1001,a[1]);
    rejected([&]{g.handleRequest(person(p,a[4]),a[4],kicked,true);});
    require(!group->findMember(a[4]) && state(g,p,a[4],kicked)==GroupRequestStateLJY::Cancelled,"Kicked inviter not invalidated.");
}
void testRoles()
{
    for(auto type:{ServiceTypeLJY::QQ,ServiceTypeLJY::WeChat,ServiceTypeLJY::Weibo})
    {
        PlatformLJY p;auto a=setup(p,type);GroupManagerLJY g(p);
        const auto* group=p.findGroup(type,1001);
        rejected([&]{g.kick(person(p,a[2]),a[2],1001,a[1]);});
        rejected([&]{g.kick(person(p,a[1]),a[1],1001,a[0]);});
        rejected([&]{g.kick(person(p,a[0]),a[0],1001,a[0]);});
        rejected([&]{g.setAdministrator(person(p,a[1]),a[1],1001,a[2],true);});
        if(type==ServiceTypeLJY::WeChat)
        {
            rejected([&]{g.setAdministrator(person(p,a[0]),a[0],1001,a[2],true);});
            rejected([&]{g.kick(person(p,a[1]),a[1],1001,a[2]);});
        }
        else
        {
            g.setAdministrator(person(p,a[0]),a[0],1001,a[2],true);
            rejected([&]{g.kick(person(p,a[1]),a[1],1001,a[2]);});
            g.setAdministrator(person(p,a[0]),a[0],1001,a[2],false);
            g.kick(person(p,a[1]),a[1],1001,a[2]);
        }
        if(type!=ServiceTypeLJY::WeChat)
        {
            rejected([&]{g.kick(person(p,a[0]),a[0],1001,a[1]);});
            require(group->findMember(a[1])->isAdministrator(),"Rejected kick changed administrator.");
            g.setAdministrator(person(p,a[0]),a[0],1001,a[1],false);
        }
        g.kick(person(p,a[0]),a[0],1001,a[1]);
        require(!group->findMember(a[1]) && group->findMember(a[0]),"Owner kick failed.");
    }
}
void testTransferLeave()
{
    for(auto type:{ServiceTypeLJY::QQ,ServiceTypeLJY::WeChat})
    {
        PlatformLJY p;auto a=setup(p,type);GroupManagerLJY g(p);const auto* group=p.findGroup(type,1001);
        rejected([&]{g.leave(person(p,a[0]),a[0],1001);});
        rejected([&]{g.transferOwner(person(p,a[1]),a[1],1001,a[2]);});
        rejected([&]{g.transferOwner(person(p,a[0]),a[0],1001,a[3]);});
        rejected([&]{g.transferOwner(person(p,a[0]),a[0],1001,a[0]);});
        g.transferOwner(person(p,a[0]),a[0],1001,a[1]);
        require(group->ownerAccountId()==a[1] && !group->findMember(a[1])->isAdministrator() &&
                !group->findMember(a[0])->isAdministrator(),"Transfer left incorrect roles.");
        rejected([&]{g.kick(person(p,a[0]),a[0],1001,a[2]);});
        g.leave(person(p,a[0]),a[0],1001);g.leave(person(p,a[2]),a[2],1001);
        require(group->members().size()==1 && group->findMember(a[1]),"Group became ownerless.");
        rejected([&]{g.leave(person(p,a[1]),a[1],1001);});
    }
}
void testIsolation()
{
    PlatformLJY p;auto a=setup(p,ServiceTypeLJY::QQ);GroupManagerLJY g(p);AccountServiceLJY accounts(p);
    int aw=accounts.openService(person(p,a[0]),ServiceTypeLJY::WeChat,"WX");
    int dw=accounts.openService(person(p,a[3]),ServiceTypeLJY::WeChat,"WX");
    g.createGroup(person(p,aw),aw,1001,"WX");
    int r=g.apply(person(p,a[3]),a[3],1001);
    rejected([&]{g.handleRequest(person(p,aw),aw,r,true);});
    rejected([&]{g.apply(person(p,a[0]),a[3],1001);});
    rejected([&]{g.invite(person(p,aw),aw,1001,a[4]);});
    rejected([&]{g.kick(person(p,a[0]),a[0],1001,dw);});
    rejected([&]{g.transferOwner(person(p,a[0]),a[0],1001,dw);});
    rejected([&]{g.setAdministrator(person(p,a[0]),a[0],1001,dw,true);});
    rejected([&]{g.apply(person(p,a[3]),a[3],9999);});
    int invite=g.invite(person(p,aw),aw,1001,dw);
    rejected([&]{g.handleRequest(person(p,a[3]),a[3],invite,true);});
    require(state(g,p,a[3],r)==GroupRequestStateLJY::Pending && !p.findGroup(ServiceTypeLJY::QQ,1001)->findMember(a[3]),
            "Cross-service attempt changed request.");
}
void testSessions()
{
    PlatformLJY p;loadDemoDataLJY(p);SessionManagerLJY s(p);
    std::vector<std::function<void()>> actions={
        [&]{s.applyToGroup(1001);},[&]{s.inviteToGroup(1001,"10002");},[&]{s.groupRequests();},
        [&]{s.handleGroupRequest(1,true);},[&]{s.leaveGroup(1001);},[&]{s.kickGroupMember(1001,"10002");},
        [&]{s.setGroupAdministrator(1001,"10002",true);},[&]{s.transferGroupOwner(1001,"10002");}};
    for(const auto& action:actions) rejected(action);
    auto a=s.registerAccount("新人","2000-01-01","吉林","DemoOnly123!",ServiceTypeLJY::QQ,"新人");
    s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");int id=s.applyToGroup(1001);s.logout();
    for(const auto& action:actions) rejected(action);
    s.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");s.handleGroupRequest(id,true);s.logout();
    s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");require(s.groups().size()==1,"Session approval did not add member.");
    s.leaveGroup(1001);require(s.groups().empty(),"Session leave failed.");
}
void testMenus()
{
    PlatformLJY p;loadDemoDataLJY(p);SessionManagerLJY s(p);
    auto a=s.registerAccount("新人","2000-01-01","吉林","DemoOnly123!",ServiceTypeLJY::QQ,"新人");
    auto run=[&](const std::string& input) {
        std::istringstream in(input);std::ostringstream out;ConsoleAppLJY app(s,in,out);
        require(app.run()==0 && !s.isLoggedIn(),"Menu exit failed.");return out.str();};
    for(const std::string tail:{"9\n1001\n3\n0\n0\n0\n0\n","9\n1001\n3\n"})
    {
        s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");run(tail);
        s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");require(s.groupRequests().empty(),"Cancelled or EOF application submitted.");s.logout();
    }
    s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");
    auto output=run("9\n1001\n3\n1\n5\n0\n0\n0\n");
    require(output.find("申请已提交，请求编号：1")!=std::string::npos,"Apply menu missing.");
    s.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
    run("9\n1001\n6\n1\n0\n0\n0\n0\n");
    s.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
    output=run("9\n1001\n6\n1\n1\n9\n10006\n1\n1\n10\n10006\n0\n2\n0\n0\n0\n");
    require(output.find("已同意，对方已入群")!=std::string::npos && output.find("管理员设置已更新")!=std::string::npos,
            "Handle/admin menus missing.");
    require(p.findGroup(ServiceTypeLJY::QQ,1001)->ownerAccountId()!=a.accountId(),"Cancelled transfer executed.");
    s.login(ServiceTypeLJY::QQ,a.externalId(),"DemoOnly123!");
    output=run("9\n1001\n7\n0\n7\n1\n0\n0\n0\n");
    require(output.find("已退出群")!=std::string::npos && output.find("当前服务暂无加入的群")!=std::string::npos,"Leave menu failed.");
}
int main()
{
    int failed=0;auto run=[&](const char* name,void (*f)()) {
        try{f();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    run("request model",testModel);run("QQ applications and fresh permissions",testApplications);
    run("WX invitations and invalidation",testInvitations);run("member roles",testRoles);
    run("transfer and leave",testTransferLeave);run("service isolation",testIsolation);
    run("session access",testSessions);run("operation menus",testMenus);
    std::cout<<(8-failed)<<"/8 group operation scenario groups passed.\n";return failed?1:0;
}
