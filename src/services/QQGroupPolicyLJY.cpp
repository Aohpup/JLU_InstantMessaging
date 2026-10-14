#include "services/QQGroupPolicyLJY.h"

bool QQGroupPolicyLJY::canApply(const GroupLJY& group, int accountId) const
{ return accountId > 0 && !group.findMember(accountId); }
bool QQGroupPolicyLJY::canInvite(const GroupLJY&, int) const { return false; }
bool QQGroupPolicyLJY::canApprove(const GroupLJY& group, int actorId) const
{
    const auto* member = group.findMember(actorId);
    return member && (actorId == group.ownerAccountId() || member->isAdministrator());
}
bool QQGroupPolicyLJY::canKick(const GroupLJY& group, int actorId, int targetId) const
{
    const auto* actor = group.findMember(actorId);
    const auto* target = group.findMember(targetId);
    if (!actor || !target || actorId == targetId || targetId == group.ownerAccountId()) return false;
    if (target->isAdministrator()) return false; // 群主也须先取消目标管理员身份。
    return actorId == group.ownerAccountId() || actor->isAdministrator();
}
bool QQGroupPolicyLJY::canSetAdministrator(const GroupLJY& group, int actorId) const
{ return actorId == group.ownerAccountId(); }
bool QQGroupPolicyLJY::canCreateSubgroup(const GroupLJY& group, int actorId) const
{ return actorId == group.ownerAccountId() && group.parentGroupId() == 0; }
