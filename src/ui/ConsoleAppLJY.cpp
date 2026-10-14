#include "ui/ConsoleAppLJY.h"
#include "domain/PasswordCredentialLJY.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <climits>
#include <utility>

ConsoleAppLJY::ConsoleAppLJY(SessionManagerLJY& session, std::istream& input, std::ostream& output,
                             std::function<void()> saveAction, const std::string& databasePath)
    : session_(session), input_(input), output_(output), saveAction_(std::move(saveAction)), databasePath_(databasePath)
{
}

void ConsoleAppLJY::pressAnyKeyToContinue()
{
    const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode = 0;
    if (stopped_ || &input_ != &std::cin || !GetConsoleMode(console, &originalMode)) return;
    // 保存原始控制台模式，以便在程序结束时恢复。
    if (!SetConsoleMode(console, originalMode &
        ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)))    // 禁用回显和行缓冲，逐键读取
    {
        output_ << "无法启用等待按键，程序将退出。\n";
        stopped_ = true;
        return;
    }
    try
    {
        // 清除上一步遗留的回车和键盘事件，防止提示出现后立即被跳过。
        if (!FlushConsoleInputBuffer(console))
            throw std::runtime_error("清理控制台输入失败。");
        output_ << ">>>按任意键继续" << std::flush;
        while (true)
        {
            INPUT_RECORD record{};
            DWORD count = 0;
            if (!ReadConsoleInputW(console, &record, 1, &count) || count == 0)
                throw std::runtime_error("读取继续按键失败。");
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
            const auto& key = record.Event.KeyEvent;    // 只处理按下事件，忽略释放事件
            if (key.wVirtualKeyCode == VK_SHIFT || key.wVirtualKeyCode == VK_CONTROL ||
                key.wVirtualKeyCode == VK_MENU) continue;
            if (key.uChar.UnicodeChar == 3) continue;
            if (key.uChar.UnicodeChar == 26) stopped_ = true;
            break;
        }
    }
    catch (...) //...是捕获所有异常的意思
    {
        SetConsoleMode(console, originalMode);
        throw;
    }
    const bool restored = SetConsoleMode(console, originalMode) != 0;
    output_ << '\n';
    if (!restored)
    {
        output_ << "恢复控制台输入模式失败，程序将退出。\n";
        stopped_ = true;
    }
}

bool ConsoleAppLJY::readInputLine(std::string& text)
{
    const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (&input_ != &std::cin || !GetConsoleMode(console, &mode))
        return static_cast<bool>(std::getline(input_, text));
    std::wstring line;
    while (true)
    {
        wchar_t buffer[256];
        DWORD count = 0;
        if (!ReadConsoleW(console, buffer, 256, &count, nullptr))
        {
            if (GetLastError() == ERROR_OPERATION_ABORTED) continue;
            throw std::runtime_error("读取控制台文本失败。");
        }
        if (count == 0) continue;
        line.append(buffer, count);
        if (line.find(L'\n') != std::wstring::npos) break;
    }
    if (line.find(wchar_t(26)) != std::wstring::npos) return false;
    while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) line.pop_back();
    // Windows 控制台返回宽字符，统一转换成业务层使用的 UTF-8 字符串。
    if (line.empty()) { text.clear(); return true; }
    const int size = WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size == 0) throw std::runtime_error("转换控制台文本失败。");
    text.resize(size);
    if (!WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()),
                            text.data(), size, nullptr, nullptr))
        throw std::runtime_error("转换控制台文本失败。");
    return true;
}

bool ConsoleAppLJY::readLine(const char* prompt, std::string& text, bool secret) //声明中secret默认值为false
{
    output_ << prompt << std::flush;
    DWORD mode = 0;
    const bool masked = secret && &input_ == &std::cin &&
                        GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);  // 获取控制台输入模式，判断是否支持密码星号输入
    const bool ok = masked ? readMaskedPassword(text) : readInputLine(text);
    if (!ok)
    {
        stopped_ = true;    // 标记程序已停止，防止后续操作继续执行
        return false;
    }
    if (text == "0")
    {
        output_ << "已返回，未提交本次操作。\n";
        return false;
    }
    if (!secret && text.size() > 4096) throw std::invalid_argument("输入过长，请缩短后重试。");
    return true;
}

bool ConsoleAppLJY::readBirthDate(std::string& text)
{
    while (!stopped_)
    {
        try
        {
            if (!readLine("生日（YYYY-MM-DD，0 返回）：", text)) return false;
            session_.validateBirthDate(text);
            return true;
        }
        catch (const std::invalid_argument& error)
        {
            output_ << "生日输入有误：" << error.what()
                    << "请重新输入生日，已填写的资料保留。\n";
        }
    }
    return false;
}

bool ConsoleAppLJY::readMaskedPassword(std::string& text)
{
    const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode = 0;
    if (!GetConsoleMode(console, &originalMode) ||
        !SetConsoleMode(console, originalMode &
            ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)))  // 禁用回显和行缓冲，逐键读取
        throw std::runtime_error("无法启用密码星号输入，已取消输入。");
    text.clear();
    bool completed = false;
    try
    {
        while (true)
        {
            INPUT_RECORD record{};
            DWORD count = 0;
            if (!ReadConsoleInputW(console, &record, 1, &count) || count == 0)
                throw std::runtime_error("读取密码输入失败。");
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
            const auto& key = record.Event.KeyEvent;
            const wchar_t ch = key.uChar.UnicodeChar;
            if (ch == L'\r') { completed = true; break; }
            if (ch == 3) continue;
            if (ch == 26) break;
            // 方向键、功能键没有输入字符；Ctrl+空格产生的空字符仍交给密码校验。
            if (ch == 0 && key.wVirtualKeyCode != VK_SPACE) continue;
            for (int i = 0; i < key.wRepeatCount; ++i)
            {
                if (ch == L'\b')
                {
                    if (!text.empty())
                    {
                        text.pop_back();
                        output_ << "\b \b" << std::flush;
                    }
                }
                else
                {
                    // 非 ASCII 字符保留为非法标记，提交时提示重输，不能静默忽略。
                    text.push_back(ch <= 127 ? static_cast<char>(ch) : '\x80');
                    output_ << '*' << std::flush;
                }
            }
        }
    }
    catch (...)
    {
        SetConsoleMode(console, originalMode);
        throw;
    }
    const bool restored = SetConsoleMode(console, originalMode) != 0;
    output_ << '\n';
    if (!restored) throw std::runtime_error("恢复控制台输入模式失败。");
    return completed;
}

int ConsoleAppLJY::readChoice(const char* prompt, int maximum)
{
    while (!stopped_)
    {
        output_ << prompt << std::flush;
        std::string line;
        if (!readInputLine(line))
        {
            stopped_ = true;
            return 0;
        }
        std::istringstream parser(line);
        int value = -1;
        if (parser >> value)
        {
            parser >> std::ws;
            if (parser.eof() && value >= 0 && value <= maximum) return value;
        }
        output_ << "请输入 0 至 " << maximum << " 之间的整数。\n";
    }
    return 0;
}

const char* ConsoleAppLJY::serviceName(ServiceTypeLJY type)
{
    switch (type)
    {
    case ServiceTypeLJY::QQ: return "QQ";
    case ServiceTypeLJY::WeChat: return "微信";
    case ServiceTypeLJY::Weibo: return "微博";
    }
    return "未知服务";
}

bool ConsoleAppLJY::chooseService(ServiceTypeLJY& type)
{
    const int choice = readChoice("选择服务：1 QQ  2 微信  3 微博  0 返回\n> ", 3);
    if (choice == 0) return false;
    const ServiceTypeLJY types[] = {ServiceTypeLJY::QQ, ServiceTypeLJY::WeChat, ServiceTypeLJY::Weibo};
    type = types[choice - 1];
    return true;
}

void ConsoleAppLJY::showMenu()
{
    if (!session_.isLoggedIn())
    {
        output_ << "\n主菜单\n1 登录\n2 注册并开通首项服务\n3 使用说明\n";
        if (saveAction_) output_ << "4 保存数据\n";
        output_ << "0 退出\n";
        return;
    }
    const auto account = session_.currentAccount();
    output_ << "\n当前服务：" << serviceName(account.serviceType()) << "  号码：" << account.externalId()
            << "\n1 查看个人资料\n2 查看服务及绑定\n3 开通其他服务\n4 切换服务\n"
            << "5 修改个人资料\n6 修改当前服务的用户昵称\n7 绑定微信与 QQ\n8 好友管理\n9 群管理\n";
    if (saveAction_) output_ << "10 保存数据\n";
    output_ << "0 统一注销\n";
}

void ConsoleAppLJY::registerAccount()
{
    ServiceTypeLJY type = ServiceTypeLJY::QQ;
    if (!chooseService(type)) return;
    std::string name, birth, location, nickname, password, repeated;
    output_ << "各项输入 0 可返回。\n";
    if (!readLine("姓名：", name) || !readBirthDate(birth) ||
        !readLine("所在地：", location) || !readLine("用户昵称：", nickname)) return;
    while (!stopped_)
    {
        if (!readLine("设置密码（隐藏密码，* 号占位；8-20 字节，仅允许大小写字母、数字和符号，不能含空格、中文或控制字符）：", password, true)) return;
        try { PasswordCredentialLJY::validatePassword(password); }
        catch (const std::invalid_argument& error)
        {
            output_ << error.what() << "请重新输入密码，已填写的资料保留。\n";
            continue;
        }
        if (!readLine("再次输入密码（隐藏密码，* 号占位）：", repeated, true)) return;
        if (password != repeated)
        {
            output_ << "两次密码不一致，请重新输入密码，已填写的资料保留。\n";
            continue;
        }
        break;
    }
    const auto account = session_.registerAccount(name, birth, location, password, type, nickname);
    output_ << "注册成功。" << serviceName(type) << " 登录号码：" << account.externalId()
            << "\n请使用该服务、号码和刚设置的密码登录。\n";
}

void ConsoleAppLJY::login()
{
    ServiceTypeLJY type = ServiceTypeLJY::QQ;
    std::string number, password;
    if (!chooseService(type) || !readLine("对外号码（0 返回）：", number) ||
        !readLine("密码（隐藏密码，* 号占位，0 返回）：", password, true)) return;
    session_.login(type, number, password);
    output_ << "登录成功。其他已开通服务可通过简单确认进入。\n";
}

void ConsoleAppLJY::showProfile()
{
    const auto person = session_.currentPerson();
    const auto account = session_.currentAccount();
    output_ << "姓名：" << person.name() << "\n生日：" << person.birthDate()
            << "\n所在地：" << person.location() << "\n当前服务：" << serviceName(account.serviceType())
            << "\n号码：" << account.externalId() << "\n用户昵称：" << account.nickname()
            << "\n申请日期：" << account.appliedOn() << "\n注册年龄：" << session_.registrationAge() << " 年\n";
}

void ConsoleAppLJY::showServices()
{
    const auto accounts = session_.accounts();
    for (const auto type : {ServiceTypeLJY::QQ, ServiceTypeLJY::WeChat, ServiceTypeLJY::Weibo})
    {
        const auto state = session_.serviceState(type);
        output_ << serviceName(type) << "：";
        if (state == ServiceLoginStateLJY::NotOpened) output_ << "未开通";
        else
        {
            output_ << (state == ServiceLoginStateLJY::LoggedIn ? "已登录" : "待确认登录");
            for (const auto& account : accounts)
                if (account.serviceType() == type)
                    output_ << " | 号码：" << account.externalId() << " | 用户昵称：" << account.nickname();
        }
        output_ << '\n';
    }
    const auto bound = session_.boundQQNumber();
    output_ << "微信绑定 QQ：" << (bound.empty() ? "未绑定" : bound) << '\n';
}

void ConsoleAppLJY::openService()
{
    ServiceTypeLJY type = ServiceTypeLJY::QQ;
    std::string nickname;
    if (!chooseService(type) || !readLine("用户昵称（0 返回）：", nickname)) return;
    session_.openService(type, nickname);
    output_ << "开通成功，新服务待确认登录。\n";
    showServices();
}

void ConsoleAppLJY::switchService()
{
    showServices();
    ServiceTypeLJY type = ServiceTypeLJY::QQ;
    if (!chooseService(type)) return;
    const auto state = session_.serviceState(type);
    if (state == ServiceLoginStateLJY::NotOpened)
        throw std::invalid_argument("该服务尚未开通，请先开通服务。");
    bool confirmed = true;
    if (state == ServiceLoginStateLJY::PendingConfirmation)
        confirmed = readChoice("确认以当前身份登录该服务？1 确认  0 取消\n> ", 1) == 1;
    if (session_.enterService(type, confirmed)) output_ << "已进入" << serviceName(type) << "。\n";
    else output_ << "已取消确认，服务状态保持不变。\n";
}

void ConsoleAppLJY::updateProfile()
{
    std::string name, birth, location;
    if (!readLine("新姓名（0 返回）：", name) || !readBirthDate(birth) ||
        !readLine("所在地（0 返回）：", location)) return;
    session_.updateProfile(name, birth, location);
    output_ << "个人资料已更新。\n";
}

void ConsoleAppLJY::updateNickname()
{
    std::string nickname;
    if (!readLine("新用户昵称（0 返回）：", nickname)) return;
    session_.updateNickname(nickname);
    output_ << "当前服务的用户昵称已更新。\n";
}

void ConsoleAppLJY::listFriends()
{
    const auto accounts = session_.friends();
    if (accounts.empty()) { output_ << "当前服务暂无好友。\n"; return; }
    for (const auto& account : accounts)
    {
        const auto remark = session_.friendRemark(account.externalId());
        output_ << "号码：" << account.externalId() << " | 用户昵称：" << account.nickname()
                << " | 我的备注：" << (remark.empty() ? "未设置" : remark) << '\n';
    }
}

void ConsoleAppLJY::showFriend()
{
    std::string number;
    if (!readLine("好友号码（0 返回）：", number)) return;
    const auto account = session_.friendAccount(number);
    const auto person = session_.friendPerson(number);
    const auto remark = session_.friendRemark(number);
    output_ << "好友号码：" << account.externalId() << "\n用户昵称：" << account.nickname()
            << "\n我的备注：" << (remark.empty() ? "未设置" : remark)
            << "\n姓名：" << person.name() << "\n生日：" << person.birthDate()
            << "\n所在地：" << person.location() << "\n申请日期：" << account.appliedOn() << '\n';
}

void ConsoleAppLJY::addFriend()
{
    std::string number;
    if (!readLine("对方在当前服务的号码（0 返回）：", number)) return;
    const auto target = session_.friendCandidate(number);
    output_ << "目标号码：" << target.externalId() << " | 用户昵称：" << target.nickname() << '\n';
    if (readChoice("确认发送好友申请？1 确认  0 取消\n> ", 1) == 0)
    {
        if (!stopped_) output_ << "已取消添加好友。\n";
        return;
    }
    session_.sendFriendRequest(number);
    output_ << "申请已发送，等待对方同意。\n";
}

void ConsoleAppLJY::updateFriendRemark()
{
    std::string number, remark;
    if (!readLine("好友号码（0 返回）：", number)) return;
    session_.friendAccount(number);
    if (!readLine("我的备注（最多 120 字节，留空清除，0 返回）：", remark)) return;
    session_.updateFriendRemark(number, remark);
    output_ << (remark.empty() ? "已清除我的备注。\n" : "我的备注已更新。\n");
}

void ConsoleAppLJY::removeFriend()
{
    std::string number;
    if (!readLine("好友号码（0 返回）：", number)) return;
    const auto target = session_.friendAccount(number);
    output_ << "目标号码：" << target.externalId() << " | 用户昵称：" << target.nickname() << '\n';
    if (readChoice("删除后双方关系及备注均清除。1 确认  0 取消\n> ", 1) == 0)
    {
        if (!stopped_) output_ << "已取消删除好友。\n";
        return;
    }
    session_.removeFriend(number);
    output_ << "好友已删除，双方关系及备注均已清除。\n";
}

void ConsoleAppLJY::friendMenu()
{
    while (!stopped_ && session_.isLoggedIn())
    {
        output_ << "\n好友管理（" << serviceName(session_.currentAccount().serviceType()) << "）\n"
                << "1 好友列表\n2 查询好友资料\n3 添加好友\n4 修改我的备注\n5 删除好友\n6 同服务共同好友\n7 跨服务共同好友\n8 跨服务推荐\n9 好友申请\n0 返回主菜单\n";
        const int choice = readChoice("> ", 9);
        if (stopped_ || choice == 0) return;
        try
        {
            switch (choice)
            {
            case 1: listFriends(); break;
            case 2: showFriend(); break;
            case 3: addFriend(); break;
            case 4: updateFriendRemark(); break;
            case 5: removeFriend(); break;
            case 6: showCommonFriends(); break;
            case 7: showCrossServiceCommonFriends(); break;
            case 8: recommendFriends(); break;
            case 9: friendRequests(); break;
            }
        }
        catch (const std::exception& error)
        {
            output_ << "操作失败：" << error.what() << '\n';
            if (!input_) stopped_ = true;
        }
        pressAnyKeyToContinue();
    }
}

bool ConsoleAppLJY::saveData()
{
    try
    {
        saveAction_();
        output_ << "保存成功：" << databasePath_ << '\n';
        return true;
    }
    catch (const std::exception& error)
    {
        output_ << "保存失败：" << error.what() << "\n保存位置：" << databasePath_
            << "\n本次修改仍在内存中，数据库保留上一次成功保存的数据。\n";
        return false;
    }
}

int ConsoleAppLJY::finish()
{
    while (saveAction_ && !saveData())
    {
        if (!stopped_)
        {
            const int choice = readChoice("1 重试保存并退出  2 放弃未保存修改并退出  0 返回菜单\n> ", 2);
            if (!stopped_)
            {
                if (choice == 0) return -1;
                if (choice == 1) continue;
                output_ << "已放弃未保存的修改，数据库保留上一次成功保存的数据。\n";
                break;
            }
        }
        // 输入结束时无法再取得选择，明确报告未保存并返回失败，不伪装成正常退出。
        session_.logout();
        output_ << "输入已结束，无法重试保存；本次修改未保存，程序异常退出。\n";
        return 1;
    }
    session_.logout();
    output_ << "程序已退出，登录状态已清空。\n";
    return 0;
}

int ConsoleAppLJY::run()
{
    output_ << "JLU 即时通信平台 | 好友与群管理\n";
    if (saveAction_)
        output_ << "数据保存到 SQLite；可主动保存，主菜单退出时自动保存。\n数据库位置："
            << databasePath_ << '\n';
    else output_ << "内存测试模式：数据仅保存在内存中，退出后清空。\n";
    while (!stopped_)
    {
        try
        {
            showMenu();
            const bool authenticated = session_.isLoggedIn();
            const int choice = readChoice("> ", authenticated ? (saveAction_ ? 10 : 9) : (saveAction_ ? 4 : 3));
            if (stopped_) break;
            if (!authenticated)
            {
                switch (choice)
                {
                case 0:
                {
                    const int result = finish();
                    if (result >= 0) return result;
                    break;
                }
                case 1: login(); break;
                case 2: registerAccount(); break;
                case 3:
                    output_ << "先注册并开通首项服务，再以服务类型、对外号码和密码登录。\n"
                            << "登录后可开通其他服务；QQ 与微博共享本人号码，微信独立发号。\n"
                            << "已开通服务须确认后登录；注销只清空登录状态，不删除账号或保存修改。\n"
                            << "各输入步骤可输入 0 返回。\n";
                    if (saveAction_) output_ << "主菜单 4 或登录菜单 10 保存，主菜单 0 保存并退出。\n";
                    else output_ << "当前为内存测试模式，退出后数据清空。\n";
                    break;
                case 4: saveData(); break;
                }
            }
            else
            {
                switch (choice)
                {
                case 0: session_.logout(); output_ << "已统一注销全部服务。\n"; break;
                case 1: showProfile(); break;
                case 2: showServices(); break;
                case 3: openService(); break;
                case 4: switchService(); break;
                case 5: updateProfile(); break;
                case 6: updateNickname(); break;
                case 8: friendMenu(); continue;
                case 9: groupMenu(); continue;
                case 7: session_.bindWeChatToQQ(); output_ << "微信与 QQ 绑定成功。\n"; break;
                case 10: saveData(); break;
                }
            }
        }
        catch (const std::exception& error)
        {
            output_ << "操作失败：" << error.what() << '\n';
            if (!input_) stopped_ = true;
        }
        // 统一在整项操作结束后暂停；局部重输仍在对应函数内部进行。
        try { pressAnyKeyToContinue(); }
        catch (const std::exception& error)
        {
            output_ << "等待按键失败：" << error.what() << '\n';
            stopped_ = true;
        }
    }
    return finish();
}


void ConsoleAppLJY::showCommonFriends()
{
    std::string number;
    if (!readLine("同服务另一账号号码（0 返回）：", number)) return;
    const auto accounts = session_.commonFriends(number);
    output_ << "同服务共同好友：" << accounts.size() << " 人。\n";
    for (const auto& account : accounts)
        output_ << "号码：" << account.externalId() << " | 用户昵称：" << account.nickname() << '\n';
}

void ConsoleAppLJY::showFriendQueryResult(const FriendQueryResultLJY& result)
{
    int index = 0;
    for (const auto& account : result.accounts())
        output_ << ++index << " 号码：" << account.externalId()
                << " | 用户昵称：" << account.nickname() << '\n';
    if (!result.skippedReasons().empty()) output_ << "未入选说明：\n";
    for (const auto& reason : result.skippedReasons()) output_ << reason << '\n';
}

void ConsoleAppLJY::showCrossServiceCommonFriends()
{
    output_ << "比较当前服务与本人的另一项服务，请选择源服务。\n";
    ServiceTypeLJY source = ServiceTypeLJY::QQ;
    if (!chooseService(source)) return;
    const auto result = session_.crossServiceCommonFriends(source);
    output_ << "跨服务共同好友：" << result.accounts().size() << " 人（按人员去重）。\n";
    showFriendQueryResult(result);
}

void ConsoleAppLJY::recommendFriends()
{
    output_ << "将本人另一项服务的好友推荐到当前服务，请选择源服务。\n";
    ServiceTypeLJY source = ServiceTypeLJY::QQ;
    if (!chooseService(source)) return;
    const auto result = session_.friendRecommendations(source);
    output_ << "可推荐好友：" << result.accounts().size() << " 人。\n";
    showFriendQueryResult(result);
    if (result.accounts().empty()) return;
    const int choice = readChoice("选择推荐序号（0 返回）：", static_cast<int>(result.accounts().size()));
    if (stopped_ || choice == 0) return;
    const auto& selected = result.accounts()[choice - 1];
    output_ << "目标服务：" << serviceName(selected.serviceType()) << " | 号码："
            << selected.externalId() << " | 用户昵称：" << selected.nickname() << '\n';
    if (readChoice("确认添加推荐好友？1 确认  0 取消\n> ", 1) == 0)
    {
        if (!stopped_) output_ << "已取消推荐添加。\n";
        return;
    }
    session_.sendFriendRequest(selected.externalId());
    output_ << "推荐申请已发送，等待对方同意。\n";
}


void ConsoleAppLJY::listGroups()
{
    const auto groups = session_.groups();
    if (groups.empty()) { output_ << "当前服务暂无加入的群。\n"; return; }
    for (const auto* group : groups)
        output_ << "群号：" << group->number() << " | 群名：" << group->name()
                << " | 管理模式：" << (group->style() == GroupStyleLJY::QQ ? "QQ" : "微信") << '\n';
}

void ConsoleAppLJY::showGroup(int number)
{
    std::vector<ServiceAccountLJY> accounts;
    const auto& group = session_.group(number, accounts);
    output_ << "群号：" << group.number() << " | 群名：" << group.name()
            << "\n所属服务：" << serviceName(group.serviceType())
            << " | 管理模式：" << (group.style() == GroupStyleLJY::QQ ? "QQ" : "微信")
            << "\n成员数量：" << accounts.size() << '\n';
    if (group.parentGroupId() != 0)
        output_ << "群类型：子群\n";
    for (const auto& account : accounts)
    {
        const auto* member = group.findMember(account.accountId());
        const char* role = account.accountId() == group.ownerAccountId() ? "群主" :
            group.style() == GroupStyleLJY::QQ && member->isAdministrator() ? "管理员" : "普通成员";
        output_ << "号码：" << account.externalId() << " | 用户昵称：" << account.nickname()
                << " | 身份：" << role << '\n';
    }
}

void ConsoleAppLJY::groupMenu()
{
    while (!stopped_ && session_.isLoggedIn())
    {
        output_ << "\n群管理（" << serviceName(session_.currentAccount().serviceType()) << "）\n";
        listGroups();
        const int number = readChoice("选择群号（可输入尚未加入的群号，0 返回主菜单）：", INT_MAX);
        if (stopped_ || number == 0) return;
        try
        {
            const auto& selected = session_.selectedGroup(number);
            bool selectAgain = false;
            while (!stopped_ && !selectAgain)
            {
                output_ << "\n当前群：" << number << " | 群名：" << selected.name()
                    << "\n所属服务：" << serviceName(selected.serviceType())
                    << " | 当前群管理模式：" << (selected.style() == GroupStyleLJY::QQ ? "QQ" : "微信")
                    << "\n1 切换群聊\n2 查询群资料和成员\n3 申请入群\n4 邀请入群\n5 查看申请与邀请\n6 处理申请或邀请"
                    << "\n7 退出群\n8 移除成员\n9 设置或取消管理员\n10 转让群主\n11 创建子群\n12 查看子群\n13 切换群管理模式\n0 返回主菜单\n";
                const int choice = readChoice("> ", 13);
                if (stopped_ || choice == 0) return;
                if (choice == 1) break;
                try
                {
                    if (choice == 2) showGroup(number);
                    else if (choice == 5) showGroupRequests(number);
                    else if (choice == 11) createSubgroup(number);
                    else if (choice == 12) showSubgroups(number);
                    else if (choice == 13) switchGroupMode(number);
                    else selectAgain = groupAction(choice, number);
                }
                catch (const std::exception& error) { output_ << "操作失败：" << error.what() << '\n'; }
                pressAnyKeyToContinue();
            }
        }
        catch (const std::exception& error)
        {
            output_ << "操作失败：" << error.what() << '\n';
            pressAnyKeyToContinue();
        }
    }
}

void ConsoleAppLJY::showGroupRequests(int number)
{
    const auto requests = session_.groupRequests(number);
    if (requests.empty()) { output_ << "当前群暂无相关申请或邀请。\n"; return; }
    const char* states[] = {"待处理","已同意","已拒绝","已失效"};
    for (const auto& request : requests)
        output_ << "请求编号：" << request.id() << " | 群号：" << request.groupNumber()
                << " | " << (request.type() == GroupRequestTypeLJY::Application ? "申请" : "邀请")
                << " | 发起者：" << request.senderNumber() << " | 入群账号：" << request.targetNumber()
                << " | " << states[static_cast<int>(request.state())] << '\n';
}
bool ConsoleAppLJY::groupAction(int choice, int number)
{
    if (choice == 6)
    {
        showGroupRequests(number);
        if (session_.groupRequests(number).empty()) return false;
        const int id = readChoice("请求编号（0 返回）：", INT_MAX);
        if (stopped_ || id == 0) return false;
        session_.checkGroupRequest(number, id);
        const int action = readChoice("处理：1 同意  2 拒绝  0 返回\n> ", 2);
        if (stopped_ || action == 0) return false;
        session_.handleGroupRequest(id, action == 1);
        output_ << (action == 1 ? "已同意，对方已入群。\n" : "已拒绝该请求。\n");
        return false;
    }
    GroupOperationLJY operation;
    switch (choice)
    {
    case 3: operation = GroupOperationLJY::Apply; break;
    case 4: operation = GroupOperationLJY::Invite; break;
    case 7: operation = GroupOperationLJY::Leave; break;
    case 8: operation = GroupOperationLJY::Kick; break;
    case 9: operation = GroupOperationLJY::SetAdministrator; break;
    case 10: operation = GroupOperationLJY::TransferOwner; break;
    default: throw std::invalid_argument("未知的群操作。");
    }
    // 先检查执行者权限，再展示成员和读取目标，最后才请求确认。
    session_.checkGroupOperation(number, operation);
    std::string target;
    if (choice == 4 || choice == 8 || choice == 9 || choice == 10)
    {
        if (choice != 4) showGroup(number);
        if (!readLine("目标账号号码（0 返回）：", target)) return false;
        session_.checkGroupOperation(number, operation, target);
    }
    int adminAction = 0;
    if (choice == 9)
    {
        adminAction = readChoice("管理员：1 设置  2 取消  0 返回\n> ", 2);
        if (stopped_ || adminAction == 0) return false;
    }
    const char* actions[] = {"","","","申请入群","邀请入群","","","退出群","移除成员","修改管理员","转让群主"};
    output_ << "操作：" << actions[choice] << " | 群号：" << number;
    if (!target.empty()) output_ << " | 目标账号：" << target;
    output_ << '\n';
    if (readChoice("确认执行？1 确认  0 取消\n> ", 1) == 0) return false;
    switch (choice)
    {
    case 3: { const int id = session_.applyToGroup(number); output_ << "申请已提交，请求编号：" << id << '\n'; break; }
    case 4: { const int id = session_.inviteToGroup(number, target); output_ << "邀请已发送，请求编号：" << id << '\n'; break; }
    case 7: session_.leaveGroup(number); output_ << "已退出群。\n"; return true;
    case 8: session_.kickGroupMember(number, target); output_ << "成员已移除。\n"; break;
    case 9: session_.setGroupAdministrator(number, target, adminAction == 1); output_ << "管理员设置已更新。\n"; break;
    case 10: session_.transferGroupOwner(number, target); output_ << "群主已转让，原群主成为普通成员。\n"; break;
    }
    return false;
}

void ConsoleAppLJY::createSubgroup(int parentNumber)
{
    session_.checkGroupOperation(parentNumber, GroupOperationLJY::CreateSubgroup);
    showGroup(parentNumber);
    const int number = readChoice("新子群群号（0 返回）：", INT_MAX);
    if (stopped_ || number == 0) return;
    std::string name, owner;
    if (!readLine("子群名称（0 返回）：", name) || !readLine("子群群主账号号码（0 返回）：", owner)) return;
    session_.checkGroupOperation(parentNumber, GroupOperationLJY::CreateSubgroup, owner);
    std::vector<std::string> members;
    output_ << "子群群主自动加入；逐个输入其他父群成员号码，0 结束选择。\n";
    while (!stopped_)
    {
        std::string member;
        if (!readLine("成员号码（0 结束选择）：", member)) break;
        session_.checkGroupOperation(parentNumber, GroupOperationLJY::CreateSubgroup, member);
        if (member == owner) throw std::invalid_argument("子群群主已自动加入，无需重复选择。");
        for (const auto& chosen : members)
            if (chosen == member) throw std::invalid_argument("该成员已选择，无需重复输入。");
        members.push_back(member);
    }
    if (stopped_) return;
    output_ << "父群：" << parentNumber << " | 子群：" << number << " | 名称：" << name
            << " | 群主：" << owner << " | 成员数：" << members.size() + 1 << '\n';
    if (readChoice("确认创建子群？1 确认  0 取消\n> ", 1) == 0) return;
    session_.createSubgroup(parentNumber, number, name, owner, members);
    output_ << "子群已创建，群号：" << number << "。\n";
}
void ConsoleAppLJY::showSubgroups(int parentNumber)
{
    const auto groups = session_.subgroups(parentNumber);
    if (groups.empty()) { output_ << "当前群没有子群。\n"; return; }
    for (const auto* group : groups)
        output_ << "子群号：" << group->number() << " | 群名：" << group->name()
            << '\n';
}
void ConsoleAppLJY::switchGroupMode(int number)
{
    session_.checkGroupOperation(number, GroupOperationLJY::SwitchMode);
    const int choice = readChoice("目标管理模式：1 QQ  2 微信  0 返回\n> ", 2);
    if (stopped_ || choice == 0) return;
    const auto style = choice == 1 ? GroupStyleLJY::QQ : GroupStyleLJY::WeChat;
    if (session_.selectedGroup(number).style() == style) { output_ << "当前已经是该管理模式。\n"; return; }
    output_ << "切换保留当前群成员、管理员标记及本群请求，所属服务不变。\n";
    if (style == GroupStyleLJY::WeChat) output_ << "切为微信模式保留已有子群、成员和申请邀请；子群继续使用自身规则，但本群不能新建子群。当前群管理员权限暂停。\n";
    if (readChoice("确认切换？1 确认  0 取消\n> ", 1) == 0) return;
    session_.switchGroupMode(number, style);
    output_ << "群管理模式已切换为" << (style == GroupStyleLJY::QQ ? "QQ" : "微信") << "。\n";
}

void ConsoleAppLJY::friendRequests()
{
    const int mine=session_.currentAccount().accountId();
    const char* states[]={"待处理","已同意","已拒绝"};
    for (const auto& request : session_.friendRequests())
    {
        const bool incoming=request.recipientId()==mine;
        const auto peer=session_.contactAccount(incoming ? request.senderId() : request.recipientId());
        output_<<(incoming ? "收到：" : "发出：")<<peer.externalId()<<" | "<<peer.nickname()<<" | "<<states[request.state()]<<'\n';
    }
    std::string number;
    if (!readLine("处理收到的申请，输入发送人号码（0 返回）：",number)) return;
    const int choice=readChoice("1 同意  2 拒绝  0 返回\n> ",2);
    if (stopped_ || choice==0) return;
    session_.handleFriendRequest(number,choice==1);
    output_<<(choice==1 ? "已同意申请，双方已成为好友。\n" : "已拒绝申请。\n");
}
