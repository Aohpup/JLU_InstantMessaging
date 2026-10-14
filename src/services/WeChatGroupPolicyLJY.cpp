#include "services/WeChatGroupPolicyLJY.h"

bool WeChatGroupPolicyLJY::canApply(const GroupLJY&, int) const { return false; }
bool WeChatGroupPolicyLJY::canInvite(const GroupLJY& group, int actorId) const
{ return group.findMember(actorId) != nullptr; }
bool WeChatGroupPolicyLJY::canApprove(const GroupLJY&, int) const { return false; }
bool WeChatGroupPolicyLJY::canKick(const GroupLJY& group, int actorId, int targetId) const
{
    return actorId == group.ownerAccountId() && actorId != targetId && group.findMember(targetId);
}
bool WeChatGroupPolicyLJY::canSetAdministrator(const GroupLJY&, int) const { return false; }
bool WeChatGroupPolicyLJY::canCreateSubgroup(const GroupLJY&, int) const { return false; }
