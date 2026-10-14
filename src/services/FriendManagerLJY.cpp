#include "services/FriendManagerLJY.h"

#include <cctype>
#include <stdexcept>

FriendManagerLJY::FriendManagerLJY(PlatformLJY& platform) : platform_(platform) {}

const ServiceAccountLJY& FriendManagerLJY::requireActor(int personId, int accountId) const
{
    const auto* account = platform_.findAccount(accountId);
    if (!platform_.findPerson(personId) || !account || account->personId() != personId)
        throw std::invalid_argument("不能操作不属于本人的服务账号。");
    return *account;
}

const ServiceAccountLJY& FriendManagerLJY::requireTarget(int personId, int accountId, int targetId) const
{
    const auto& actor = requireActor(personId, accountId);
    const auto* target = platform_.findAccount(targetId);
    if (!target) throw std::invalid_argument("目标账号不存在。");
    if (accountId == targetId) throw std::invalid_argument("不能将自己添加为好友。");
    if (actor.serviceType() != target->serviceType())
        throw std::invalid_argument("只能操作当前服务中的好友关系。");
    return *target;
}

const FriendRelationLJY& FriendManagerLJY::friendRelation(int personId, int accountId, int targetId) const
{
    requireTarget(personId, accountId, targetId);
    const auto* relation = platform_.findFriendRelation(accountId, targetId);
    if (!relation) throw std::invalid_argument("该账号不是当前服务中的好友。");
    return *relation;
}

void FriendManagerLJY::addFriend(int actorPersonId, int actorAccountId, int targetAccountId)
{
    requireTarget(actorPersonId, actorAccountId, targetAccountId);
    createRelation(actorAccountId, targetAccountId);
}

void FriendManagerLJY::createRelation(int accountId, int targetId)
{
    FriendRelationLJY relation(accountId, targetId);
    const auto key = std::make_pair(relation.firstAccountId(), relation.secondAccountId());
    // 双向关系只提交一次，避免仅添加一方成功的中间状态。
    //! emplace 返回 pair<iterator,bool>，其中.first 是迭代器，.second 是是否插入成功的标志
    if (!platform_.friendRelations_.emplace(key, relation).second)
        throw std::invalid_argument("双方已经是好友。");
}

void FriendManagerLJY::removeFriend(int actorPersonId, int actorAccountId, int targetAccountId)
{
    const auto& relation = friendRelation(actorPersonId, actorAccountId, targetAccountId);
    const auto key = std::make_pair(relation.firstAccountId(), relation.secondAccountId());
    platform_.friendRelations_.erase(key);
}

void FriendManagerLJY::updateRemark(int actorPersonId, int actorAccountId, int targetAccountId, const std::string& remark)
{
    const auto& relation = friendRelation(actorPersonId, actorAccountId, targetAccountId);
    if (remark.size() > 120) throw std::invalid_argument("备注不能超过 120 字节。");
    bool hasText = false;
    for (unsigned char ch : remark)
    {
        if (std::iscntrl(ch)) throw std::invalid_argument("备注不能含控制字符。");
        if (!std::isspace(ch)) hasText = true;
    }
    if (!remark.empty() && !hasText) throw std::invalid_argument("备注不能全为空白；留空可清除备注。");
    const auto key = std::make_pair(relation.firstAccountId(), relation.secondAccountId());
    platform_.friendRelations_.at(key).setRemark(actorAccountId, remark);
}

std::vector<ServiceAccountLJY> FriendManagerLJY::friends(int actorPersonId, int actorAccountId) const
{
    requireActor(actorPersonId, actorAccountId);
    std::vector<ServiceAccountLJY> result;
    for (const auto* relation : platform_.friendRelationsForAccount(actorAccountId))
        result.push_back(*platform_.findAccount(relation->otherAccountId(actorAccountId)));
    return result;
}

//TODO:无需成为好友便可查询是否合理

std::vector<ServiceAccountLJY> FriendManagerLJY::commonFriends(
    int personId, int accountId, int otherAccountId) const
{
    requireTarget(personId, accountId, otherAccountId);
    std::vector<ServiceAccountLJY> result;
    for (const auto* relation : platform_.friendRelationsForAccount(accountId))
    {
        const int peerId = relation->otherAccountId(accountId);
        if (platform_.findFriendRelation(otherAccountId, peerId))
            result.push_back(*platform_.findAccount(peerId));
    }
    return result;
}

const ServiceAccountLJY* FriendManagerLJY::mappedAccount(const ServiceAccountLJY& source,
    ServiceTypeLJY targetType, std::string& reason) const
{
    reason.clear();
    const auto* target = platform_.findAccountForPerson(source.personId(), targetType);
    if (!target) { reason = "未开通目标服务。"; return nullptr; }
    if (source.serviceType() == ServiceTypeLJY::WeChat || targetType == ServiceTypeLJY::WeChat)
    {
        const auto& wx = source.serviceType() == ServiceTypeLJY::WeChat ? source : *target;
        const auto& other = source.serviceType() == ServiceTypeLJY::WeChat ? *target : source;
        const auto* qq = platform_.boundQQForWeChat(wx.accountId());
        if (!qq || qq->serviceType() != ServiceTypeLJY::QQ || qq->personId() != wx.personId())
        { reason = "缺少有效的微信与 QQ 绑定。"; return nullptr; }
        if ((other.serviceType() == ServiceTypeLJY::QQ && qq->accountId() != other.accountId()) ||
            (other.serviceType() == ServiceTypeLJY::Weibo && qq->externalId() != other.externalId()))
        { reason = "绑定的 QQ 与目标映射不一致。"; return nullptr; }
    }
    else if (source.externalId() != target->externalId())
    { reason = "QQ 与微博的共享号码不一致。"; return nullptr; }
    return target;
}

//TODO: 较为晦涩，之后详细阅读

const ServiceAccountLJY& FriendManagerLJY::requireSource(
    const ServiceAccountLJY& target, ServiceTypeLJY sourceType) const
{
    if (!platform_.findService(sourceType)) throw std::invalid_argument("未知的源服务类型。");
    if (sourceType == target.serviceType()) throw std::invalid_argument("请选择与当前服务不同的源服务。");
    const auto* source = platform_.findAccountForPerson(target.personId(), sourceType);
    if (!source) throw std::invalid_argument("本人尚未开通所选源服务。");
    std::string reason;
    if (!mappedAccount(*source, target.serviceType(), reason))
        throw std::invalid_argument("本人账号无法关联：" + reason);
    return *source;
}

FriendQueryResultLJY FriendManagerLJY::queryAcrossServices(int personId, int targetAccountId,
    ServiceTypeLJY sourceType, bool commonOnly) const
{
    const auto& target = requireActor(personId, targetAccountId);
    const auto& source = requireSource(target, sourceType);
    std::string reason;
    std::vector<ServiceAccountLJY> result;
    std::vector<std::string> skipped;
    std::set<int> seenPeople;
    for (const auto* relation : platform_.friendRelationsForAccount(source.accountId()))
    {
        const auto& peer = *platform_.findAccount(relation->otherAccountId(source.accountId()));
        const auto* mapped = mappedAccount(peer, target.serviceType(), reason);
        if (mapped)
        {
            const bool alreadyFriend = platform_.findFriendRelation(targetAccountId, mapped->accountId()) != nullptr;
            if (mapped->personId() == personId) reason = "不能推荐本人。";
            else if (alreadyFriend != commonOnly)
                reason = commonOnly ? "尚未成为当前服务好友。" : "已经是当前服务好友。";
            else if (!seenPeople.insert(mapped->personId()).second) reason = "同一人员已列出。";
            else { result.push_back(*mapped); continue; }
        }
        skipped.push_back("源账号 " + peer.externalId() + "（" + peer.nickname() + "）：" + reason);
    }
    return FriendQueryResultLJY(result, skipped);
}

FriendQueryResultLJY FriendManagerLJY::crossServiceCommonFriends(int personId, int targetAccountId,
    ServiceTypeLJY sourceType) const
{
    return queryAcrossServices(personId, targetAccountId, sourceType, true);
}

FriendQueryResultLJY FriendManagerLJY::friendRecommendations(int personId, int targetAccountId,
    ServiceTypeLJY sourceType) const
{
    return queryAcrossServices(personId, targetAccountId, sourceType, false);
}

void FriendManagerLJY::addRecommendedFriend(int personId, int targetAccountId,
    ServiceTypeLJY sourceType, int recommendedAccountId)
{
    const auto& peer = requireTarget(personId, targetAccountId, recommendedAccountId);
    const auto& target = *platform_.findAccount(targetAccountId);
    const auto& source = requireSource(target, sourceType);
    const auto* sourcePeer = platform_.findAccountForPerson(peer.personId(), sourceType);
    if (!sourcePeer || !platform_.findFriendRelation(source.accountId(), sourcePeer->accountId()))
        throw std::invalid_argument("该账号已不符合推荐条件，请重新查询推荐列表。");

    std::string reason;
    const auto* mapped = mappedAccount(*sourcePeer, target.serviceType(), reason);
    if (!mapped || mapped->accountId() != recommendedAccountId)
        throw std::invalid_argument("无法推荐该账号：" + reason);
    createRelation(targetAccountId, recommendedAccountId);
}

void FriendManagerLJY::sendRequest(int personId, int accountId, int targetId)
{
    requireTarget(personId, accountId, targetId);
    if (platform_.findFriendRelation(accountId, targetId)) throw std::invalid_argument("双方已经是好友。");
    const auto key = std::make_pair(std::min(accountId,targetId), std::max(accountId,targetId));
    const auto found = platform_.friendRequests_.find(key);
    if (found != platform_.friendRequests_.end() && found->second.state() == 0)
        throw std::invalid_argument(found->second.senderId() == accountId ? "申请已发送，请等待对方处理。" : "对方已向你发送申请，请到好友申请中处理。");
    platform_.friendRequests_.insert_or_assign(key, FriendRequestLJY(accountId,targetId));
}
void FriendManagerLJY::handleRequest(int personId, int accountId, int senderId, bool accept)
{
    requireTarget(personId, accountId, senderId);
    const auto key = std::make_pair(std::min(accountId,senderId), std::max(accountId,senderId));
    const auto found = platform_.friendRequests_.find(key);
    if (found == platform_.friendRequests_.end() || found->second.recipientId() != accountId)
        throw std::invalid_argument("只有收到申请的账号可以处理。");
    auto& request = found->second;
    if (request.state() != 0) throw std::invalid_argument("该申请已经处理。");
    if (accept) createRelation(accountId, senderId);
    request.state_ = accept ? 1 : 2;
}
std::vector<FriendRequestLJY> FriendManagerLJY::requests(int personId, int accountId) const
{
    requireActor(personId, accountId);
    std::vector<FriendRequestLJY> result;
    for (const auto& entry : platform_.friendRequests_)
        if (entry.second.senderId()==accountId || entry.second.recipientId()==accountId) result.push_back(entry.second);
    return result;
}
std::vector<ServiceAccountLJY> FriendManagerLJY::cityCandidates(int personId, int accountId) const
{
    const auto& actor = requireActor(personId,accountId);
    const auto& city = platform_.findPerson(personId)->location();
    std::vector<ServiceAccountLJY> result;
    for (const auto& entry : platform_.accounts_)
    {
        const auto& peer = entry.second;
        if (peer.accountId()==accountId || peer.serviceType()!=actor.serviceType() ||
            platform_.findPerson(peer.personId())->location()!=city || platform_.findFriendRelation(accountId,peer.accountId())) continue;
        const auto key=std::make_pair(std::min(accountId,peer.accountId()),std::max(accountId,peer.accountId()));
        const auto request=platform_.friendRequests_.find(key);
        if (request!=platform_.friendRequests_.end() && request->second.state()==0) continue;
        result.push_back(peer);
    }
    return result;
}
