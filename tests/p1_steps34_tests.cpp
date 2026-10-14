#include "services/AccountServiceLJY.h"

#include <functional>
#include <iostream>
#include <stdexcept>

// 检查测试条件，不满足时抛出异常并说明失败原因。
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// 确认操作抛出输入校验异常；未抛出或异常类型不符则测试失败。
void expectInvalid(const std::function<void()>& action)
{
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument but action succeeded.");
}

// 验证日期格式、月份天数和闰年规则。
void testInvalidDates()
{
    for (const auto* date : {"2023-02-29", "1900-02-29", "2026-04-31", "2026-00-10",
                             "2026-13-01", "2026-01-00", "0000-01-01", "2026-9-09",
                             "2026/09/09", "2026-0x-09", "2026-09-09x", ""})
        expectInvalid([&] { DateLJY invalid(date); });
    require(DateLJY("2000-02-29").toString() == "2000-02-29", "Leap century must be valid.");
    require(DateLJY("2024-02-29").toString() == "2024-02-29", "Leap day must be valid.");
    require(DateLJY::today().toString().size() == 10, "Local date must be readable.");
}

// 验证注册年龄的周年边界及闰日处理。
void testAnniversaries()
{
    const DateLJY start("2020-09-09");
    require(start.fullYearsUntil(DateLJY("2026-09-08")) == 5, "Anniversary has not arrived.");
    require(start.fullYearsUntil(DateLJY("2026-09-09")) == 6, "Anniversary must count.");
    require(start.fullYearsUntil(start) == 0, "Same day must be zero.");
    const DateLJY leap("2020-02-29");
    require(leap.fullYearsUntil(DateLJY("2021-02-28")) == 0, "Leap anniversary convention.");
    require(leap.fullYearsUntil(DateLJY("2021-03-01")) == 1, "Leap anniversary in March.");
    expectInvalid([&] { start.fullYearsUntil(DateLJY("2019-01-01")); });
}

// 验证多个业务对象共用平台数据，人员编号连续且不重复。
void testPeopleAndSharedManagers()
{
    PlatformLJY platform;
    AccountServiceLJY first(platform, "2026-09-09"), second(platform, "2026-09-09");
    const int a = first.createPerson("Same name", "2000-01-01", "A");
    const auto* pointer = platform.findPerson(a);
    const int b = second.createPerson("Same name", "2000-01-01", "B");
    require(a > 0 && b != a && platform.personCount() == 2, "Managers must share ID allocation.");
    require(pointer == platform.findPerson(a) && pointer->location() == "A", "Map pointer must remain valid.");
    require(!platform.findPerson(-1) && platform.accountsForPerson(999).empty(), "Missing queries.");
}

// 验证资料合法性，并检查修改失败后原资料保持不变。
void testProfileValidation()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    expectInvalid([&] { manager.createPerson(" ", "2000-01-01", "A"); });
    expectInvalid([&] { manager.createPerson("A", "2000-01-01", "\t"); });
    expectInvalid([&] { manager.createPerson("A", "2026-09-10", "A"); });
    expectInvalid([&] { manager.createPerson("A", "2023-02-29", "A"); });
    require(platform.personCount() == 0, "Failed creation must not insert a person.");
    const int a = manager.createPerson("A", "2000-01-01", "City A");
    require(a == 1, "Failed creation must not consume the first ID.");
    manager.updateProfile(a, "Alice", "2000-02-29", "City B");
    expectInvalid([&] { manager.updateProfile(a, "Wrong", "2026-09-10", "Wrong"); });
    require(platform.findPerson(a)->name() == "Alice" && platform.findPerson(a)->location() == "City B",
            "Failed edit must preserve all old fields.");
    expectInvalid([&] { manager.updateProfile(999, "A", "2000-01-01", "A"); });
}

// 验证 QQ 和微博两种开通顺序都能复用本人的号码。
void testSharedIDsInBothOrders()
{
    for (bool weiboFirst : {false, true})
    {
        PlatformLJY platform;
        AccountServiceLJY manager(platform, "2026-09-09");
        const int a = manager.createPerson("A", "2000-01-01", "A");
        const auto firstType = weiboFirst ? ServiceTypeLJY::Weibo : ServiceTypeLJY::QQ;
        const auto secondType = weiboFirst ? ServiceTypeLJY::QQ : ServiceTypeLJY::Weibo;
        const int first = manager.openService(a, firstType, "First");
        const int second = manager.openService(a, secondType, "Second");
        require(first != second, "Internal IDs must be distinct across services.");
        const auto& external = platform.findAccount(first)->externalId();
        require(external == platform.findAccount(second)->externalId(), "External IDs must be shared.");
        require(platform.findAccount(firstType, external)->accountId() == first, "Lookup must include service.");
        require(platform.findAccount(secondType, external)->accountId() == second, "Cross-service lookup collision.");
    }
}

// 验证微信独立号码及平台各类查询接口。
void testIndependentIDsAndQueries()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09"), secondManager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    const int b = manager.createPerson("B", "2000-01-01", "B");
    const int wxA = manager.openService(a, ServiceTypeLJY::WeChat, "WX A");
    const int qqA = manager.openService(a, ServiceTypeLJY::QQ, "QQ A");
    const int wxB = secondManager.openService(b, ServiceTypeLJY::WeChat, "WX B");
    const int qqB = secondManager.openService(b, ServiceTypeLJY::QQ, "QQ B");
    require(platform.findAccount(wxA)->externalId() != platform.findAccount(wxB)->externalId(), "WeChat IDs unique.");
    require(platform.findAccount(qqA)->externalId() != platform.findAccount(qqB)->externalId(), "QQ IDs unique.");
    require(platform.findAccount(wxA)->externalId() != platform.findAccount(qqA)->externalId(), "Namespaces independent.");
    require(platform.accountsForPerson(a).size() == 2 && platform.accountsForPerson(b).size() == 2, "Owner filtering.");
    require(platform.findAccountForPerson(a, ServiceTypeLJY::QQ)->accountId() == qqA, "Find person service.");
    require(!platform.findAccountForPerson(a, ServiceTypeLJY::Weibo), "Unopened service must remain absent.");
    require(!platform.findAccount(999) && !platform.findAccount(ServiceTypeLJY::QQ, "missing"), "Unknown accounts.");
    require(platform.accountCount() == 4, "Shared allocation across managers.");
}

// 验证非法开通被拒绝，失败后不新增账号或消耗号码。
void testOpenRejections()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    expectInvalid([&] { manager.openService(999, ServiceTypeLJY::QQ, "A"); });
    expectInvalid([&] { manager.openService(a, static_cast<ServiceTypeLJY>(99), "A"); });
    expectInvalid([&] { manager.openService(a, ServiceTypeLJY::QQ, " "); });
    expectInvalid([&] { manager.openService(a, ServiceTypeLJY::QQ, "A", "2026-09-10"); });
    expectInvalid([&] { manager.openService(a, ServiceTypeLJY::QQ, "A", "1999-12-31"); });
    expectInvalid([&] { manager.openService(a, ServiceTypeLJY::QQ, "A", "2025-02-29"); });
    require(platform.accountCount() == 0, "Failed opens must leave no records.");
    const int qq = manager.openService(a, ServiceTypeLJY::QQ, "QQ");
    require(qq == 1 && platform.findAccount(qq)->externalId() == "10001", "Failures must not consume IDs.");
    expectInvalid([&] { manager.openService(a, ServiceTypeLJY::QQ, "Duplicate"); });
    require(platform.accountCount() == 1 && platform.findAccount(qq)->nickname() == "QQ", "Duplicate changed state.");
}

// 验证生日、申请日期和业务参考日期之间的先后约束。
void testDatesAcrossProfileAndAccounts()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    const int qq = manager.openService(a, ServiceTypeLJY::QQ, "QQ", "2020-09-09");
    const int wb = manager.openService(a, ServiceTypeLJY::Weibo, "WB", "2020-09-10");
    require(manager.registrationAge(qq) == 6 && manager.registrationAge(wb) == 5, "Registration age belongs to individual service application.");
    expectInvalid([&] { manager.updateProfile(a, "Bad", "2021-01-01", "Bad"); });
    require(platform.findPerson(a)->birthDate() == "2000-01-01", "Invalid profile changed birth date.");
    expectInvalid([&] { manager.registrationAge(999); });
}

// 验证仅能修改本人账号的昵称，并拒绝无效输入。
void testNicknameOwnership()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    const int b = manager.createPerson("B", "2000-01-01", "B");
    const int qq = manager.openService(a, ServiceTypeLJY::QQ, "Original");
    const auto* original = platform.findAccount(qq);
    const auto external = original->externalId();
    manager.updateNickname(a, qq, "Updated");
    expectInvalid([&] { manager.updateNickname(b, qq, "Intruder"); });
    expectInvalid([&] { manager.updateNickname(a, qq, "\t"); });
    expectInvalid([&] { manager.updateNickname(a, 999, "Missing"); });
    require(original == platform.findAccount(qq) && original->nickname() == "Updated", "Nickname/reference changed.");
    require(original->externalId() == external && original->personId() == a, "Immutable identifiers changed.");
}

// 验证本人微信与 QQ 绑定成功，并拒绝重复绑定。
void testBindingAndDuplicates()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    const int qq = manager.openService(a, ServiceTypeLJY::QQ, "QQ");
    const int wx = manager.openService(a, ServiceTypeLJY::WeChat, "WX");
    require(!platform.boundQQForWeChat(wx), "Accounts must not auto-bind.");
    const auto wxNumber = platform.findAccount(wx)->externalId();
    manager.bindWeChatToQQ(a, wx, qq);
    expectInvalid([&] { manager.bindWeChatToQQ(a, wx, qq); });
    require(platform.boundQQForWeChat(wx)->accountId() == qq, "Repeated binding damaged relation.");
    require(platform.findAccount(wx)->externalId() == wxNumber && platform.accountCount() == 2, "Binding merged accounts.");
}

// 验证绑定双方的服务类型和归属，拒绝跨人绑定。
void testBindingOwnershipAndTypes()
{
    PlatformLJY platform;
    AccountServiceLJY manager(platform, "2026-09-09");
    const int a = manager.createPerson("A", "2000-01-01", "A");
    const int b = manager.createPerson("B", "2000-01-01", "B");
    const int qqA = manager.openService(a, ServiceTypeLJY::QQ, "QQ A");
    const int wxA = manager.openService(a, ServiceTypeLJY::WeChat, "WX A");
    const int qqB = manager.openService(b, ServiceTypeLJY::QQ, "QQ B");
    const int wxB = manager.openService(b, ServiceTypeLJY::WeChat, "WX B");
    const int wbA = manager.openService(a, ServiceTypeLJY::Weibo, "WB A");
    expectInvalid([&] { manager.bindWeChatToQQ(a, wxA, qqB); });
    expectInvalid([&] { manager.bindWeChatToQQ(a, wxB, qqA); });
    expectInvalid([&] { manager.bindWeChatToQQ(b, wxA, qqA); });
    expectInvalid([&] { manager.bindWeChatToQQ(a, qqA, wxA); });
    expectInvalid([&] { manager.bindWeChatToQQ(a, wxA, wbA); });
    expectInvalid([&] { manager.bindWeChatToQQ(a, wxA, 999); });
    expectInvalid([&] { manager.bindWeChatToQQ(999, wxA, qqA); });
    require(!platform.boundQQForWeChat(wxA) && !platform.boundQQForWeChat(wxB), "Failed binds changed state.");
    manager.bindWeChatToQQ(a, wxA, qqA);
    expectInvalid([&] { manager.bindWeChatToQQ(b, wxB, qqA); });
    manager.bindWeChatToQQ(b, wxB, qqB);
    require(platform.boundQQForWeChat(wxA)->accountId() == qqA && platform.boundQQForWeChat(wxB)->accountId() == qqB,
            "Cross-person conflict overwrote a valid binding.");
}

// 验证通过基类接口读取三种服务各自的规则。
void testServicePolymorphism()
{
    PlatformLJY platform;
    const auto* qq = platform.findService(ServiceTypeLJY::QQ);
    const auto* wx = platform.findService(ServiceTypeLJY::WeChat);
    const auto* wb = platform.findService(ServiceTypeLJY::Weibo);
    require(qq && wx && wb, "Platform must own all three services.");
    require(qq->accountIdMode() == wb->accountIdMode() && qq->accountIdMode() != wx->accountIdMode(), "ID modes.");
    require(wx->defaultGroupStyle() == GroupStyleLJY::WeChat && wb->defaultGroupStyle() == GroupStyleLJY::QQ,
            "Group styles must match agreed service defaults.");
    require(!platform.findService(static_cast<ServiceTypeLJY>(99)), "Unknown service must not be created.");
}

// 逐组执行测试并汇总结果；存在失败时返回非零退出码。
int main()
{
    int failed = 0;
    const auto run = [&](const char* name, void (*test)()) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    run("invalid dates", testInvalidDates);
    run("anniversaries", testAnniversaries);
    run("people and shared managers", testPeopleAndSharedManagers);
    run("profile validation", testProfileValidation);
    run("shared IDs in both orders", testSharedIDsInBothOrders);
    run("independent IDs and queries", testIndependentIDsAndQueries);
    run("open rejections", testOpenRejections);
    run("dates across profile and accounts", testDatesAcrossProfileAndAccounts);
    run("nickname ownership", testNicknameOwnership);
    run("binding and duplicates", testBindingAndDuplicates);
    run("binding ownership and types", testBindingOwnershipAndTypes);
    run("service polymorphism", testServicePolymorphism);
    std::cout << (12 - failed) << "/12 scenario groups passed.\n";
    return failed == 0 ? 0 : 1;
}
