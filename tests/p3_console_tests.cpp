#include "ui/ConsoleAppLJY.h"
#include "services/DemoDataLJY.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

// 同一保存动作用于两个菜单和退出；注销本身不保存。
void testSaveMenus()
{
    PlatformLJY platform; loadDemoDataLJY(platform); SessionManagerLJY session(platform);
    std::istringstream input("4\n1\n1\n10001\nDemoOnly123!\n6\n保存后的昵称\n10\n0\n0\n");
    std::ostringstream output;
    int saves = 0;
    ConsoleAppLJY app(session, input, output, [&] {
        ++saves;
        if (saves > 1) require(platform.findAccount(ServiceTypeLJY::QQ, "10001")->nickname() == "保存后的昵称", "Saving did not see latest data.");
    }, "test.db");
    require(app.run() == 0 && saves == 3 && !session.isLoggedIn(), "Menu/exit save count incorrect.");
    require(output.str().find("4 保存数据") != std::string::npos && output.str().find("10 保存数据") != std::string::npos,
        "Save entries missing.");
    require(output.str().find("退出后清空") == std::string::npos, "Persistent mode shown as memory-only.");
}

// 第一次保存失败后可在退出提示重试，也可返回菜单后再次退出。
void testRetryAndReturn()
{
    for (const char* script : {"0\n1\n", "0\n0\n0\n"})
    {
        PlatformLJY platform; SessionManagerLJY session(platform);
        std::istringstream input(script); std::ostringstream output; int saves = 0;
        ConsoleAppLJY app(session, input, output, [&] {
            if (++saves == 1) throw std::runtime_error("临时写入失败");
        }, "test.db");
        require(app.run() == 0 && saves == 2, "Retry or return flow failed.");
        require(output.str().find("保存失败：临时写入失败") != std::string::npos &&
            output.str().find("保存成功：test.db") != std::string::npos, "Save failure/success missing.");
    }
}

// 放弃必须选 2；输入已经结束且保存失败时返回错误，不无限等待或假报成功。
void testDiscardAndEof()
{
    for (const char* script : {"0\n2\n", ""})
    {
        PlatformLJY platform; SessionManagerLJY session(platform);
        std::istringstream input(script); std::ostringstream output; int saves = 0;
        ConsoleAppLJY app(session, input, output, [&] { ++saves; throw std::runtime_error("只读文件"); }, "test.db");
        const bool eof = std::string(script).empty();
        require(app.run() == (eof ? 1 : 0) && saves == 1, "Discard or EOF result incorrect.");
        require(output.str().find(eof ? "本次修改未保存，程序异常退出" : "已放弃未保存的修改") != std::string::npos,
            "Unsaved result not explained.");
        require(output.str().find("保存成功") == std::string::npos, "Failure falsely reported success.");
    }
    PlatformLJY platform; SessionManagerLJY session(platform);
    std::istringstream input(""); std::ostringstream output; int saves = 0;
    ConsoleAppLJY app(session, input, output, [&] { ++saves; }, "test.db");
    require(app.run() == 0 && saves == 1, "EOF did not attempt saving.");
}

int main()
{
    int failed = 0;
    auto test = [&](const char* name, void(*action)()) {
        try { action(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    test("explicit save menus and normal exit", testSaveMenus);
    test("save retry and return to menu", testRetryAndReturn);
    test("explicit discard and EOF results", testDiscardAndEof);
    std::cout << (3 - failed) << "/3 persistence menu scenario groups passed.\n";
    return failed ? 1 : 0;
}
