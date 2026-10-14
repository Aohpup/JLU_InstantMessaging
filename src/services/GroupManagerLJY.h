#pragma once

#include "services/PlatformLJY.h"
#include "services/GroupOperationLJY.h"
#include "services/QQGroupPolicyLJY.h"
#include "services/WeChatGroupPolicyLJY.h"

// 负责群查询、入群请求和成员管理；菜单只传入操作，规则由策略判断。
class GroupManagerLJY
{
public:
    // 平台生命周期须长于本对象。
    explicit GroupManagerLJY(PlatformLJY& platform);
    // 创建群并返回内部编号；群主自动入群，初始成员用于演示建群，不是开放加群入口。
    int createGroup(int personId, int ownerAccountId, int number, const std::string& name,
                    const std::vector<GroupMemberLJY>& initialMembers = {});
    // 查询本人该服务的群；返回平台管理的只读指针。
    std::vector<const GroupLJY*> groups(int personId, int accountId) const;
    // 按当前账号服务和群号查群，只有成员能读取群资料及成员名单。
    const GroupLJY& memberGroup(int personId, int accountId, int number) const;
    // 根据模式返回同一基类接口，策略对象由本管理器持有。
    const GroupPolicyLJY& policy(GroupStyleLJY style) const;

    // 提前检查当前群操作；targetId 为 0 时只检查执行者，目标输入后再检查目标。
    void checkOperation(int personId, int accountId, int number, GroupOperationLJY operation, int targetId = 0) const;
    // 检查请求存在、所属群、处理权限和状态；number 为 0 时不限定群号。
    void checkRequest(int personId, int accountId, int requestId, int number = 0) const;

    // 申请加入当前服务的 QQ 风格群，返回请求编号；不立即入群。
    int apply(int personId, int accountId, int number);
    // 群成员邀请同服务非成员，返回请求编号；待对方确认后入群。
    int invite(int personId, int accountId, int number, int targetId);
    // 返回本人相关或当前有审批权的请求，包含处理过的记录。
    std::vector<GroupRequestLJY> requests(int personId, int accountId) const;
    // 申请由当前群主/管理员处理，邀请仅由收件人处理；同意前检查最新资格。
    void handleRequest(int personId, int accountId, int requestId, bool accept);
    // 成员退出；群主必须先转让。离群会使本人发出的待处理邀请失效。
    void leave(int personId, int accountId, int number);
    // 按当前策略踢人，禁止移除自己；群主也须先取消目标管理员身份。
    void kick(int personId, int accountId, int number, int targetId);
    // 仅 QQ 风格群主可任免管理员，目标须为非群主成员。
    void setAdministrator(int personId, int accountId, int number, int targetId, bool enabled);
    // 转让给同群其他成员，新旧群主的管理员标记清除，原群主成为普通成员。
    void transferOwner(int personId, int accountId, int number, int targetId);

    // 父群群主建立一级子群；新群主和初始成员均须为父群成员，返回内部群编号。
    int createSubgroup(int personId, int accountId, int parentNumber, int number,
        const std::string& name, int ownerId, const std::vector<int>& memberIds);
    // 父群成员可查看子群列表；子群成员详情仍走 memberGroup。
    std::vector<const GroupLJY*> subgroups(int personId, int accountId, int parentNumber) const;
    // 仅群主可切换模式；保留本群资料及已有子群、成员和请求，子群继续使用自身规则。
    void switchMode(int personId, int accountId, int number, GroupStyleLJY style);

private:
    // 移除父群成员前一次检查所有子群，担任任一子群群主时拒绝整项操作。
    void checkRemoval(const GroupLJY& group, int accountId) const;
    // 子群入群时须仍属于父群；顶级群不额外限制。
    void requireParentMember(const GroupLJY& group, int accountId) const;
    // 输入预检查和实际提交复用，避免菜单另写一套权限规则。
    void checkOperation(const GroupLJY& group, int accountId, GroupOperationLJY operation, int targetId) const;
    // 检查本人账号并按其服务查群，不要求已入群，供申请和群操作使用。
    GroupLJY& requireGroup(int personId, int accountId, int number);
    // 检查通过后保存请求；重复请求由 checkOperation 在确认与提交前检查。
    int saveRequest(const GroupLJY& group, GroupRequestTypeLJY type,
                    const std::string& sender, const std::string& target);
    // 移除成员并使其发出的邀请失效，退群与踢人共用。
    void removeMember(GroupLJY& group, int accountId);
    PlatformLJY& platform_;
    QQGroupPolicyLJY qqPolicy_;
    WeChatGroupPolicyLJY weChatPolicy_;
    // 检查执行者确实拥有该服务账号。
    const ServiceAccountLJY& requireAccount(int personId, int accountId) const;
};
