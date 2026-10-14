#include "services/DemoDataLJY.h"
#include "services/AccountServiceLJY.h"
#include "services/FriendManagerLJY.h"
#include "services/GroupManagerLJY.h"
#include <stdexcept>

void loadDemoDataLJY(PlatformLJY& platform)
{
    if (platform.personCount() || platform.accountCount() || platform.groupCount() || platform.friendRelationCount())
        throw std::invalid_argument("演示数据只能加载到空平台，不能重复加载或覆盖已有数据。");
    AccountServiceLJY accounts(platform);
    FriendManagerLJY friends(platform);
    GroupManagerLJY groups(platform);
    const ServiceTypeLJY types[] = {ServiceTypeLJY::QQ, ServiceTypeLJY::WeChat, ServiceTypeLJY::Weibo};
    const char* labels[] = {"QQ", "微信", "微博"};
    const bool opened[6][3] = {{true,true,true}, {true,true,false}, {true,false,false},
                              {true,true,true}, {true,false,true}, {false,true,false}};
    int people[6]{};
    int ids[6][3]{};
    for (int i = 0; i < 6; ++i)
    {
        const std::string name = std::string("演示") + static_cast<char>('A' + i);
        for (int s = 0; s < 3; ++s)
        {
            if (!opened[i][s]) continue;
            if (people[i] == 0)
            {
                ids[i][s] = accounts.registerAccount(name, "2000-01-01", "长春",
                    "DemoOnly123!", types[s], name + labels[s]);
                people[i] = platform.findAccount(ids[i][s])->personId();
            }
            else ids[i][s] = accounts.openService(people[i], types[s], name + labels[s]);
        }
        if (ids[i][0] && ids[i][1]) accounts.bindWeChatToQQ(people[i], ids[i][1], ids[i][0]);
    }
    for (int peer : {1,2,3}) friends.addFriend(people[0], ids[0][0], ids[peer][0]);
    friends.addFriend(people[1], ids[1][0], ids[2][0]);
    friends.addFriend(people[0], ids[0][1], ids[3][1]);
    friends.addFriend(people[0], ids[0][2], ids[4][2]);
    for (int s = 0; s < 3; ++s)
    {
        std::vector<int> eligible;
        for (int i = 0; i < 6; ++i) if (ids[i][s]) eligible.push_back(i);
        for (int g = 0; g < 6; ++g)
        {
            const int ownerIndex = g % static_cast<int>(eligible.size());
            const int owner = eligible[ownerIndex];
            std::vector<GroupMemberLJY> members;
            bool administratorSet = false;
            for (int j = 0; j < static_cast<int>(eligible.size()); ++j)
            {
                if (j == ownerIndex) continue;
                if (g != 0 && j != 0 && j != (ownerIndex + 1) % static_cast<int>(eligible.size())) continue;
                const bool administrator = s != 1 && !administratorSet;
                members.emplace_back(ids[eligible[j]][s], administrator);
                if (administrator) administratorSet = true;
            }
            groups.createGroup(people[owner], ids[owner][s], 1001 + g,
                std::string(labels[s]) + "演示群" + std::to_string(1001 + g), members);
        }
    }
}

void showDemoAccountsLJY(const PlatformLJY& platform, std::ostream& output)
{
    output << "已加载课堂演示数据：6 名人员、18 个群。\n统一测试密码：DemoOnly123!\n";
    const ServiceTypeLJY types[] = {ServiceTypeLJY::QQ, ServiceTypeLJY::WeChat, ServiceTypeLJY::Weibo};
    const char* labels[] = {"QQ", "微信", "微博"};
    // 演示人员通过已加载账号收集，显示时不依赖连续的人员编号。
    std::set<int> people;
    for (auto type : types)
    {
        const auto* group = platform.findGroup(type, 1001);
        if (group) for (const auto& member : group->members())
            people.insert(platform.findAccount(member.accountId())->personId());
    }
    for (int id : people)
    {
        output << platform.findPerson(id)->name();
        for (int s = 0; s < 3; ++s)
        {
            const auto* account = platform.findAccountForPerson(id, types[s]);
            output << " | " << labels[s] << ": " << (account ? account->externalId() : "未开通");
        }
        output << '\n';
    }
    output << "A 已开通三项服务且在全部演示群中；登录后选 9 群管理。\n"
           << "使用 --demo 或 --empty 可分别进入演示或空平台内存测试模式；这两种模式退出后修改清空。\n";
}
