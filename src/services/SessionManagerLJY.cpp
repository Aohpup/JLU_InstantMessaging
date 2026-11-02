#include "services/SessionManagerLJY.h"

#include <stdexcept>

SessionManagerLJY::SessionManagerLJY(PlatformLJY& platform, const std::string& referenceDate)
    : platform_(platform), accountService_(platform, referenceDate), friendManager_(platform), groupManager_(platform)
{
}

ServiceAccountLJY SessionManagerLJY::registerAccount(const std::string& name, const std::string& birthDate,
    const std::string& location, const std::string& password, ServiceTypeLJY type,
    const std::string& nickname)
{
    if (isLoggedIn()) throw std::invalid_argument("请先注销，再注册新的人员。");
    const int accountId = accountService_.registerAccount(name, birthDate, location, password, type, nickname);
    return *platform_.findAccount(accountId);
}

void SessionManagerLJY::login(ServiceTypeLJY type, const std::string& externalId,
                            const std::string& password)
{
    if (isLoggedIn()) throw std::invalid_argument("已有登录会话，请切换服务或先注销。");
    const int personId = accountService_.authenticate(type, externalId, password);
    const int accountId = platform_.findAccount(type, externalId)->accountId();
    // 身份验证和状态准备全部成功后再提交，失败不遗留部分登录状态。
    std::set<int> confirmed{accountId};
    platform_.passwordLoginHistory_.insert(accountId);
    loggedInAccounts_.swap(confirmed);
    currentPersonId_ = personId;
    currentAccountId_ = accountId;
}

bool SessionManagerLJY::enterService(ServiceTypeLJY type, bool confirmed)
{
    requireLogin();
    if (!platform_.findService(type)) throw std::invalid_argument("未知的服务类型。");
    const auto* account = platform_.findAccountForPerson(currentPersonId_, type);
    if (!account) throw std::invalid_argument("该服务尚未开通，请先开通服务。");
    if (account->accountId() == currentAccountId_)
        throw std::invalid_argument("当前已在该服务中，无需切换。");
    if (!confirmed) return false;
    loggedInAccounts_.insert(account->accountId());
    currentAccountId_ = account->accountId();
    return true;
}

void SessionManagerLJY::logout() noexcept
{
    loggedInAccounts_.clear();
    currentPersonId_ = 0;
    currentAccountId_ = 0;
}

bool SessionManagerLJY::isLoggedIn() const noexcept { return currentPersonId_ != 0; }

void SessionManagerLJY::requireLogin() const
{
    if (!isLoggedIn()) throw std::invalid_argument("请先登录。");
}

ServiceLoginStateLJY SessionManagerLJY::serviceState(ServiceTypeLJY type) const
{
    requireLogin();
    if (!platform_.findService(type)) throw std::invalid_argument("未知的服务类型。");
    const auto* account = platform_.findAccountForPerson(currentPersonId_, type);
    if (!account) return ServiceLoginStateLJY::NotOpened;
    return loggedInAccounts_.count(account->accountId()) != 0
        ? ServiceLoginStateLJY::LoggedIn : ServiceLoginStateLJY::PendingConfirmation;
}

PersonLJY SessionManagerLJY::currentPerson() const
{
    requireLogin();
    return *platform_.findPerson(currentPersonId_);
}

ServiceAccountLJY SessionManagerLJY::currentAccount() const
{
    requireLogin();
    return *platform_.findAccount(currentAccountId_);
}

std::vector<ServiceAccountLJY> SessionManagerLJY::accounts() const
{
    requireLogin();
    std::vector<ServiceAccountLJY> result;
    for (const auto* account : platform_.accountsForPerson(currentPersonId_))
        result.push_back(*account);
    return result;
}

int SessionManagerLJY::openService(ServiceTypeLJY type, const std::string& nickname)
{
    requireLogin();
    return accountService_.openService(currentPersonId_, type, nickname);
}

void SessionManagerLJY::validateBirthDate(const std::string& birthDate) const
{
    accountService_.validateBirthDate(birthDate, currentPersonId_);
}

void SessionManagerLJY::updateProfile(const std::string& name, const std::string& birthDate,
                                    const std::string& location, int avatarColor)
{
    requireLogin();
    accountService_.updateProfile(currentPersonId_, name, birthDate, location, avatarColor);
}

void SessionManagerLJY::updateNickname(const std::string& nickname)
{
    requireLogin();
    accountService_.updateNickname(currentPersonId_, currentAccountId_, nickname);
}

void SessionManagerLJY::bindWeChatToQQ()
{
    requireLogin();
    const auto* qq = platform_.findAccountForPerson(currentPersonId_, ServiceTypeLJY::QQ);
    const auto* wx = platform_.findAccountForPerson(currentPersonId_, ServiceTypeLJY::WeChat);
    if (!qq || !wx) throw std::invalid_argument("请先开通本人的 QQ 和微信，再进行绑定。");
    // 已绑定是账号现状，优先于首次绑定所需的登录条件。
    if (platform_.boundQQForWeChat(wx->accountId()))
        throw std::invalid_argument(currentAccount().serviceType() == ServiceTypeLJY::QQ
            ? "QQ 账号已绑定。" : "微信账号已绑定。");
    for (const auto& binding : platform_.weChatToQQ_)
        if (binding.second == qq->accountId()) throw std::invalid_argument("QQ 账号已绑定。");
    const bool bothCurrent = loggedInAccounts_.count(qq->accountId()) != 0 &&
                             loggedInAccounts_.count(wx->accountId()) != 0;
    const bool bothVerified = platform_.passwordLoginHistory_.count(qq->accountId()) != 0 &&
                              platform_.passwordLoginHistory_.count(wx->accountId()) != 0;
    if (!bothCurrent && !bothVerified)
        throw std::invalid_argument("请先分别用密码登录本人的 QQ 和微信，或在当前会话确认两个服务，再进行绑定。");
    accountService_.bindWeChatToQQ(currentPersonId_, wx->accountId(), qq->accountId());
}

void SessionManagerLJY::changePassword(const std::string& oldPassword, const std::string& newPassword)
{
    requireLogin();
    accountService_.changePassword(currentPersonId_, oldPassword, newPassword);
}

void SessionManagerLJY::unbindWeChatFromQQ()
{
    requireLogin();
    const auto* wx = platform_.findAccountForPerson(currentPersonId_, ServiceTypeLJY::WeChat);
    if (!wx) throw std::invalid_argument("本人尚未开通微信。");
    accountService_.unbindWeChatFromQQ(currentPersonId_, wx->accountId());
}

std::string SessionManagerLJY::boundQQNumber() const
{
    requireLogin();
    const auto* wx = platform_.findAccountForPerson(currentPersonId_, ServiceTypeLJY::WeChat);
    const auto* qq = wx ? platform_.boundQQForWeChat(wx->accountId()) : nullptr;
    return qq ? qq->externalId() : "";
}

int SessionManagerLJY::registrationAge() const
{
    requireLogin();
    return accountService_.registrationAge(currentAccountId_);
}

ServiceAccountLJY SessionManagerLJY::contactAccount(const std::string& externalId) const
{
    const auto current = currentAccount();
    const auto* target = platform_.findAccount(current.serviceType(), externalId);
    if (!target) throw std::invalid_argument("当前服务中不存在该号码。");
    return *target;
}

ServiceAccountLJY SessionManagerLJY::friendCandidate(const std::string& externalId) const
{
    const auto target = contactAccount(externalId);
    if (target.accountId() == currentAccountId_) throw std::invalid_argument("不能将自己添加为好友。");
    if (platform_.findFriendRelation(currentAccountId_, target.accountId()))
        throw std::invalid_argument("双方已经是好友。");
    return target;
}

void SessionManagerLJY::addFriend(const std::string& externalId)
{
    const auto target = contactAccount(externalId);
    friendManager_.addFriend(currentPersonId_, currentAccountId_, target.accountId());
}

void SessionManagerLJY::removeFriend(const std::string& externalId)
{
    const auto target = contactAccount(externalId);
    friendManager_.removeFriend(currentPersonId_, currentAccountId_, target.accountId());
}

void SessionManagerLJY::updateFriendRemark(const std::string& externalId, const std::string& remark)
{
    const auto target = contactAccount(externalId);
    friendManager_.updateRemark(currentPersonId_, currentAccountId_, target.accountId(), remark);
}

std::vector<ServiceAccountLJY> SessionManagerLJY::friends() const
{
    requireLogin();
    return friendManager_.friends(currentPersonId_, currentAccountId_);
}

ServiceAccountLJY SessionManagerLJY::friendAccount(const std::string& externalId) const
{
    const auto target = contactAccount(externalId);
    friendManager_.friendRelation(currentPersonId_, currentAccountId_, target.accountId());
    return target;
}

PersonLJY SessionManagerLJY::friendPerson(const std::string& externalId) const
{
    const auto target = friendAccount(externalId);
    return *platform_.findPerson(target.personId());
}

std::string SessionManagerLJY::friendRemark(const std::string& externalId) const
{
    const auto target = contactAccount(externalId);
    return friendManager_.friendRelation(currentPersonId_, currentAccountId_, target.accountId())
        .remarkFor(currentAccountId_);
}


std::vector<ServiceAccountLJY> SessionManagerLJY::commonFriends(const std::string& externalId) const
{
    const auto other = contactAccount(externalId);
    return friendManager_.commonFriends(currentPersonId_, currentAccountId_, other.accountId());
}

FriendQueryResultLJY SessionManagerLJY::crossServiceCommonFriends(ServiceTypeLJY sourceType) const
{
    requireLogin();
    return friendManager_.crossServiceCommonFriends(currentPersonId_, currentAccountId_, sourceType);
}

FriendQueryResultLJY SessionManagerLJY::friendRecommendations(ServiceTypeLJY sourceType) const
{
    requireLogin();
    return friendManager_.friendRecommendations(currentPersonId_, currentAccountId_, sourceType);
}

void SessionManagerLJY::addRecommendedFriend(ServiceTypeLJY sourceType, const std::string& externalId)
{
    const auto target = contactAccount(externalId);
    friendManager_.addRecommendedFriend(currentPersonId_, currentAccountId_, sourceType, target.accountId());
}

std::vector<const GroupLJY*> SessionManagerLJY::groups() const
{
    requireLogin();
    return groupManager_.groups(currentPersonId_, currentAccountId_);
}
const GroupLJY& SessionManagerLJY::selectedGroup(int number) const
{
    const auto account = currentAccount();
    const auto* group = platform_.findGroup(account.serviceType(), number);
    if (!group) throw std::invalid_argument("当前服务中不存在该群号。");
    return *group;
}
void SessionManagerLJY::checkGroupOperation(int number, GroupOperationLJY operation, const std::string& targetNumber) const
{
    requireLogin();
    const int target = targetNumber.empty() ? 0 : contactAccount(targetNumber).accountId();
    groupManager_.checkOperation(currentPersonId_, currentAccountId_, number, operation, target);
}
void SessionManagerLJY::checkGroupRequest(int number, int requestId) const
{
    requireLogin();
    groupManager_.checkRequest(currentPersonId_, currentAccountId_, requestId, number);
}
const GroupLJY& SessionManagerLJY::group(int number, std::vector<ServiceAccountLJY>& memberAccounts) const
{
    requireLogin();
    const auto& result = groupManager_.memberGroup(currentPersonId_, currentAccountId_, number);
    memberAccounts.clear();
    for (const auto& member : result.members())
        memberAccounts.push_back(*platform_.findAccount(member.accountId()));
    return result;
}

int SessionManagerLJY::applyToGroup(int number)
{
    requireLogin();
    return groupManager_.apply(currentPersonId_,currentAccountId_,number);
}
int SessionManagerLJY::inviteToGroup(int number,const std::string& targetNumber)
{
    const auto target = contactAccount(targetNumber);
    return groupManager_.invite(currentPersonId_,currentAccountId_,number,target.accountId());
}
std::vector<GroupRequestLJY> SessionManagerLJY::groupRequests(int number) const
{
    requireLogin();
    if (number != 0) selectedGroup(number);
    std::vector<GroupRequestLJY> result;
    for (const auto& request : groupManager_.requests(currentPersonId_, currentAccountId_))
        if (number == 0 || request.groupNumber() == number) result.push_back(request);
    return result;
}
void SessionManagerLJY::handleGroupRequest(int requestId,bool accept)
{
    requireLogin();
    groupManager_.handleRequest(currentPersonId_,currentAccountId_,requestId,accept);
}
void SessionManagerLJY::leaveGroup(int number)
{
    requireLogin();
    groupManager_.leave(currentPersonId_,currentAccountId_,number);
}
void SessionManagerLJY::kickGroupMember(int number,const std::string& targetNumber)
{
    const auto target = contactAccount(targetNumber);
    groupManager_.kick(currentPersonId_,currentAccountId_,number,target.accountId());
}
void SessionManagerLJY::setGroupAdministrator(int number,const std::string& targetNumber,bool enabled)
{
    const auto target = contactAccount(targetNumber);
    groupManager_.setAdministrator(currentPersonId_,currentAccountId_,number,target.accountId(),enabled);
}
void SessionManagerLJY::transferGroupOwner(int number,const std::string& targetNumber)
{
    const auto target = contactAccount(targetNumber);
    groupManager_.transferOwner(currentPersonId_,currentAccountId_,number,target.accountId());
}

int SessionManagerLJY::createSubgroup(int parentNumber, int number, const std::string& name,
    const std::string& ownerNumber, const std::vector<std::string>& memberNumbers)
{
    const auto owner = contactAccount(ownerNumber);
    std::vector<int> members;
    for (const auto& member : memberNumbers) members.push_back(contactAccount(member).accountId());
    return groupManager_.createSubgroup(currentPersonId_, currentAccountId_, parentNumber, number, name, owner.accountId(), members);
}
std::vector<const GroupLJY*> SessionManagerLJY::subgroups(int parentNumber) const
{
    requireLogin();
    return groupManager_.subgroups(currentPersonId_, currentAccountId_, parentNumber);
}
void SessionManagerLJY::switchGroupMode(int number, GroupStyleLJY style)
{
    requireLogin();
    groupManager_.switchMode(currentPersonId_, currentAccountId_, number, style);
}

void SessionManagerLJY::sendFriendRequest(const std::string& externalId)
{
    const auto target=contactAccount(externalId);
    friendManager_.sendRequest(currentPersonId_,currentAccountId_,target.accountId());
}
void SessionManagerLJY::handleFriendRequest(const std::string& senderNumber, bool accept)
{
    const auto sender=contactAccount(senderNumber);
    friendManager_.handleRequest(currentPersonId_,currentAccountId_,sender.accountId(),accept);
}
std::vector<FriendRequestLJY> SessionManagerLJY::friendRequests() const
{
    requireLogin(); return friendManager_.requests(currentPersonId_,currentAccountId_);
}
std::vector<ServiceAccountLJY> SessionManagerLJY::cityCandidates() const
{
    requireLogin(); return friendManager_.cityCandidates(currentPersonId_,currentAccountId_);
}

ServiceAccountLJY SessionManagerLJY::contactAccount(int accountId) const
{
    const auto current=currentAccount();
    const auto* target=platform_.findAccount(accountId);
    if (!target || target->serviceType()!=current.serviceType()) throw std::invalid_argument("当前服务中不存在该账号。");
    return *target;
}

int SessionManagerLJY::createGroup(const std::string& name)
{
    const auto account = currentAccount();
    int number = 1001;
    while (platform_.findGroup(account.serviceType(), number)) ++number;
    groupManager_.createGroup(currentPersonId_, currentAccountId_, number, name, {});
    return number;
}
