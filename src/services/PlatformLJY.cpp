#include "services/PlatformLJY.h"
#include "services/QQServiceLJY.h"
#include "services/WeChatServiceLJY.h"
#include "services/WeiboServiceLJY.h"

#include <algorithm>

PlatformLJY::PlatformLJY()
{
    services_.emplace(ServiceTypeLJY::QQ, std::make_unique<QQServiceLJY>());
    services_.emplace(ServiceTypeLJY::WeChat, std::make_unique<WeChatServiceLJY>());
    services_.emplace(ServiceTypeLJY::Weibo, std::make_unique<WeiboServiceLJY>());
}

const PersonLJY* PlatformLJY::findPerson(int personId) const
{
    const auto it = people_.find(personId);
    return it == people_.end() ? nullptr : &it->second;
}

const ServiceAccountLJY* PlatformLJY::findAccount(int accountId) const
{
    const auto it = accounts_.find(accountId);
    return it == accounts_.end() ? nullptr : &it->second;
}

const ServiceAccountLJY* PlatformLJY::findAccount(ServiceTypeLJY type, const std::string& externalId) const
{
    for (const auto& entry : accounts_)
        if (entry.second.serviceType() == type && entry.second.externalId() == externalId)
            return &entry.second;
    return nullptr;
}

const ServiceAccountLJY* PlatformLJY::findAccountForPerson(int personId, ServiceTypeLJY type) const
{
    for (const auto& entry : accounts_)
        if (entry.second.personId() == personId && entry.second.serviceType() == type)
            return &entry.second;
    return nullptr;
}

std::vector<const ServiceAccountLJY*> PlatformLJY::accountsForPerson(int personId) const
{
    std::vector<const ServiceAccountLJY*> result;
    for (const auto& entry : accounts_)
        if (entry.second.personId() == personId) result.push_back(&entry.second);
    return result;
}

const SocialServiceLJY* PlatformLJY::findService(ServiceTypeLJY type) const
{
    const auto it = services_.find(type);
    return it == services_.end() ? nullptr : it->second.get();
}

const ServiceAccountLJY* PlatformLJY::boundQQForWeChat(int weChatAccountId) const
{
    const auto it = weChatToQQ_.find(weChatAccountId);
    return it == weChatToQQ_.end() ? nullptr : findAccount(it->second);
}

int PlatformLJY::personCount() const noexcept { return static_cast<int>(people_.size()); }
int PlatformLJY::accountCount() const noexcept { return static_cast<int>(accounts_.size()); }

const FriendRelationLJY* PlatformLJY::findFriendRelation(int firstAccountId, int secondAccountId) const
{
    const auto key = std::make_pair(std::min(firstAccountId, secondAccountId),
                                    std::max(firstAccountId, secondAccountId));
    const auto it = friendRelations_.find(key);
    return it == friendRelations_.end() ? nullptr : &it->second;
}

std::vector<const FriendRelationLJY*> PlatformLJY::friendRelationsForAccount(int accountId) const
{
    std::vector<const FriendRelationLJY*> result;
    for (const auto& entry : friendRelations_)
        if (entry.second.firstAccountId() == accountId || entry.second.secondAccountId() == accountId)
            result.push_back(&entry.second);
    return result;
}

int PlatformLJY::friendRelationCount() const noexcept { return static_cast<int>(friendRelations_.size()); }

const GroupLJY* PlatformLJY::findGroup(int groupId) const
{
    const auto it = groups_.find(groupId);
    return it == groups_.end() ? nullptr : &it->second;
}
const GroupLJY* PlatformLJY::findGroup(ServiceTypeLJY service, int number) const
{
    for (const auto& entry : groups_)
        if (entry.second.serviceType() == service && entry.second.number() == number) return &entry.second;
    return nullptr;
}
int PlatformLJY::groupCount() const noexcept { return static_cast<int>(groups_.size()); }
