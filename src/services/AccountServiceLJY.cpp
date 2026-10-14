#include "services/AccountServiceLJY.h"

#include <cctype>
#include <limits>
#include <stdexcept>

AccountServiceLJY::AccountServiceLJY(PlatformLJY& platform, const std::string& referenceDate)
    : platform_(platform),
      referenceDate_(referenceDate.empty() ? DateLJY::today() : DateLJY(referenceDate))
{
}

void AccountServiceLJY::requireText(const std::string& text, const std::string& field)
{
    for (unsigned char ch : text)
        if (!std::isspace(ch)) return;
    throw std::invalid_argument(field + "不能为空或全为空白。");
}

int AccountServiceLJY::nextNumber(int lastNumber)
{
    if (lastNumber == std::numeric_limits<int>::max())
        throw std::overflow_error("可用编号已用尽。");
    return lastNumber + 1;
}

void AccountServiceLJY::validateProfile(const std::string& name, const std::string& birthDate,
                                       const std::string& location) const
{
    requireText(name, "姓名");
    requireText(location, "所在地");
    validateBirthDate(birthDate);
}

const PersonLJY& AccountServiceLJY::requirePerson(int personId) const
{
    const auto* person = platform_.findPerson(personId);
    if (!person) throw std::invalid_argument("人员不存在。");
    return *person;
}

const ServiceAccountLJY& AccountServiceLJY::requireOwnedAccount(int personId, int accountId) const
{
    requirePerson(personId);
    const auto* account = platform_.findAccount(accountId);
    if (!account) throw std::invalid_argument("账号不存在。");
    if (account->personId() != personId)
        throw std::invalid_argument("不能操作他人的账号。");
    return *account;
}

int AccountServiceLJY::createPerson(const std::string& name, const std::string& birthDate,
                                   const std::string& location)
{
    validateProfile(name, birthDate, location);
    const int id = nextNumber(platform_.lastPersonId_);
    const auto added = platform_.people_.emplace(id, PersonLJY(id, name, birthDate, location));
    if (!added.second) throw std::logic_error("人员编号已存在。");
    platform_.lastPersonId_ = id;
    return id;
}

int AccountServiceLJY::registerAccount(const std::string& name, const std::string& birthDate,
    const std::string& location, const std::string& password, ServiceTypeLJY type,
    const std::string& nickname)
{
    requireText(nickname, "昵称");
    if (!platform_.findService(type)) throw std::invalid_argument("未知的服务类型。");
    PasswordCredentialLJY credential(password);
    const int previousPersonId = platform_.lastPersonId_;
    const int personId = createPerson(name, birthDate, location);
    try
    {
        const auto inserted = platform_.credentials_.emplace(personId, credential);
        if (!inserted.second) throw std::logic_error("人员凭据重复。");
        return openService(personId, type, nickname);
    }
    catch (...)
    {
        // 首项服务未成功开通时撤回新人员和凭据，避免出现无法登录的半成品账号。
        platform_.credentials_.erase(personId);
        platform_.people_.erase(personId);
        platform_.lastPersonId_ = previousPersonId;
        throw;
    }
}

int AccountServiceLJY::authenticate(ServiceTypeLJY type, const std::string& externalId,
                                   const std::string& password) const
{
    const auto* account = platform_.findAccount(type, externalId);
    if (account)
    {
        const auto found = platform_.credentials_.find(account->personId());
        if (found != platform_.credentials_.end() && found->second.matches(password))
            return account->personId();
    }
    throw std::invalid_argument("账号或密码错误。");
}

void AccountServiceLJY::validateBirthDate(const std::string& birthDate, int personId) const
{
    const DateLJY birth(birthDate);
    if (referenceDate_ < birth)
        throw std::invalid_argument("生日不能晚于业务参考日期。");
    if (personId != 0)
    {
        requirePerson(personId);
        for (const auto* account : platform_.accountsForPerson(personId))
            if (DateLJY(account->appliedOn()) < birth)
                throw std::invalid_argument("生日不能晚于已有账号的申请日期。");
    }
}

void AccountServiceLJY::updateProfile(int actorPersonId, const std::string& name,
                                     const std::string& birthDate, const std::string& location, int avatarColor)
{
    requireText(name, "姓名");
    requireText(location, "所在地");
    validateBirthDate(birthDate, actorPersonId);
    // 所有校验通过后整体替换资料，编号和账号关联保持不变。
    platform_.people_.at(actorPersonId) = PersonLJY(actorPersonId, name, birthDate, location,
        avatarColor == -1 ? requirePerson(actorPersonId).avatarColor() : avatarColor);
}

int AccountServiceLJY::openService(int actorPersonId, ServiceTypeLJY type,
                                  const std::string& nickname, const std::string& appliedOn)
{
    const auto& person = requirePerson(actorPersonId);
    const auto* service = platform_.findService(type);
    if (!service) throw std::invalid_argument("未知的服务类型。");
    requireText(nickname, "昵称");
    if (platform_.findAccountForPerson(actorPersonId, type))
        throw std::invalid_argument("本人已开通该服务。");
    const DateLJY applied = appliedOn.empty() ? referenceDate_ : DateLJY(appliedOn);
    if (referenceDate_ < applied)
        throw std::invalid_argument("申请日期不能晚于业务参考日期。");
    if (applied < DateLJY(person.birthDate()))
        throw std::invalid_argument("申请日期不能早于生日。");

    const bool shared = service->accountIdMode() == AccountIdModeLJY::SharedQQWeibo;
    std::string externalId;
    if (shared)
    {
        // 查找本人已经开通的共享号码服务，QQ/微博谁先开通都有效。
        for (const auto* account : platform_.accountsForPerson(actorPersonId))
        {
            const auto* existingService = platform_.findService(account->serviceType());
            if (existingService->accountIdMode() == AccountIdModeLJY::SharedQQWeibo)
            {
                externalId = account->externalId();
                break;
            }
        }
    }
    const bool allocateNumber = externalId.empty();
    int number = 0;
    if (allocateNumber)
    {
        number = nextNumber(shared ? platform_.lastSharedNumber_ : platform_.lastWeChatNumber_);
        externalId = shared ? std::to_string(number) : "wx_" + std::to_string(number);
    }
    if (platform_.findAccount(type, externalId))
        throw std::logic_error("该服务中的对外号码已存在。");
    const int accountId = nextNumber(platform_.lastAccountId_);
    const auto added = platform_.accounts_.emplace(accountId,
        ServiceAccountLJY(accountId, actorPersonId, type, externalId, nickname, applied.toString()));
    if (!added.second) throw std::logic_error("内部账号编号已存在。");
    // 保存成功后才推进号码，业务失败不留下账号或消耗计数。
    platform_.lastAccountId_ = accountId;
    if (allocateNumber)
    {
        if (shared) platform_.lastSharedNumber_ = number;
        else platform_.lastWeChatNumber_ = number;
    }
    return accountId;
}

void AccountServiceLJY::updateNickname(int actorPersonId, int accountId, const std::string& nickname)
{
    const auto& account = requireOwnedAccount(actorPersonId, accountId);
    requireText(nickname, "昵称");
    platform_.accounts_.at(accountId) = ServiceAccountLJY(account.accountId(), account.personId(),
        account.serviceType(), account.externalId(), nickname, account.appliedOn());
}

void AccountServiceLJY::bindWeChatToQQ(int actorPersonId, int weChatAccountId, int qqAccountId)
{
    const auto& weChat = requireOwnedAccount(actorPersonId, weChatAccountId);
    const auto& qq = requireOwnedAccount(actorPersonId, qqAccountId);
    if (weChat.serviceType() != ServiceTypeLJY::WeChat || qq.serviceType() != ServiceTypeLJY::QQ)
        throw std::invalid_argument("绑定时须按微信、QQ 的顺序提供账号。");
    if (platform_.weChatToQQ_.count(weChatAccountId) != 0)
        throw std::invalid_argument("微信账号已绑定。");
    for (const auto& binding : platform_.weChatToQQ_)
        if (binding.second == qqAccountId) throw std::invalid_argument("QQ 账号已绑定。");
    platform_.weChatToQQ_.emplace(weChatAccountId, qqAccountId);
}

void AccountServiceLJY::changePassword(int actorPersonId, const std::string& oldPassword,
                                     const std::string& newPassword)
{
    requirePerson(actorPersonId);
    auto found = platform_.credentials_.find(actorPersonId);
    if (found == platform_.credentials_.end() || !found->second.matches(oldPassword))
        throw std::invalid_argument("原密码错误。");
    PasswordCredentialLJY replacement(newPassword);
    found->second = replacement;
}

void AccountServiceLJY::unbindWeChatFromQQ(int actorPersonId, int weChatAccountId)
{
    const auto& wx = requireOwnedAccount(actorPersonId, weChatAccountId);
    if (wx.serviceType() != ServiceTypeLJY::WeChat)
        throw std::invalid_argument("请提供本人微信账号。");
    if (platform_.weChatToQQ_.erase(weChatAccountId) == 0)
        throw std::invalid_argument("微信与 QQ 尚未绑定。");
}

int AccountServiceLJY::registrationAge(int accountId) const
{
    const auto* account = platform_.findAccount(accountId);
    if (!account) throw std::invalid_argument("账号不存在。");
    return DateLJY(account->appliedOn()).fullYearsUntil(referenceDate_);
}
