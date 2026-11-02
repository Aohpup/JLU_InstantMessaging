#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTest>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
static void input(QWidget& window, const char* name, const QString& value)
{
    auto* edit = window.findChild<QLineEdit*>(name);
    check(edit != nullptr, name);
    edit->setText(value);
}
static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isEnabled() && button->isVisible(), name);
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}
static void selectFriend(QWidget& window, const QString& number)
{
    auto* list = window.findChild<QListWidget*>("friendsList");
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == number)
        { list->setCurrentRow(i); return; }
    throw std::runtime_error("friend missing from GUI list");
}

// 两次独立进程之间由 Python 启动真实终端，只使用传入的隔离库。
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        check(app.arguments().size() == 3, "expected --write/--read and database path");
        const bool write = app.arguments()[1] == "--write";
        const auto path = std::filesystem::path(app.arguments()[2].toStdWString());
        PlatformLJY platform;
        SqliteStorageLJY storage(path.u8string());
        if (write) loadDemoDataLJY(platform); else storage.load(platform);
        SessionManagerLJY session(platform);
        MainWindowLJY window(session, [&] { storage.save(platform); }, app.arguments()[2]);
        window.show();
        input(window, "loginNumber", "10001");
        input(window, "loginPassword", write ? "DemoOnly123!" : "Roundtrip123!");
        click(window, "loginButton");
        check(session.isLoggedIn(), "fresh password login");
        check(session.boundQQNumber() == "10001", "QQ/WeChat binding preserved");
        if (write)
        {
            click(window, "profileButton");
            input(window, "profileLocation", "图形端城市");
            click(window, "avatarColor4");
            click(window, "profileButton");
            click(window, "passwordButton");
            input(window, "oldPassword", "DemoOnly123!");
            input(window, "newPassword", "Roundtrip123!");
            input(window, "confirmPassword", "Roundtrip123!");
            click(window, "passwordButton");
            check(session.currentPerson().location() == "图形端城市", "GUI profile committed");
        }
        else
        {
            check(session.currentPerson().location() == "终端城市", "terminal profile restored");
            check(session.currentAccount().nickname() == "终端昵称", "terminal nickname restored");
            check(window.findChild<QLabel*>("sidebarNickname")->text() == "终端昵称", "nickname displayed");
        }
        check(session.currentPerson().avatarColor() == 4, "avatar color retained");
        click(window, "friendsNav");
        selectFriend(window, "10002");
        if (write)
        {
            click(window, "friendRemarkButton");
            input(window, "friendRemark", "图形端备注");
            click(window, "saveRemarkButton");
            input(window, "addFriendNumber", "10005");
            answerDialogLJY(QMessageBox::Yes);
            click(window, "addFriendButton");
            check(session.friendRemark("10002") == "图形端备注", "GUI remark committed");
            check(session.friendRequests().size() == 1, "GUI request sent");
        }
        else
        {
            check(session.friendRemark("10002") == "终端备注", "terminal remark restored");
            selectFriend(window, "10005");
            check(session.friendAccount("10005").externalId() == "10005", "terminal approval restored");
        }
        click(window, "groupsNav");
        input(window, "groupNumber", "1001");
        click(window, "findGroupButton");
        window.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(2);
        if (write)
        {
            window.findChild<QComboBox*>("groupMode")->setCurrentIndex(1);
            answerDialogLJY(QMessageBox::Yes);
            click(window, "switchGroupModeButton");
        }
        check(session.selectedGroup(1001).style() == (write ? GroupStyleLJY::WeChat : GroupStyleLJY::QQ), "group mode roundtrip");
        if (!write)
        {
            answerDialogLJY(QMessageBox::Yes);
            click(window, "switchService1");
            check(session.currentAccount().externalId() == "wx_20001", "bound service available");
        }
        /* 业务操作成功后自动保存。 */
        window.close();
        check(!window.isVisible(), "saved close");
        std::cout << (write ? "PASS GUI write\n" : "PASS GUI reload after actual terminal edits\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
