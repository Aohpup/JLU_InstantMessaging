#include "gui/MainWindowLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include "storage/DefaultDatabasePathLJY.h"
#include <QApplication>
#include "gui/InlineDialogLJY.h"
#include <filesystem>
#include <memory>
#include <stdexcept>

// 独立图形入口；Qt 管理事件循环，平台、存储和会话的寿命覆盖整个窗口。
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    // 统一默认字体，表单、下拉列表和局部样式都沿用微软雅黑。
    QFont font("Microsoft YaHei"); font.setPixelSize(13); font.setWeight(QFont::Normal);
    app.setFont(font);
    // 固定浅色系统调色板，避免系统深色模式与本阶段浅色样式混用。
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f5f8f8"));
    palette.setColor(QPalette::WindowText, QColor("#233c42"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, QColor("#233c42"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#233c42"));
    palette.setColor(QPalette::Highlight, QColor("#247d6b"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    try
    {
        const auto args = app.arguments();
        const auto option = args.value(1);
        const bool empty = args.size() == 2 && option == "--empty";
        const bool demo = args.size() == 2 && option == "--demo";
        const bool customDatabase = args.size() == 3 && option == "--database";
        const bool help = args.size() == 2 && option == "--help";
        const QString usage = "用法：jlu_im_gui.exe [--empty | --demo | --database 路径 | --help]\n"
            "无参数使用默认数据库；新数据库首次写入演示数据。\n"
            "--demo 为演示内存模式；--empty 为空平台内存模式。\n"
            "演示 QQ：10001，密码：DemoOnly123!\n"
            "不要与终端同时编辑同一个数据库。\n默认库：" + QString::fromStdWString(defaultDatabasePathLJY().wstring());
        if (args.size() != 1 && !empty && !demo && !customDatabase && !help)
            throw std::invalid_argument(usage.toUtf8().toStdString());
        if (help) { InlineDialogLJY::ask(nullptr, "使用说明", usage, {InlineDialogLJY::Ok}); return 0; }
        if (customDatabase && args[2].isEmpty()) throw std::invalid_argument("数据库路径不能为空。");
        PlatformLJY platform;
        std::unique_ptr<SqliteStorageLJY> storage;
        QString databasePath;
        if (empty || demo)
        {
            if (demo) loadDemoDataLJY(platform);
        }
        else
        {
            // Qt 参数按 Unicode 读取，传给 SQLite 前才转换为 UTF-8。
            const auto path = std::filesystem::absolute(customDatabase ?
                std::filesystem::path(args[2].toStdWString()) : defaultDatabasePathLJY()).lexically_normal();
            databasePath = QString::fromStdWString(path.wstring());
            const bool newDatabase = !std::filesystem::exists(path);
            storage = std::make_unique<SqliteStorageLJY>(path.u8string());
            if (newDatabase) { loadDemoDataLJY(platform); storage->save(platform); }
            else storage->load(platform);
        }
        SessionManagerLJY session(platform);
        std::function<void()> saveAction;
        if (storage) saveAction = [&] { storage->save(platform); };
        MainWindowLJY window(session, saveAction, databasePath);
        window.show();
        return app.exec();
    }
    catch (const std::exception& error)
    {
        InlineDialogLJY::ask(nullptr, "启动失败", QString::fromUtf8(error.what()), {InlineDialogLJY::Ok});
        return 1;
    }
}
