#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/GroupsPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <sqlite3.h>
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isVisible() && button->isEnabled(), name);
    for (auto* scroll : window.findChildren<QScrollArea*>())
        if (scroll->isAncestorOf(button)) scroll->ensureWidgetVisible(button);
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}
static void answer(QMessageBox::StandardButton choice) { answerDialogLJY(choice); }
// 仅在隔离库安装失败触发器，验证真实保存事务回滚，不向正式程序加入故障开关。
static void execute(const std::filesystem::path& path, const char* sql)
{
    sqlite3* db = nullptr;
    check(sqlite3_open(path.u8string().c_str(), &db) == SQLITE_OK, "test database open");
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    check(result == SQLITE_OK, "test trigger operation");
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        const auto directory = std::filesystem::u8path(JLU_GUI_TEST_DIR);
        std::filesystem::create_directories(directory);
        const auto path = directory / "subgroups.db";
        const bool reload = app.arguments().contains("--reload");
        if (!reload) std::filesystem::remove(path);
        PlatformLJY platform;
        SqliteStorageLJY storage(path.u8string());
        if (reload) storage.load(platform); else loadDemoDataLJY(platform);
        SessionManagerLJY session(platform);
        MainWindowLJY window(session, [&] { storage.save(platform); }, QString::fromStdWString(path.wstring()));
        window.show();
        auto input = [&](const char* name, const QString& value) { window.findChild<QLineEdit*>(name)->setText(value); };
        input("loginNumber", "10001"); input("loginPassword", "DemoOnly123!");
        click(window, "loginButton"); click(window, "groupsNav");
        auto select = [&](int number) { input("groupNumber", QString::number(number)); click(window, "findGroupButton"); };
        auto* tabs = window.findChild<QTabWidget*>("groupTabs");
        auto* choices = window.findChild<QListWidget*>("subgroupMembers");
        auto* owner = window.findChild<QComboBox*>("subgroupOwner");
        auto* children = window.findChild<QListWidget*>("subgroupsList");
        auto* mode = window.findChild<QComboBox*>("groupMode");
        auto* status = window.findChild<QLabel*>("saveStatus");
        auto group = [&](int number) { return platform.findGroup(ServiceTypeLJY::QQ, number); };
        auto id = [&](const char* number) { return platform.findAccount(ServiceTypeLJY::QQ, number)->accountId(); };
        auto shot = [&](const char* name) {
            app.processEvents(); window.grab().save(QString::fromStdWString((directory / name).wstring()));
        };
        auto begin = [&](int number) {
            select(1001); tabs->setCurrentIndex(2); click(window, "createSubgroupButton");
            input("subgroupNumber", QString::number(number)); input("subgroupName", "课程学习小组");
        };
        auto member = [&](const QString& number) {
            for (int row = 0; row < choices->count(); ++row)
                if (choices->item(row)->data(Qt::UserRole).toString() == number) return choices->item(row);
            throw std::runtime_error("member choice missing");
        };
        auto saveChild = [&] { answer(QMessageBox::Yes); click(window, "saveSubgroupButton"); };
        select(1001); tabs->setCurrentIndex(2);
        if (reload)
        {
            check(group(1001)->style() == GroupStyleLJY::QQ && group(2001) && group(2002), "retained children restored after reload");
            check(group(2003) && group(2003)->parentGroupId() == group(1001)->groupId(), "child relation persisted");
            check(group(2003)->ownerAccountId() == id("10002") && group(2003)->findMember(id("10004")), "chosen owner and member persisted");
            check(!group(2004) && session.groupRequests(2001).size()==1, "discarded child absent and retained requests present");
            check(children->count() == 3, "reloaded children visible");
            window.close(); std::cout << "PASS: subgroup hierarchy, owner, members, preservation and discard restored in separate process\n"; return 0;
        }
        begin(2001);
        check(owner->count() == 5 && choices->count() == 5, "only parent members offered");
        owner->setCurrentIndex(owner->findData("10002"));
        check(member("10002")->checkState() == Qt::Checked && !(member("10002")->flags() & Qt::ItemIsEnabled), "chosen owner automatically joined");
        owner->setCurrentIndex(owner->findData("10001"));
        // 用鼠标验证选中、取消及群主锁定，保存三种状态的实际控件截图。
        auto toggleMember = [&](const QString& number) {
            auto* item=member(number); choices->scrollToItem(item); app.processEvents();
            const auto rect=choices->visualItemRect(item);
            QTest::mouseClick(choices->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(rect.left()+16,rect.center().y()));
        };
        toggleMember("10003"); check(member("10003")->checkState()==Qt::Checked,"mouse checks member");
        toggleMember("10003"); check(member("10003")->checkState()==Qt::Unchecked,"mouse unchecks member");
        toggleMember("10001"); check(member("10001")->checkState()==Qt::Checked,"owner stays checked");
        toggleMember("10003"); toggleMember("10004");
        choices->scrollToTop(); app.processEvents();
        choices->grab().save(QString::fromStdWString((directory / "子群勾选框状态.png").wstring()));
        shot("群_子群勾选框.png");
        input("subgroupNumber", "20x"); click(window, "saveSubgroupButton");
        check(status->styleSheet().contains("#fdeaea") && window.findChild<QLineEdit*>("subgroupName")->text() == "课程学习小组", "invalid input red and draft preserved");
        input("subgroupNumber", "2001");
        answer(QMessageBox::No); click(window, "saveSubgroupButton"); check(!group(2001), "cancel create unchanged");
        window.resize(900,600); app.processEvents();
        for (auto* scroll : window.findChildren<QScrollArea*>()) if (scroll->widget()->objectName() == "groupsPage")
        {
            scroll->ensureWidgetVisible(window.findChild<QPushButton*>("saveSubgroupButton"));
            check(scroll->horizontalScrollBar()->maximum() == 0, "small-window subgroup editor fits width");
        }
        shot("群_子群创建_小窗口.png");
        window.resize(1100,780); saveChild();
        check(group(2001) && group(2001)->members().size() == 3 && children->count() == 1, "created child reflected in list");
        children->setCurrentRow(0); click(window, "openSubgroupButton");
        click(window, "createSubgroupButton"); check(status->styleSheet().contains("#fdeaea") && !window.findChild<QWidget*>("subgroupEditor")->isVisible(), "no nested child and red reason");
        click(window, "parentGroupButton");
        // 父群移除普通成员，子群资格同步消失。
        tabs->setCurrentIndex(0);
        auto* members = window.findChild<QTableWidget*>("groupMembers");
        for (int row = 0; row < members->rowCount(); ++row)
            if (members->item(row,0)->data(Qt::UserRole).toString() == "10003") members->selectRow(row);
        answer(QMessageBox::Yes); click(window,"groupMemberAction");
        check(!group(1001)->findMember(id("10003")) && !group(2001)->findMember(id("10003")), "parent removal cascades to child");
        SessionManagerLJY applicant(platform); applicant.login(ServiceTypeLJY::QQ, "10005", "DemoOnly123!");
        applicant.applyToGroup(2001);
        // 已通过 UI 自动保存。
        { PlatformLJY saved; SqliteStorageLJY reader(path.u8string()); reader.load(saved);
          check(saved.findGroup(ServiceTypeLJY::QQ,2001)->members().size() == 2, "child membership saved"); }
        tabs->setCurrentIndex(2); shot("群_子群与模式.png");
        const auto parentCount = group(1001)->members().size();
        mode->setCurrentIndex(1); answer(QMessageBox::No); click(window,"switchGroupModeButton");
        check(group(2001) && !session.groupRequests(2001).empty() && mode->currentIndex() == 0, "cancel mode preserves child and requests");
        QString confirmation;
        mode->setCurrentIndex(1);
        QTimer::singleShot(0,[&] {
            auto* box = activeDialogLJY();
            if (box) { confirmation = box->findChild<QLabel*>("dialogMessage")->text(); box->grab().save(QString::fromStdWString((directory/"群_模式切换确认.png").wstring())); dialogButtonLJY(box,QMessageBox::Yes)->click(); }
        });
        click(window,"switchGroupModeButton");
        check(confirmation.contains("子群") && confirmation.contains("保留"), "confirmation explains subgroup preservation");
        check(group(2001) && session.groupRequests(2001).size()==1 && group(1001)->members().size() == parentCount, "WeChat preserves children, requests and parent");
        mode->setCurrentIndex(0); answer(QMessageBox::Yes); click(window,"switchGroupModeButton");
        check(group(2001) && children->count() == 1, "QQ preserves same child");
        // 当前子群在父群切换后仍保持选中，不再需要释放回退流程。
        begin(2002); saveChild(); select(2002);
        session.switchGroupMode(1001, GroupStyleLJY::WeChat);
        click(window,"refreshGroupsButton");
        check(group(2002) && window.findChild<QLabel*>("groupSummary")->text().contains("2002"), "selected child survives parent mode switch");
        click(window,"parentGroupButton");
        mode->setCurrentIndex(0); answer(QMessageBox::Yes); click(window,"switchGroupModeButton");
        // 已通过 UI 自动保存。
        const char* block = "CREATE TRIGGER p4_fail BEFORE DELETE ON chat_groups BEGIN SELECT RAISE(ABORT,'P4 save failure'); END;";
        execute(path, block);
        begin(2003); owner->setCurrentIndex(owner->findData("10002")); member("10004")->setCheckState(Qt::Checked); saveChild();
        check(status->text().contains("保存失败") && status->styleSheet().contains("#fdeaea") && group(2003), "actual SQLite save failure preserves memory and red footer");
        { PlatformLJY saved; SqliteStorageLJY reader(path.u8string()); reader.load(saved); check(!saved.findGroup(ServiceTypeLJY::QQ,2003), "failed transaction leaves database unchanged"); }
        window.resize(900,600); app.processEvents(); shot("群_保存失败_小窗口.png");
        const QRect footer(status->mapTo(&window,QPoint(0,0)),status->size());
        check(window.rect().contains(footer), "fixed error footer fully visible in small window");
        answer(QMessageBox::Cancel); window.close(); check(window.isVisible() && group(2003), "cancel close keeps window and memory");
        QTimer::singleShot(0,[&] {
            if (auto* box = activeDialogLJY()) { execute(path,"DROP TRIGGER p4_fail;"); dialogButtonLJY(box,QMessageBox::Retry)->click(); }
        });
        window.close(); check(!window.isVisible(), "retry success closes window");
        { PlatformLJY saved; SqliteStorageLJY reader(path.u8string()); reader.load(saved); check(saved.findGroup(ServiceTypeLJY::QQ,2003), "retry saves child"); }
        window.show(); execute(path,block); begin(2004); saveChild();
        answer(QMessageBox::Discard); window.close(); check(!window.isVisible(), "explicit discard closes window");
        execute(path,"DROP TRIGGER p4_fail;");
        { PlatformLJY saved; SqliteStorageLJY reader(path.u8string()); reader.load(saved); check(saved.findGroup(ServiceTypeLJY::QQ,2003) && !saved.findGroup(ServiceTypeLJY::QQ,2004), "discard preserves last successful database"); }
        std::cout << "PASS: child creation, parent cascade, mode preservation, selection, real SQLite rollback and all close choices\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
    return 0;
}
