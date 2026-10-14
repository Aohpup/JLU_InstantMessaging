#include "ui/ConsoleAppLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include "storage/DefaultDatabasePathLJY.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

// Ctrl+C 由本程序忽略；终端选中文本时的复制仍由终端自身处理。
static BOOL WINAPI ignoreCtrlC(DWORD event)
{
    return event == CTRL_C_EVENT ? TRUE : FALSE;
}

// Windows 宽字符入口保留中文路径，交给 SQLite 前再转成 UTF-8。
int wmain(int argc, wchar_t* argv[])
{
    const UINT inputCodePage = GetConsoleCP();
    const UINT outputCodePage = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    int result = 0;
    bool handlerInstalled = false;
    try
    {
        const std::wstring option = argc > 1 ? argv[1] : L"";
        const bool empty = argc == 2 && option == L"--empty";
        const bool demo = argc == 2 && option == L"--demo";
        const bool customDatabase = argc == 3 && option == L"--database";
        const bool help = argc == 2 && option == L"--help";
        if (argc != 1 && !empty && !demo && !customDatabase && !help)
            throw std::invalid_argument("用法：jlu_im.exe [--empty | --demo | --database 路径 | --help]");
        if (customDatabase && std::wstring(argv[2]).empty())
            throw std::invalid_argument("数据库路径不能为空。");
        if (help)
        {
            std::cout << "用法：jlu_im.exe [--empty | --demo | --database 路径 | --help]\n"
                << "无参数：使用默认数据库；--database：使用指定数据库。新库首次写入演示数据。\n"
                << "--empty：空平台内存模式；--demo：演示内存模式；两者均不读写数据库。\n"
                << "默认数据库：" << defaultDatabasePathLJY().u8string() << '\n';
        }
        else
        {
            if (!SetConsoleCtrlHandler(ignoreCtrlC, TRUE))
                throw std::runtime_error("无法启用 Ctrl+C 保护。");
            handlerInstalled = true;
            PlatformLJY platform;
            std::unique_ptr<SqliteStorageLJY> storage;
            std::string databasePath;
            if (empty || demo)
            {
                if (demo)
                {
                    loadDemoDataLJY(platform);
                    showDemoAccountsLJY(platform, std::cout);
                }
            }
            else
            {
                const auto path = std::filesystem::absolute(customDatabase ? std::filesystem::path(argv[2]) :
                    defaultDatabasePathLJY()).lexically_normal();
                databasePath = path.u8string();
                std::cout << "数据库位置：" << databasePath << '\n';
                // 在打开前区分新文件；已有空库也只读取，不能按人员数量重复初始化。
                const bool newDatabase = !std::filesystem::exists(path);
                storage = std::make_unique<SqliteStorageLJY>(databasePath);
                if (newDatabase)
                {
                    loadDemoDataLJY(platform);
                    storage->save(platform);
                    std::cout << "新数据库已创建，演示数据已保存。\n";
                    showDemoAccountsLJY(platform, std::cout);
                }
                else
                {
                    storage->load(platform);
                    std::cout << "已读取数据库：" << platform.personCount() << " 名人员，"
                        << platform.accountCount() << " 个账号，" << platform.groupCount() << " 个群。请重新登录。\n";
                }
            }
            SessionManagerLJY session(platform);
            // 菜单只调用保存动作；SQL 和数据库对象仍由存储层管理。
            std::function<void()> saveAction;
            if (storage) saveAction = [&] { storage->save(platform); };
            ConsoleAppLJY app(session, std::cin, std::cout, saveAction, databasePath);
            result = app.run();
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "程序启动失败：" << error.what() << '\n';
        result = 1;
    }
    if (handlerInstalled) SetConsoleCtrlHandler(ignoreCtrlC, FALSE);
    if (inputCodePage != 0) SetConsoleCP(inputCodePage);
    if (outputCodePage != 0) SetConsoleOutputCP(outputCodePage);
    return result;
}
