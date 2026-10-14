#include "domain/PasswordCredentialLJY.h"
#include "ui/ConsoleAppLJY.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

// 检查场景结果，失败时保留便于定位的原因。
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// 确认无效操作由业务拒绝，而不是意外通过。
void expectInvalid(const std::function<void()>& action)
{
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument.");
}

// 创建仅供测试的账号；密码不是用户真实凭据。
ServiceAccountLJY registerUser(SessionManagerLJY& session, const std::string& name = "Alice",
                              ServiceTypeLJY type = ServiceTypeLJY::QQ)
{
    return session.registerAccount(name, "2000-01-01", "Changchun", "TestOnly123!", type, name);
}

// 用字符串流驱动真实菜单，便于验证输入、输出和最终业务状态。
std::string runMenu(SessionManagerLJY& session, const std::string& script)
{
    std::istringstream input(script);
    std::ostringstream output;
    ConsoleAppLJY app(session, input, output);
    require(app.run() == 0, "Console exit failed.");
    require(output.str().find(">>>按任意键继续") == std::string::npos,
            "Script input must not pause or consume the next menu command.");
    return output.str();
}

// 验证长度边界、ASCII 字符范围和密码匹配，防止非法密码被接受。
void testPassword()
{
    PasswordCredentialLJY credential("TestOnly123!");
    require(credential.matches("TestOnly123!"), "Valid password rejected.");
    require(!credential.matches("TestOnly123?") && !credential.matches(""), "Wrong password accepted.");
    expectInvalid([] { PasswordCredentialLJY shortPassword("short"); });
    expectInvalid([] { PasswordCredentialLJY blank("        "); });
    expectInvalid([] { PasswordCredentialLJY longPassword(std::string(21, 'x')); });
    expectInvalid([] { PasswordCredentialLJY control("test\n1234"); });
    for (int length : {8, 20})
    {
        const std::string password(length, '~');
        PasswordCredentialLJY boundary(password);
        require(boundary.matches(password), "Valid boundary password rejected.");
    }
    // 枚举全部字节，确保空格、DEL、高位字节与控制字符均被拒绝。
    for (int value = 0; value <= 255; ++value)
    {
        std::string password = "Valid123";
        password.push_back(static_cast<char>(value));
        if (value >= 33 && value <= 126) PasswordCredentialLJY::validatePassword(password);
        else
        {
            expectInvalid([&] { PasswordCredentialLJY::validatePassword(password); });
            require(!credential.matches(password), "Invalid byte accepted at login.");
        }
    }
    expectInvalid([] { PasswordCredentialLJY chinese("密码abcd"); });
}

// 无效注册不创建人员或账号，也不消耗编号；注册不会自动登录。
void testRegistration()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform, "2026-09-09");
    expectInvalid([&] { session.registerAccount("A", "2027-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "A"); });
    expectInvalid([&] { session.registerAccount("A", "2000-01-01", "City", "short", ServiceTypeLJY::QQ, "A"); });
    expectInvalid([&] { session.registerAccount("A", "2000-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, " "); });
    expectInvalid([&] { session.registerAccount("A", "2000-01-01", "City", "TestOnly123!", static_cast<ServiceTypeLJY>(99), "A"); });
    require(platform.personCount() == 0 && platform.accountCount() == 0, "Failed registration left data.");
    const auto account = registerUser(session);
    require(account.personId() == 1 && account.accountId() == 1 && account.externalId() == "10001", "IDs consumed by failures.");
    require(!session.isLoggedIn() && platform.personCount() == 1, "Registration authenticated implicitly.");
}

// 首次登录验证密码、服务和对外号码；失败不建立会话。
void testAuthentication()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform, "2026-09-09");
    const auto account = registerUser(session);
    expectInvalid([&] { session.login(ServiceTypeLJY::QQ, account.externalId(), "wrong123"); });
    expectInvalid([&] { session.login(ServiceTypeLJY::QQ, "unknown", "TestOnly123!"); });
    expectInvalid([&] { session.login(ServiceTypeLJY::Weibo, account.externalId(), "TestOnly123!"); });
    require(!session.isLoggedIn() && platform.accountCount() == 1, "Failed login changed state.");
    session.login(ServiceTypeLJY::QQ, account.externalId(), "TestOnly123!");
    require(session.currentPerson().personId() == account.personId(), "Wrong acting person.");
    require(session.serviceState(ServiceTypeLJY::QQ) == ServiceLoginStateLJY::LoggedIn, "First service not logged in.");
    expectInvalid([&] { session.login(ServiceTypeLJY::QQ, account.externalId(), "TestOnly123!"); });
    expectInvalid([&] { registerUser(session, "Bob"); });
    require(session.currentAccount().accountId() == account.accountId(), "Rejected login replaced session.");
}

// 匿名状态不能访问资料、修改信息、开通服务或确认登录。
void testAnonymousGuards()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    expectInvalid([&] { session.currentPerson(); });
    expectInvalid([&] { session.currentAccount(); });
    expectInvalid([&] { session.accounts(); });
    expectInvalid([&] { session.openService(ServiceTypeLJY::QQ, "A"); });
    expectInvalid([&] { session.updateProfile("A", "2000-01-01", "City"); });
    expectInvalid([&] { session.updateNickname("A"); });
    expectInvalid([&] { session.enterService(ServiceTypeLJY::QQ, true); });
    expectInvalid([&] { session.serviceState(ServiceTypeLJY::QQ); });
    expectInvalid([&] { session.bindWeChatToQQ(); });
    expectInvalid([&] { session.boundQQNumber(); });
    expectInvalid([&] { session.registrationAge(); });
    session.logout();
    require(platform.personCount() == 0 && platform.accountCount() == 0, "Anonymous access mutated data.");
}

// 跨服务确认和取消只影响登录状态，不能隐式开通服务。
void testServiceConfirmation()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto qq = registerUser(session);
    session.login(ServiceTypeLJY::QQ, qq.externalId(), "TestOnly123!");
    expectInvalid([&] { session.enterService(ServiceTypeLJY::WeChat, true); });
    require(platform.accountCount() == 1, "Switch implicitly opened service.");
    const int wx = session.openService(ServiceTypeLJY::WeChat, "WX");
    require(session.currentAccount().accountId() == qq.accountId(), "Open switched service.");
    require(session.serviceState(ServiceTypeLJY::WeChat) == ServiceLoginStateLJY::PendingConfirmation, "Open authenticated service.");
    require(!session.enterService(ServiceTypeLJY::WeChat, false), "Cancel succeeded as login.");
    require(session.currentAccount().accountId() == qq.accountId(), "Cancel changed current service.");
    require(session.serviceState(ServiceTypeLJY::WeChat) == ServiceLoginStateLJY::PendingConfirmation, "Cancel changed pending state.");
    require(session.enterService(ServiceTypeLJY::WeChat, true), "Confirmation failed.");
    require(session.currentAccount().accountId() == wx, "Wrong target service.");
    require(session.serviceState(ServiceTypeLJY::QQ) == ServiceLoginStateLJY::LoggedIn, "Switch lost earlier login.");
    expectInvalid([&] { session.enterService(static_cast<ServiceTypeLJY>(99), true); });
    expectInvalid([&] { session.serviceState(static_cast<ServiceTypeLJY>(99)); });
    require(session.currentAccount().accountId() == wx, "Invalid service changed state.");
}

// 注销清空所有服务状态；再次登录需要密码并重新确认其他服务。
void testLogoutAndRelogin()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto qq = registerUser(session);
    session.login(ServiceTypeLJY::QQ, qq.externalId(), "TestOnly123!");
    const int wx = session.openService(ServiceTypeLJY::WeChat, "WX");
    session.enterService(ServiceTypeLJY::WeChat, true);
    session.logout();
    session.logout();
    expectInvalid([&] { session.updateNickname("Intruder"); });
    expectInvalid([&] { session.enterService(ServiceTypeLJY::QQ, true); });
    expectInvalid([&] { session.openService(ServiceTypeLJY::Weibo, "Intruder"); });
    expectInvalid([&] { session.updateProfile("Intruder", "2000-01-01", "City"); });
    expectInvalid([&] { session.bindWeChatToQQ(); });
    expectInvalid([&] { session.currentPerson(); });
    require(platform.findAccount(wx)->nickname() == "WX", "Logged out mutation succeeded.");
    session.login(ServiceTypeLJY::WeChat, platform.findAccount(wx)->externalId(), "TestOnly123!");
    require(session.serviceState(ServiceTypeLJY::QQ) == ServiceLoginStateLJY::PendingConfirmation, "Stale login survived logout.");
    require(session.serviceState(ServiceTypeLJY::WeChat) == ServiceLoginStateLJY::LoggedIn, "Relogin failed.");
}

// 同一人员的一份凭据可用于各服务；没有凭据的旧测试人员不能登录。
void testSharedCredentials()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto wb = registerUser(session, "Alice", ServiceTypeLJY::Weibo);
    session.login(ServiceTypeLJY::Weibo, wb.externalId(), "TestOnly123!");
    const int qq = session.openService(ServiceTypeLJY::QQ, "QQ");
    require(platform.findAccount(qq)->externalId() == wb.externalId(), "Shared ID changed.");
    session.logout();
    session.login(ServiceTypeLJY::QQ, wb.externalId(), "TestOnly123!");
    session.logout();
    AccountServiceLJY lowLevel(platform);
    const int legacy = lowLevel.createPerson("Legacy", "2000-01-01", "City");
    const int legacyQQ = lowLevel.openService(legacy, ServiceTypeLJY::QQ, "Legacy");
    expectInvalid([&] { session.login(ServiceTypeLJY::QQ, platform.findAccount(legacyQQ)->externalId(), "TestOnly123!"); });
    require(!session.isLoggedIn(), "Uncredentialed person authenticated.");
}

// 两个会话的身份独立，修改和注销不能串到另一个人员。
void testSessionIsolation()
{
    PlatformLJY platform;
    SessionManagerLJY first(platform), second(platform);
    const auto a = registerUser(first);
    const auto b = second.registerAccount("Bob", "2000-01-01", "City", "BobOnly456!", ServiceTypeLJY::QQ, "Bob");
    first.login(ServiceTypeLJY::QQ, a.externalId(), "TestOnly123!");
    second.login(ServiceTypeLJY::QQ, b.externalId(), "BobOnly456!");
    expectInvalid([&] { first.login(ServiceTypeLJY::QQ, b.externalId(), "BobOnly456!"); });
    first.updateNickname("Alice Updated");
    first.updateProfile("Alice New", "2000-01-01", "Jilin");
    require(second.currentAccount().nickname() == "Bob" && second.currentPerson().name() == "Bob", "Cross-person write.");
    first.logout();
    require(second.isLoggedIn(), "Logout affected another session.");
    expectInvalid([&] { first.login(ServiceTypeLJY::QQ, b.externalId(), "TestOnly123!"); });
    require(!first.isLoggedIn(), "Other person's password accepted.");
}

// 绑定仅能使用本人的两个已确认服务，重复绑定失败。
void testBinding()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto qq = registerUser(session);
    session.login(ServiceTypeLJY::QQ, qq.externalId(), "TestOnly123!");
    expectInvalid([&] { session.bindWeChatToQQ(); });
    session.openService(ServiceTypeLJY::WeChat, "WX");
    expectInvalid([&] { session.bindWeChatToQQ(); });
    require(session.boundQQNumber().empty(), "Unconfirmed binding persisted.");
    session.enterService(ServiceTypeLJY::WeChat, true);
    session.bindWeChatToQQ();
    require(session.boundQQNumber() == qq.externalId(), "Binding used wrong person.");
    expectInvalid([&] { session.bindWeChatToQQ(); });
}

// 资料和昵称修改只作用于当前人员及服务，非法修改保持原值。
void testProfileAndNickname()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform, "2026-09-09");
    const auto qq = registerUser(session);
    session.login(ServiceTypeLJY::QQ, qq.externalId(), "TestOnly123!");
    session.openService(ServiceTypeLJY::WeChat, "WX");
    session.updateNickname("QQ New");
    require(platform.findAccountForPerson(qq.personId(), ServiceTypeLJY::WeChat)->nickname() == "WX", "Modified pending service.");
    expectInvalid([&] { session.updateNickname(" "); });
    expectInvalid([&] { session.updateProfile("Wrong", "2027-01-01", "Wrong"); });
    require(session.currentAccount().nickname() == "QQ New" && session.currentPerson().name() == "Alice", "Invalid edit changed data.");
    require(session.registrationAge() == 0, "New account age must be zero.");
}

// 生日在输入时检查，错误、取消和输入结束均不提前提交资料。
void checkConsoleBirthDate()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform, "2026-09-09");
    const std::string invalidDates =
        "11wds\n\n2000/01/01\n2000-1-01\n200x-01-01\n0000-01-01\n"
        "2000-00-01\n2000-13-01\n2000-01-00\n2000-01-32\n2000-04-31\n"
        "1900-02-29\n2025-02-29\n2026-09-10\n9999-12-31\n";
    const auto rejected = runMenu(session, "2\n1\nAlice\n" + invalidDates + "0\n0\n");
    require(rejected.find("请重新输入生日") != std::string::npos &&
            rejected.find("所在地：") == std::string::npos &&
            rejected.find("设置密码") == std::string::npos, "Invalid birthday advanced to later fields.");
    require(platform.personCount() == 0 && platform.accountCount() == 0, "Birthday cancellation committed data.");
    runMenu(session, "2\n1\nAlice\n11wds\n");
    require(platform.personCount() == 0, "Birthday EOF committed data.");
    const auto registered = runMenu(session,
        "2\n1\nAlice\n" + invalidDates + std::string(4097, 'x') +
        "\n2000-02-29\nCity\nNick\nTestOnly123!\nTestOnly123!\n0\n");
    require(platform.personCount() == 1 && platform.findPerson(1)->birthDate() == "2000-02-29" &&
            platform.findPerson(1)->name() == "Alice", "Birthday retry lost name or valid leap date.");
    require(registered.find("姓名：") == registered.rfind("姓名：") &&
            registered.find("操作失败") == std::string::npos, "Birthday retry left registration.");
    require(registered.find("用户昵称：") != std::string::npos &&
            registered.find("服务昵称") == std::string::npos, "Nickname label was not updated.");

    // 参考日期采用会话日期；生日等于当天合法，修改时仍不能晚于已有账号申请日期。
    session.validateBirthDate("2026-09-09");
    expectInvalid([&] { session.validateBirthDate("2026-09-10"); });
    SessionManagerLJY later(platform, "2026-09-10");
    later.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
    const auto updated = runMenu(later, "5\nAlice New\n11wds\n2026-09-10\n2004-02-29\nNew City\n0\n0\n");
    require(updated.find("生日不能晚于已有账号的申请日期") != std::string::npos &&
            updated.find("操作失败") == std::string::npos, "Profile birthday limit did not retry locally.");
    require(platform.findPerson(1)->name() == "Alice New" &&
            platform.findPerson(1)->birthDate() == "2004-02-29" &&
            platform.findPerson(1)->location() == "New City", "Profile birthday retry lost entered data.");
    for (const auto& tail : {std::string("0\n0\n0\n"), std::string()})
    {
        later.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
        runMenu(later, "5\nDiscarded\n11wds\n" + tail);
        require(platform.findPerson(1)->name() == "Alice New" && !later.isLoggedIn(),
                "Cancelled profile birthday changed data or failed to exit.");
    }
}

// 数字菜单拒绝非法输入和整数溢出，且可正常返回与退出。
void testConsoleInput()
{
    checkConsoleBirthDate();
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto output = runMenu(session, "abc\n1x\n1.2\n999999999999999999999\n-1\n\n3\n1\n0\n0\n");
    require(output.find("请输入 0 至 3 之间的整数") != std::string::npos, "No invalid input message.");
    require(output.find("先注册并开通首项服务") != std::string::npos, "Help menu missing.");
    require(platform.personCount() == 0 && !session.isLoggedIn(), "Input handling mutated state.");
}

// 无效密码及确认错误只重输密码；取消和输入中断不能留下半份资料。
void testConsoleRegistrationCancellation()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto output = runMenu(session,
        "2\n1\nAlice\n0\n"
        "2\n1\nAlice\n2000-01-01\nCity\nNick\nTestOnly123!\nMismatch123\n"
        "0\n2\n2\nInterrupted\n");
    require(output.find("两次密码不一致") != std::string::npos, "Password confirmation missing.");
    require(platform.personCount() == 0 && platform.accountCount() == 0, "Incomplete registration committed.");
    for (const auto& tail : {std::string("short\n0\n0\n"), std::string("short\n"),
                             std::string("TestOnly123!\n0\n0\n"), std::string("TestOnly123!\n")})
    {
        runMenu(session, "2\n1\nAlice\n2000-01-01\nCity\nNick\n" + tail);
        require(platform.personCount() == 0 && platform.accountCount() == 0,
                "Password retry cancellation or EOF committed data.");
    }
    const auto retry = runMenu(session,
        "2\n1\nAlice\n2000-01-01\nCity\nNick\n"
        "short\n123456789012345678901\nInvalid 123\n密码abcd\n" +
        std::string(4097, 'x') + "\nTestOnly123!\nMismatch123\nTestOnly123!\nTestOnly123!\n0\n");
    require(platform.personCount() == 1 && platform.accountCount() == 1,
            "Password retries did not create exactly one account.");
    const auto* person = platform.findPerson(1);
    const auto* account = platform.findAccountForPerson(1, ServiceTypeLJY::QQ);
    require(person && person->name() == "Alice" && person->birthDate() == "2000-01-01" &&
            person->location() == "City" && account && account->nickname() == "Nick",
            "Password retries lost previously entered profile.");
    require(retry.find("姓名：") == retry.rfind("姓名：") &&
            retry.find("已填写的资料保留") != std::string::npos &&
            retry.find("操作失败") == std::string::npos, "Password retry returned to main menu.");
    require(retry.find("各项输入 0 可返回。") != std::string::npos &&
            retry.find("设置密码（隐藏密码，* 号占位；8-20 字节") != std::string::npos &&
            retry.find("TestOnly123!") == std::string::npos, "Password prompt or secrecy incorrect.");
    require(retry.find("再次输入密码") != std::string::npos &&
            retry.find("两次密码不一致") != std::string::npos &&
            retry.find("QQ 登录号码：10001") != std::string::npos,
            "Console must confirm password and display the registered account number.");
    session.login(ServiceTypeLJY::QQ, "10001", "TestOnly123!");
    session.logout();
}

// 通过菜单串起注册、开通、取消及确认、绑定、修改、注销和重新认证。
void testConsoleFullFlow()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform, "2026-09-09");
    const auto output = runMenu(session,
        "2\n1\n李佳业\n2000-01-01\n长春\nQQ昵称\nTestOnly123!\nTestOnly123!\n"
        "1\n1\n10001\nTestOnly123!\n"
        "3\n2\n微信昵称\n"
        "4\n2\n0\n"
        "4\n2\n1\n"
        "7\n"
        "5\n李佳业\n2000-01-01\n吉林\n"
        "6\n新微信昵称\n1\n0\n"
        "1\n2\nwx_20001\nTestOnly123!\n1\n0\n0\n");
    require(platform.personCount() == 1 && platform.accountCount() == 2, "Menu flow created incorrect data.");
    require(platform.findPerson(1)->location() == "吉林", "Menu profile update failed.");
    const auto* wx = platform.findAccountForPerson(1, ServiceTypeLJY::WeChat);
    require(wx && wx->nickname() == "新微信昵称", "Menu nickname update failed.");
    const auto* bound = platform.boundQQForWeChat(wx->accountId());
    require(bound && bound->externalId() == "10001", "Menu binding failed.");
    require(output.find("已取消确认") != std::string::npos && output.find("已进入微信") != std::string::npos, "Missing confirmation flow.");
    require(output.find("操作失败") == std::string::npos, "Happy path produced an error.");
    require(output.find("TestOnly123!") == std::string::npos, "Password appeared in output.");
    require(!session.isLoggedIn(), "Exit left a session alive.");
}

// 资料修改输入中断时不提交，输入结束后也必须注销。
void testConsoleEOF()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto qq = registerUser(session);
    session.login(ServiceTypeLJY::QQ, qq.externalId(), "TestOnly123!");
    runMenu(session, "5\nIncomplete\n2000-01-01\n");
    require(platform.findPerson(qq.personId())->name() == "Alice", "EOF partially committed profile.");
    require(!session.isLoggedIn(), "EOF did not log out.");
}

// 错误密码后仍留在主菜单，未开通服务不能通过菜单自动登录。
void testConsoleRejections()
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    registerUser(session);
    const auto output = runMenu(session,
        "1\n1\n10001\nWrong1234\n"
        "1\n1\n10001\nTestOnly123!\n"
        "4\n2\n"
        "3\n1\nDuplicate\n"
        "1\n0\n0\n");
    require(output.find("账号或密码错误") != std::string::npos, "Missing credential failure.");
    require(output.find("尚未开通") != std::string::npos, "Missing unopened service rejection.");
    require(platform.accountCount() == 1 && !session.isLoggedIn(), "Rejected menu actions changed data.");
}

// 独立执行第 5、6 步的场景测试，任一失败使 CTest 失败。
int main()
{
    int failed = 0;
    const auto run = [&](const char* name, void (*test)()) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    run("password", testPassword);
    run("registration", testRegistration);
    run("authentication", testAuthentication);
    run("anonymous guards", testAnonymousGuards);
    run("service confirmation", testServiceConfirmation);
    run("logout and relogin", testLogoutAndRelogin);
    run("shared credentials", testSharedCredentials);
    run("session isolation", testSessionIsolation);
    run("binding", testBinding);
    run("profile and nickname", testProfileAndNickname);
    run("console input", testConsoleInput);
    run("console registration cancellation", testConsoleRegistrationCancellation);
    run("console full flow", testConsoleFullFlow);
    run("console EOF", testConsoleEOF);
    run("console rejections", testConsoleRejections);
    std::cout << (15 - failed) << "/15 scenario groups passed.\n";
    return failed == 0 ? 0 : 1;
}
