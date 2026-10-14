#pragma once

#include "domain/GroupLJY.h"

// 策略只判断群内规则；账号存在和服务归属由群业务检查。
class GroupPolicyLJY
{
public:
    // 支持通过基类接口使用两种策略。
    virtual ~GroupPolicyLJY();
    // 非成员能否主动申请；此函数只判断，申请记录由群业务创建。
    virtual bool canApply(const GroupLJY& group, int accountId) const = 0;
    // 当前成员能否邀请他人，以及能否审批入群申请。
    virtual bool canInvite(const GroupLJY& group, int actorId) const = 0;
    virtual bool canApprove(const GroupLJY& group, int actorId) const = 0;
    // 判断踢人权限，拒绝踢自己、群主或不存在的成员。
    virtual bool canKick(const GroupLJY& group, int actorId, int targetId) const = 0;
    // 判断任免管理员及创建子群的权限。
    virtual bool canSetAdministrator(const GroupLJY& group, int actorId) const = 0;
    virtual bool canCreateSubgroup(const GroupLJY& group, int actorId) const = 0;
};
