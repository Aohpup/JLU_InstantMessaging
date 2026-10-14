#include "services/DemoDataLJY.h"
#include "ui/ConsoleAppLJY.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void denied(const std::function<void()>& action)
{
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected rejection.");
}
const ServiceAccountLJY& qq(const PlatformLJY& p, const char* number)
{ return *p.findAccount(ServiceTypeLJY::QQ, number); }

// 快照仅用于测试对比；正式切换代码不复制平台或构造回滚框架。
std::string data(const GroupLJY& group)
{
    std::ostringstream out;
    out << group.groupId() << ' ' << group.number() << ' ' << static_cast<int>(group.serviceType())
        << ' ' << group.name() << ' ' << group.ownerAccountId() << ' ' << group.parentGroupId();
    for (const auto& m : group.members()) out << ' ' << m.accountId() << ':' << m.isAdministrator();
    return out.str();
}
std::string requests(GroupManagerLJY& g, const ServiceAccountLJY& a)
{
    std::ostringstream out;
    for (const auto& r : g.requests(a.personId(), a.accountId()))
        out << r.id() << ':' << r.groupNumber() << ':' << static_cast<int>(r.type()) << ':'
            << r.senderNumber() << ':' << r.targetNumber() << ':' << static_cast<int>(r.state()) << ';';
    return out.str();
}

// 只测必要的归属、一级子群和权限，创建失败不遗留半个群。
void testCreation()
{
    PlatformLJY p; loadDemoDataLJY(p); GroupManagerLJY g(p);
    const auto& a=qq(p,"10001"); const auto& b=qq(p,"10002"); const auto& c=qq(p,"10003");
    const int childId=g.createSubgroup(a.personId(),a.accountId(),1001,2001,"讨论组",b.accountId(),{c.accountId()});
    const auto& child=*p.findGroup(childId);
    require(child.parentGroupId()==p.findGroup(ServiceTypeLJY::QQ,1001)->groupId(),"Parent link missing.");
    require(child.members().size()==2 && child.ownerAccountId()==b.accountId(),"Wrong initial members.");
    require(g.subgroups(a.personId(),a.accountId(),1001).size()==1,"Subgroup list missing.");
    denied([&]{g.createSubgroup(b.personId(),b.accountId(),1001,2002,"越权",b.accountId(),{});});
    denied([&]{g.createSubgroup(b.personId(),b.accountId(),2001,2002,"二级",b.accountId(),{});});
    AccountServiceLJY accounts(p);
    int newcomer=accounts.createPerson("新同学","2000-01-01","吉林");
    int outsider=accounts.openService(newcomer,ServiceTypeLJY::QQ,"新人");
    denied([&]{g.createSubgroup(a.personId(),a.accountId(),1001,2002,"无效成员",a.accountId(),{outsider});});
    denied([&]{g.apply(newcomer,outsider,2001);});
    require(p.groupCount()==19,"Invalid creation changed group count.");
    int request=g.apply(a.personId(),a.accountId(),2001);
    g.handleRequest(b.personId(),b.accountId(),request,true);
    require(child.findMember(a.accountId()),"Parent member could not join child.");
}

// 多个子群先统一检查；父群移除同步所有子群。
void testRemoval()
{
    PlatformLJY p; loadDemoDataLJY(p); GroupManagerLJY g(p);
    const auto& a=qq(p,"10001"); const auto& b=qq(p,"10002");
    const auto& c=qq(p,"10003"); const auto& e=qq(p,"10005");
    auto first=g.createSubgroup(a.personId(),a.accountId(),1001,2001,"一组",a.accountId(),{b.accountId(),c.accountId()});
    auto second=g.createSubgroup(a.personId(),a.accountId(),1001,2002,"二组",c.accountId(),{b.accountId()});
    auto before=data(*p.findGroup(first));
    denied([&]{g.leave(c.personId(),c.accountId(),1001);});
    denied([&]{g.kick(a.personId(),a.accountId(),1001,c.accountId());});
    require(data(*p.findGroup(first))==before && p.findGroup(ServiceTypeLJY::QQ,1001)->findMember(c.accountId()),
        "Failed parent removal partially changed groups.");
    g.transferOwner(c.personId(),c.accountId(),2002,b.accountId());
    int pending=g.apply(e.personId(),e.accountId(),2001);
    g.kick(a.personId(),a.accountId(),1001,c.accountId());
    require(!p.findGroup(first)->findMember(c.accountId()) && !p.findGroup(second)->findMember(c.accountId()),
        "Parent kick did not clear all children.");
    g.leave(e.personId(),e.accountId(),1001);
    denied([&]{g.handleRequest(a.personId(),a.accountId(),pending,true);});
    require(!p.findGroup(first)->findMember(e.accountId()),"Former parent member joined through stale request.");
}

// 切换只改变本群规则；子群、成员、请求和自身模式均保留。
void testModes()
{
    PlatformLJY p; loadDemoDataLJY(p); GroupManagerLJY g(p);
    const auto& a=qq(p,"10001"); const auto& b=qq(p,"10002");
    const auto& c=qq(p,"10003"); const auto& d=qq(p,"10004");
    const int child=g.createSubgroup(a.personId(),a.accountId(),1001,2001,"讨论组",b.accountId(),{a.accountId(),c.accountId()});
    const int pending=g.apply(d.personId(),d.accountId(),2001);
    const auto parentData=data(*p.findGroup(ServiceTypeLJY::QQ,1001));
    const auto childData=data(*p.findGroup(child));
    const auto requestData=requests(g,b);
    denied([&]{g.switchMode(b.personId(),b.accountId(),1001,GroupStyleLJY::WeChat);});
    denied([&]{g.switchMode(a.personId(),a.accountId(),1001,static_cast<GroupStyleLJY>(99));});
    g.switchMode(a.personId(),a.accountId(),1001,GroupStyleLJY::WeChat);
    require(data(*p.findGroup(ServiceTypeLJY::QQ,1001))==parentData && data(*p.findGroup(child))==childData,
        "Switch changed group data.");
    require(requests(g,b)==requestData && g.subgroups(a.personId(),a.accountId(),1001).size()==1,
        "Subgroup or request lost.");
    require(p.findGroup(child)->style()==GroupStyleLJY::QQ,"Child policy changed with parent.");
    denied([&]{g.kick(b.personId(),b.accountId(),1001,c.accountId());});
    denied([&]{g.createSubgroup(a.personId(),a.accountId(),1001,2002,"新子群",a.accountId(),{});});
    g.handleRequest(b.personId(),b.accountId(),pending,true);
    require(p.findGroup(child)->findMember(d.accountId()),"Retained child request cannot be handled.");
    g.kick(a.personId(),a.accountId(),1001,c.accountId());
    require(!p.findGroup(child)->findMember(c.accountId()),"WeChat parent removal did not reach retained child.");
    g.switchMode(a.personId(),a.accountId(),1001,GroupStyleLJY::QQ);
    require(p.findGroup(child) && p.findGroup(ServiceTypeLJY::QQ,1001)->findMember(b.accountId())->isAdministrator(),
        "Returning to QQ lost child or administrator role.");
    const int next=g.createSubgroup(a.personId(),a.accountId(),1001,2002,"第二子群",a.accountId(),{});
    require(next>child,"Child IDs not continued.");
}

// 通过真实菜单函数验证先选群、取消/EOF不提交、模式及时刷新及保留提示。
void testMenus()
{
    PlatformLJY p; loadDemoDataLJY(p); SessionManagerLJY s(p);
    auto run=[&](const std::string& script) {
        s.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
        std::istringstream in(script);std::ostringstream out;ConsoleAppLJY app(s,in,out);
        require(app.run()==0,"Console failed.");return out.str();
    };
    run("9\n1001\n11\n2001\n讨论组\n10001\n10003\n0\n0\n");
    run("9\n1001\n11\n2001\n讨论组\n10001\n10003\n0\n");
    require(p.groupCount()==18,"Cancelled/EOF creation changed groups.");
    auto out=run("9\n1001\n11\n2001\n讨论组\n10001\n10003\n0\n1\n12\n13\n2\n0\n");
    require(out.find("子群号：2001")!=std::string::npos && p.groupCount()==19,"Create/list menu failed.");
    require(p.findGroup(ServiceTypeLJY::QQ,1001)->style()==GroupStyleLJY::QQ,"Cancelled switch committed.");
    out=run("9\n1001\n13\n2\n1\n12\n1\n2001\n2\n1\n1001\n13\n1\n1\n12\n");
    require(out.find("当前群管理模式：微信")!=std::string::npos && out.find("保留已有子群")!=std::string::npos &&
        out.find("子群号：2001")!=std::string::npos && p.groupCount()==19,"Mode/preservation menu missing.");
    require(p.findGroup(ServiceTypeLJY::QQ,1001)->style()==GroupStyleLJY::QQ,"Return to QQ failed.");
}
int main()
{
    int failed=0;
    auto test=[&](const char* name,void(*fn)()) {
        try{fn();std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
    };
    test("subgroup creation",testCreation);test("parent removal consistency",testRemoval);
    test("mode round trips and requests",testModes);test("subgroup and mode menus",testMenus);
    std::cout<<(4-failed)<<"/4 subgroup/mode scenario groups passed.\n";return failed?1:0;
}
