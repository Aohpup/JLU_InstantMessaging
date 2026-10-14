#include "services/SessionManagerLJY.h"
#include <iostream>
#include <stdexcept>

// 检查新增绑定路径的最终结果。
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

// 无效绑定和错误密码必须被拒绝。
template<class Action>
void rejected(Action action)
{
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid operation accepted.");
}

// QQ 和微信以不同顺序分别登录，注销后从任一服务均能绑定。
void independentLogins(bool weChatFirst, bool returnToFirst)
{
    PlatformLJY platform;
    SessionManagerLJY session(platform);
    const auto firstType = weChatFirst ? ServiceTypeLJY::WeChat : ServiceTypeLJY::QQ;
    const auto secondType = weChatFirst ? ServiceTypeLJY::QQ : ServiceTypeLJY::WeChat;
    const auto first = session.registerAccount("Alice", "2000-01-01", "City", "TestOnly123!", firstType, "A");
    session.login(firstType, first.externalId(), "TestOnly123!");
    const int secondId = session.openService(secondType, "B");
    const auto secondNumber = platform.findAccount(secondId)->externalId();
    rejected([&] { session.bindWeChatToQQ(); });
    session.logout();
    rejected([&] { session.login(secondType, secondNumber, "Wrong1234"); });
    session.login(firstType, first.externalId(), "TestOnly123!");
    rejected([&] { session.bindWeChatToQQ(); });
    session.logout();
    session.login(secondType, secondNumber, "TestOnly123!");
    require(session.serviceState(firstType) == ServiceLoginStateLJY::PendingConfirmation,
            "History improperly restored a logged-in state.");
    if (returnToFirst)
    {
        session.logout();
        session.login(firstType, first.externalId(), "TestOnly123!");
    }
    session.bindWeChatToQQ();
    require(!session.boundQQNumber().empty(), "Independent login binding failed.");
    rejected([&] { session.bindWeChatToQQ(); });
    session.logout();
    rejected([&] { session.bindWeChatToQQ(); });
}

// 历史归属平台账号：换会话仍可用，不能借用其他人员的登录记录。
void historyOwnership()
{
    PlatformLJY platform;
    SessionManagerLJY a(platform);
    auto first = a.registerAccount("Alice", "2000-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "A");
    a.login(ServiceTypeLJY::QQ, first.externalId(), "TestOnly123!");
    const int wx = a.openService(ServiceTypeLJY::WeChat, "WX");
    a.logout();
    SessionManagerLJY later(platform);
    later.login(ServiceTypeLJY::WeChat, platform.findAccount(wx)->externalId(), "TestOnly123!");
    later.bindWeChatToQQ();
    require(later.boundQQNumber() == first.externalId(), "History did not survive a new session.");
    later.logout();
    auto bob = later.registerAccount("Bob", "2000-01-01", "City", "TestOnly123!", ServiceTypeLJY::QQ, "B");
    later.login(ServiceTypeLJY::QQ, bob.externalId(), "TestOnly123!");
    later.openService(ServiceTypeLJY::WeChat, "WX B");
    rejected([&] { later.bindWeChatToQQ(); });
    // 当前会话双服务确认的原路径继续可用。
    later.enterService(ServiceTypeLJY::WeChat, true);
    later.bindWeChatToQQ();
    require(later.boundQQNumber() == bob.externalId(), "Current-session path regressed.");
}

int main()
{
    try
    {
        for (bool first : {false, true})
            for (bool returnToFirst : {false, true}) independentLogins(first, returnToFirst);
        historyOwnership();
        std::cout << "PASS 5 binding-history scenarios.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
