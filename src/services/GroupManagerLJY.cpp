#include "services/GroupManagerLJY.h"
#include <stdexcept>
#include <algorithm>

GroupManagerLJY::GroupManagerLJY(PlatformLJY& platform) : platform_(platform) {}
const ServiceAccountLJY& GroupManagerLJY::requireAccount(int personId, int accountId) const
{
    const auto* account = platform_.findAccount(accountId);
    if (!account || account->personId() != personId)
        throw std::invalid_argument("不能操作不属于本人的服务账号。");
    return *account;
}
const GroupPolicyLJY& GroupManagerLJY::policy(GroupStyleLJY style) const
{
    if (style == GroupStyleLJY::QQ) return qqPolicy_;
    if (style == GroupStyleLJY::WeChat) return weChatPolicy_;
    throw std::invalid_argument("未知的群管理模式。");
}
int GroupManagerLJY::createGroup(int personId, int ownerAccountId, int number,
    const std::string& name, const std::vector<GroupMemberLJY>& initialMembers)
{
    const auto& owner = requireAccount(personId, ownerAccountId);
    if (platform_.findGroup(owner.serviceType(), number)) throw std::invalid_argument("当前服务中群号已存在。");
    const auto style = platform_.findService(owner.serviceType())->defaultGroupStyle();
    const int id = platform_.lastGroupId_ + 1;
    GroupLJY group(id, owner.serviceType(), number, name, ownerAccountId, style);
    for (const auto& member : initialMembers)
    {
        const auto* account = platform_.findAccount(member.accountId());
        if (!account || account->serviceType() != owner.serviceType())
            throw std::invalid_argument("群成员必须开通群所属服务。");
        if (group.findMember(member.accountId())) throw std::invalid_argument("群成员重复，群主不需再次加入。");
        if (member.isAdministrator() && !policy(style).canSetAdministrator(group, ownerAccountId))
            throw std::invalid_argument("当前群模式不允许设置管理员。");
        group.members_.push_back(member);
    }
    // 先建好完整群再保存，输入错误不会留下半个群。
    platform_.groups_.emplace(id, group);
    platform_.lastGroupId_ = id;
    return id;
}
std::vector<const GroupLJY*> GroupManagerLJY::groups(int personId, int accountId) const
{
    requireAccount(personId, accountId);
    std::vector<const GroupLJY*> result;
    for (const auto& entry : platform_.groups_)
        if (entry.second.findMember(accountId)) result.push_back(&entry.second);
    return result;
}
const GroupLJY& GroupManagerLJY::memberGroup(int personId, int accountId, int number) const
{
    const auto& account = requireAccount(personId, accountId);
    const auto* group = platform_.findGroup(account.serviceType(), number);
    if (!group) throw std::invalid_argument("当前服务中不存在该群号。");
    if (!group->findMember(accountId)) throw std::invalid_argument("当前账号不是该群成员，不能查看群资料和成员。");
    return *group;
}


GroupLJY& GroupManagerLJY::requireGroup(int personId, int accountId, int number)
{
    const auto& account = requireAccount(personId, accountId);
    const auto* group = platform_.findGroup(account.serviceType(), number);
    if (!group) throw std::invalid_argument("当前服务中不存在该群号。");
    return platform_.groups_.at(group->groupId());
}
void GroupManagerLJY::checkOperation(int personId, int accountId, int number,
    GroupOperationLJY operation, int targetId) const
{
    const auto& account = requireAccount(personId, accountId);
    const auto* group = platform_.findGroup(account.serviceType(), number);
    if (!group) throw std::invalid_argument("当前服务中不存在该群号。");
    checkOperation(*group, accountId, operation, targetId);
}

void GroupManagerLJY::checkOperation(const GroupLJY& group, int accountId,
    GroupOperationLJY operation, int targetId) const
{
    const auto* actor = group.findMember(accountId);
    const bool owner = accountId == group.ownerAccountId();
    const bool qq = group.style() == GroupStyleLJY::QQ;
    const auto& rules = policy(group.style());
    if (operation == GroupOperationLJY::Apply)
    {
        requireParentMember(group, accountId);
        if (actor) throw std::invalid_argument("本人已经是该群成员，无需申请。");
        if (!rules.canApply(group, accountId))
            throw std::invalid_argument("微信群不允许主动申请，请由群成员邀请。");
    }
    else
    {
        if (!actor) throw std::invalid_argument("本人尚未加入该群，不能执行此操作。");
        switch (operation)
        {
        case GroupOperationLJY::Invite:
            if (!rules.canInvite(group, accountId))
                throw std::invalid_argument("QQ 风格群不支持邀请，请对方申请后由群主或管理员审批。");
            break;
        case GroupOperationLJY::Leave:
            if (owner) throw std::invalid_argument("群主请先转让群主，再退出群。");
            checkRemoval(group, accountId);
            break;
        case GroupOperationLJY::Kick:
            if (!owner && (!qq || !actor->isAdministrator()))
                throw std::invalid_argument(qq ? "普通成员不可移除成员，需群主或管理员操作。"
                                                : "微信群仅群主可以移除成员，普通成员无此权限。");
            break;
        case GroupOperationLJY::SetAdministrator:
            if (!qq) throw std::invalid_argument("微信群不设管理员，不能设置或取消管理员。");
            if (!owner) throw std::invalid_argument(actor->isAdministrator()
                ? "管理员不可任免管理员，只有群主可以操作。" : "普通成员不可任免管理员，只有群主可以操作。");
            break;
        case GroupOperationLJY::TransferOwner:
            if (!owner) throw std::invalid_argument("只有群主可以转让群主身份。");
            break;
        case GroupOperationLJY::CreateSubgroup:
            if (group.parentGroupId() != 0) throw std::invalid_argument("只允许一级子群，不能在子群下继续建群。");
            if (!qq) throw std::invalid_argument("微信模式不能创建子群，请先切换到 QQ 模式。");
            if (!rules.canCreateSubgroup(group, accountId)) throw std::invalid_argument("只有父群群主可以创建子群。");
            break;
        case GroupOperationLJY::SwitchMode:
            if (!owner) throw std::invalid_argument("只有群主可以切换群管理模式。");
            break;
        default: throw std::invalid_argument("未知的群操作。");
        }
    }

    // 未输入目标时先完成执行者检查，有目标时立即检查号码及群内身份。
    if (targetId != 0)
    {
        const auto* target = platform_.findAccount(targetId);
        if (!target || target->serviceType() != group.serviceType())
            throw std::invalid_argument("目标账号不存在或不属于当前服务。");
        const auto* member = group.findMember(targetId);
        if (operation == GroupOperationLJY::Invite)
        {
            requireParentMember(group, targetId);
            if (member) throw std::invalid_argument("目标账号已经是该群成员，无需邀请。");
        }
        else
        {
            if (operation == GroupOperationLJY::Kick && targetId == accountId)
                throw std::invalid_argument("不可移除自己，请使用退出群；群主须先转让。");
            if (!member) throw std::invalid_argument("目标账号不是该群成员。");
            if (operation == GroupOperationLJY::Kick)
            {
                if (targetId == group.ownerAccountId())
                    throw std::invalid_argument("管理员不可移除群主。");
                if (qq && member->isAdministrator())
                    throw std::invalid_argument(owner ? "该成员是管理员，请先取消其管理员身份再移除。"
                                                      : "管理员不可移除管理员，请由群主先取消其管理员身份。");
                if (!rules.canKick(group, accountId, targetId))
                    throw std::invalid_argument("当前身份不具备移除该成员的权限。");
                checkRemoval(group, targetId);
            }
            if (operation == GroupOperationLJY::SetAdministrator && targetId == group.ownerAccountId())
                throw std::invalid_argument("群主身份不能通过设置或取消管理员修改。");
            if (operation == GroupOperationLJY::TransferOwner && targetId == accountId)
                throw std::invalid_argument("不能将群主转让给自己，请选择其他成员。");
        }
    }

    // 待处理记录也在确认前检查；未提供邀请目标时暂不检查重复邀请。
    if (operation == GroupOperationLJY::Apply || (operation == GroupOperationLJY::Invite && targetId != 0))
    {
        const auto type = operation == GroupOperationLJY::Apply ? GroupRequestTypeLJY::Application : GroupRequestTypeLJY::Invitation;
        const auto& target = platform_.findAccount(operation == GroupOperationLJY::Apply ? accountId : targetId)->externalId();
        for (const auto& entry : platform_.groupRequests_)
        {
            const auto& request = entry.second;
            if (request.serviceType() == group.serviceType() && request.groupNumber() == group.number() &&
                request.type() == type && request.targetNumber() == target && request.state() == GroupRequestStateLJY::Pending)
                throw std::invalid_argument("该账号已有同群待处理申请或邀请。");
        }
    }
}

void GroupManagerLJY::checkRequest(int personId, int accountId, int requestId, int number) const
{
    const auto& account = requireAccount(personId, accountId);
    const auto it = platform_.groupRequests_.find(requestId);
    if (it == platform_.groupRequests_.end() || it->second.serviceType() != account.serviceType() ||
        (number != 0 && it->second.groupNumber() != number))
        throw std::invalid_argument("当前群中不存在该请求编号。");
    const auto& request = it->second;
    const auto* group = platform_.findGroup(request.serviceType(), request.groupNumber());
    if (!group) throw std::invalid_argument("请求对应的群不存在。");
    if (request.type() == GroupRequestTypeLJY::Application)
    {
        if (!policy(group->style()).canApprove(*group, accountId))
            throw std::invalid_argument("只有 QQ 风格群的群主或管理员可以审批入群申请。");
    }
    else if (request.targetNumber() != account.externalId())
        throw std::invalid_argument("只有被邀请人本人可以接受或拒绝邀请。");
    if (request.state() != GroupRequestStateLJY::Pending)
        throw std::invalid_argument("该请求已处理或已失效。");
}

void GroupManagerLJY::requireParentMember(const GroupLJY& group, int accountId) const
{
    if (group.parentGroupId() == 0) return;
    const auto* parent = platform_.findGroup(group.parentGroupId());
    if (!parent || !parent->findMember(accountId))
        throw std::invalid_argument("加入子群前必须先加入其父群。");
}
void GroupManagerLJY::checkRemoval(const GroupLJY& group, int accountId) const
{
    for (const auto& entry : platform_.groups_)
        if (entry.second.parentGroupId() == group.groupId() && entry.second.ownerAccountId() == accountId)
            throw std::invalid_argument("该成员仍是子群 " + std::to_string(entry.second.number()) +
                " 的群主，请先转让子群群主。");
}
int GroupManagerLJY::createSubgroup(int personId, int accountId, int parentNumber, int number,
    const std::string& name, int ownerId, const std::vector<int>& memberIds)
{
    const auto& parent = requireGroup(personId, accountId, parentNumber);
    checkOperation(parent, accountId, GroupOperationLJY::CreateSubgroup, 0);
    if (platform_.findGroup(parent.serviceType(), number))
        throw std::invalid_argument("当前服务中群号已存在。");
    if (!parent.findMember(ownerId)) throw std::invalid_argument("子群群主必须是父群成员。");
    const int id = platform_.lastGroupId_ + 1;
    GroupLJY child(id, parent.serviceType(), number, name, ownerId, GroupStyleLJY::QQ);
    child.parentGroupId_ = parent.groupId();
    for (int memberId : memberIds)
    {
        if (!parent.findMember(memberId)) throw std::invalid_argument("子群成员必须是父群成员。");
        if (child.findMember(memberId)) throw std::invalid_argument("子群成员重复，子群群主已自动加入。");
        child.members_.emplace_back(memberId);
    }
    platform_.groups_.emplace(id, child);
    platform_.lastGroupId_ = id;
    return id;
}
std::vector<const GroupLJY*> GroupManagerLJY::subgroups(int personId, int accountId, int parentNumber) const
{
    const auto& parent = memberGroup(personId, accountId, parentNumber);
    std::vector<const GroupLJY*> result;
    for (const auto& entry : platform_.groups_)
        if (entry.second.parentGroupId() == parent.groupId()) result.push_back(&entry.second);
    return result;
}
void GroupManagerLJY::switchMode(int personId, int accountId, int number, GroupStyleLJY style)
{
    auto& group = requireGroup(personId, accountId, number);
    checkOperation(group, accountId, GroupOperationLJY::SwitchMode, 0);
    policy(style); // 仅切换本群规则，已有子群、成员和请求保持原样。
    group.style_ = style;
}

int GroupManagerLJY::saveRequest(const GroupLJY& group, GroupRequestTypeLJY type,
    const std::string& sender, const std::string& target)
{
    const int id = platform_.lastGroupRequestId_ + 1;
    platform_.groupRequests_.emplace(id, GroupRequestLJY(id, group.serviceType(), group.number(), type, sender, target));
    platform_.lastGroupRequestId_ = id;
    return id;
}
int GroupManagerLJY::apply(int personId, int accountId, int number)
{
    auto& group = requireGroup(personId, accountId, number);
    checkOperation(group, accountId, GroupOperationLJY::Apply, 0);
    const auto& account = *platform_.findAccount(accountId);
    return saveRequest(group, GroupRequestTypeLJY::Application, account.externalId(), account.externalId());
}
int GroupManagerLJY::invite(int personId, int accountId, int number, int targetId)
{
    auto& group = requireGroup(personId, accountId, number);
    if (targetId <= 0) throw std::invalid_argument("目标账号不存在。");
    checkOperation(group, accountId, GroupOperationLJY::Invite, targetId);
    const auto* target = platform_.findAccount(targetId);
    return saveRequest(group, GroupRequestTypeLJY::Invitation,
        platform_.findAccount(accountId)->externalId(), target->externalId());
}
std::vector<GroupRequestLJY> GroupManagerLJY::requests(int personId, int accountId) const
{
    const auto& account = requireAccount(personId, accountId);
    std::vector<GroupRequestLJY> result;
    for (const auto& entry : platform_.groupRequests_)
    {
        const auto& request = entry.second;
        if (request.serviceType() != account.serviceType()) continue;
        const auto* group = platform_.findGroup(request.serviceType(), request.groupNumber());
        const bool related = request.senderNumber() == account.externalId() || request.targetNumber() == account.externalId();
        const bool approver = request.type() == GroupRequestTypeLJY::Application && group &&
                              (group->ownerAccountId() == accountId || policy(group->style()).canApprove(*group, accountId));
        if (related || approver) result.push_back(request);
    }
    return result;
}
void GroupManagerLJY::handleRequest(int personId, int accountId, int requestId, bool accept)
{
    checkRequest(personId, accountId, requestId);
    auto& request = platform_.groupRequests_.at(requestId);
    const auto* found = platform_.findGroup(request.serviceType(), request.groupNumber());
    auto& group = platform_.groups_.at(found->groupId());
    const auto& rules = policy(group.style());
    const bool application = request.type() == GroupRequestTypeLJY::Application;
    if (!accept) { request.state_ = GroupRequestStateLJY::Rejected; return; }
    const auto* target = platform_.findAccount(request.serviceType(),request.targetNumber());
    if (!target || group.findMember(target->accountId()))
        throw std::invalid_argument("目标账号不存在或已经在群内。");
    if (application)
    {
        if (!rules.canApply(group,target->accountId())) throw std::invalid_argument("当前群模式不接受该申请。");
    }
    else
    {
        const auto* sender = platform_.findAccount(request.serviceType(),request.senderNumber());
        if (!sender || !rules.canInvite(group,sender->accountId()))
            throw std::invalid_argument("邀请人已无邀请资格。");
    }
    requireParentMember(group, target->accountId());
    group.members_.emplace_back(target->accountId());
    request.state_ = GroupRequestStateLJY::Accepted;
    // 同一对象入群后，其他待处理请求不再有效。
    for (auto& entry : platform_.groupRequests_)
    {
        auto& other = entry.second;
        if (other.serviceType() == request.serviceType() && other.groupNumber() == request.groupNumber() &&
            other.targetNumber() == request.targetNumber() && other.state() == GroupRequestStateLJY::Pending)
            other.state_ = GroupRequestStateLJY::Cancelled;
    }
}
void GroupManagerLJY::removeMember(GroupLJY& group, int accountId)
{
    const auto& number = platform_.findAccount(accountId)->externalId();
    group.members_.erase(std::remove_if(group.members_.begin(),group.members_.end(),
        [accountId](const GroupMemberLJY& member) { return member.accountId() == accountId; }),group.members_.end());
    for (auto& entry : platform_.groupRequests_)
    {
        auto& request = entry.second;
        if (request.serviceType() == group.serviceType() && request.groupNumber() == group.number() &&
            (request.targetNumber() == number ||
             (request.type() == GroupRequestTypeLJY::Invitation && request.senderNumber() == number)) &&
            request.state() == GroupRequestStateLJY::Pending)
            request.state_ = GroupRequestStateLJY::Cancelled;
    }
    // 子群只有一级；预检查已排除子群群主，此处统一移除其子群成员资格。
    for (auto& entry : platform_.groups_)
        if (entry.second.parentGroupId() == group.groupId()) removeMember(entry.second, accountId);
}
void GroupManagerLJY::leave(int personId, int accountId, int number)
{
    auto& group = requireGroup(personId,accountId,number);
    checkOperation(group, accountId, GroupOperationLJY::Leave, 0);
    removeMember(group,accountId);
}
void GroupManagerLJY::kick(int personId, int accountId, int number, int targetId)
{
    auto& group = requireGroup(personId,accountId,number);
    if (targetId <= 0) throw std::invalid_argument("目标账号不存在。");
    checkOperation(group, accountId, GroupOperationLJY::Kick, targetId);
    removeMember(group,targetId);
}
void GroupManagerLJY::setAdministrator(int personId, int accountId, int number, int targetId, bool enabled)
{
    auto& group = requireGroup(personId,accountId,number);
    if (targetId <= 0) throw std::invalid_argument("目标账号不存在。");
    checkOperation(group, accountId, GroupOperationLJY::SetAdministrator, targetId);
    for (auto& member : group.members_)
        if (member.accountId() == targetId) { member.administrator_ = enabled; return; }
}
void GroupManagerLJY::transferOwner(int personId, int accountId, int number, int targetId)
{
    auto& group = requireGroup(personId,accountId,number);
    if (targetId <= 0) throw std::invalid_argument("目标账号不存在。");
    checkOperation(group, accountId, GroupOperationLJY::TransferOwner, targetId);
    for (auto& member : group.members_)
        if (member.accountId() == accountId || member.accountId() == targetId) member.administrator_ = false;
    group.ownerAccountId_ = targetId;
}
