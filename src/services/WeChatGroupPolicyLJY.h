#pragma once

#include "services/GroupPolicyLJY.h"

// 微信由成员邀请，仅群主有管理特权，不设置管理员或子群。
class WeChatGroupPolicyLJY : public GroupPolicyLJY
{
public:
    // 查询当前策略是否允许申请、邀请或审批。
    bool canApply(const GroupLJY& group, int accountId) const override;
    bool canInvite(const GroupLJY& group, int actorId) const override;
    bool canApprove(const GroupLJY& group, int actorId) const override;
    // 按双方群内身份判断踢人权限。
    bool canKick(const GroupLJY& group, int actorId, int targetId) const override;
    // 判断管理员任免和子群创建权限，不修改任何群数据。
    bool canSetAdministrator(const GroupLJY& group, int actorId) const override;
    bool canCreateSubgroup(const GroupLJY& group, int actorId) const override;
};
