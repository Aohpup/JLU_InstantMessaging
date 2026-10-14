#include "gui/MainWindowLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTest>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <stdexcept>
void check(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}
void waitFor(const std::function<bool()> &condition, const char *why, int ms = 12000)
{
    QElapsedTimer t;
    t.start();
    while (!condition() && t.elapsed() < ms)
        QTest::qWait(20);
    check(condition(), why);
}
void click(QWidget &w, const char *name)
{
    auto *b = w.findChild<QPushButton *>(name);
    check(b && b->isVisible() && b->isEnabled(), name);
    b->click();
    QApplication::processEvents();
}
void marker(const QString &path)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "marker");
    file.write("ok");
}
void choose(QWidget &w, const char *name, int number)
{
    auto *list = w.findChild<QListWidget *>(name);
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toInt() == number)
        {
            list->setCurrentRow(i);
            QApplication::processEvents();
            return;
        }
    throw std::runtime_error("group not found");
}
bool body(QWidget &w, const QString &text)
{
    for (auto *label : w.findChildren<QLabel *>("groupChatBody"))
        if (label->text() == text)
            return true;
    return false;
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguiemj.ttf");
    try
    {
        const auto args = app.arguments();
        check(args.size() == 7, "args: role service db port evidence group");
        const auto role = args[1], path = args[3], folder = args[5];
        const int service = args[2].toInt(), port = args[4].toInt(), group = args[6].toInt();
        const bool reload = role.startsWith("reload");
        const QString letter = reload ? role.right(1) : role;
        const QString accounts[3][3] = {
            {"10001", "10002", "10003"}, {"wx_20001", "wx_20002", "wx_20003"}, {"10001", "10004", "10005"}};
        const auto own = accounts[service][letter == "A" ? 0 : letter == "B" ? 1 : 2];
        const bool existed = QFile::exists(path);
        PlatformLJY platform;
        SqliteStorageLJY store(path.toStdString());
        if (existed)
            store.load(platform);
        else
        {
            loadDemoDataLJY(platform);
            if (group == 2001)
            {
                SessionManagerLJY setup(platform);
                setup.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
                setup.createSubgroup(1001, 2001, "群聊天验收子群", "10001", {"10002", "10003"});
            }
            store.save(platform);
        }
        QSettings settings(path + ".group.connection.ini", QSettings::IniFormat);
        settings.setValue(QString::number(service) + "/" + QString::number(group) + "/port", port);
        settings.sync();
        SessionManagerLJY session(platform);
        MainWindowLJY window(session, [&] { store.save(platform); }, path);
        window.show();
        QTest::qWait(60);
        window.findChild<QComboBox *>("loginService")->setCurrentIndex(service);
        window.findChild<QLineEdit *>("loginNumber")->setText(own);
        window.findChild<QLineEdit *>("loginPassword")->setText("DemoOnly123!");
        click(window, "loginButton");
        if (letter == "A")
        {
            click(window, "groupsNav");
            window.findChild<QLineEdit *>("groupNumber")->setText(QString::number(group));
            click(window, "findGroupButton");
            click(window, "groupMessageButton");
        }
        else
        {
            click(window, "messagesNav");
            window.findChild<QTabWidget *>("messageTabs")->setCurrentIndex(1);
            choose(window, "chatGroups", group);
        }
        auto *page = window.findChild<GroupMessagesPageLJY *>();
        check(page && page->isVisible(), "group tab not opened");
        auto *editor = window.findChild<QPlainTextEdit *>("groupChatEditor");
        auto *send = window.findChild<QPushButton *>("groupChatSend");
        if (reload)
        {
            check(window.findChildren<QLabel *>("groupChatBody").size() >= 3, "restart missing history");
            window.grab().save(folder + "/" + role + ".png");
            window.close();
            std::cout << "PASS reload " << letter.toStdString() << '\n';
            return 0;
        }
        waitFor([&] { return send->isEnabled(); }, "automatic enter not ready");
        marker(folder + "/" + letter + ".joined");
        waitFor([&] { return window.findChild<QLabel *>("groupChatOnline")->text() == "3 人在线"; },
                "three peers not online");
        const auto text = letter + "：三人即时群消息\n第二行🙂";
        editor->setPlainText(letter + "：三人即时群消息");
        editor->moveCursor(QTextCursor::End);
        QTest::keyClick(editor, Qt::Key_Return, Qt::ShiftModifier);
        editor->insertPlainText("第二行🙂");
        QTest::keyClick(editor, Qt::Key_Return);
        waitFor(
            [&] {
                return body(window, "A：三人即时群消息\n第二行🙂") &&
                       body(window, "B：三人即时群消息\n第二行🙂") &&
                       body(window, "C：三人即时群消息\n第二行🙂");
            },
            "three bodies missing");
        check(editor->toPlainText().isEmpty(), "send retained editor");
        marker(folder + "/" + letter + ".round1");
        waitFor(
            [&] {
                return QFile::exists(folder + "/A.round1") && QFile::exists(folder + "/B.round1") &&
                       QFile::exists(folder + "/C.round1");
            },
            "round1 barrier");
        if (letter == "C")
        {
            window.close();
            std::cout << "PASS ordinary member host C, close for takeover\n";
            return 0;
        }
        waitFor(
            [&] {
                return send->isEnabled() &&
                       window.findChild<QLabel *>("groupChatOnline")->text() == "2 人在线";
            },
            "GUI host takeover");
        editor->setPlainText(letter + "：自动接管后继续");
        QTest::keyClick(editor, Qt::Key_Return);
        waitFor([&] { return body(window, "A：自动接管后继续") && body(window, "B：自动接管后继续"); },
                "takeover message missing");
        // 页面隐藏仍能收到消息，且正常业务整体保存不清空群历史。
        click(window, "accountNav");
        QTest::qWait(60);
        click(window, "messagesNav");
        check(page->isVisible(), "group tab lost on navigation");
        click(window, "saveButton");
        window.resize(1100, 820);
        QTest::qWait(80);
        window.grab().save(folder + "/" + letter + "-chat.png");
        window.resize(900, 600);
        QTest::qWait(80);
        check(window.size() == QSize(900, 600), "group layout minimum");
        click(window, "groupChatSettings");
        window.grab().save(folder + "/" + letter + "-settings.png");
        click(window, "groupChatCancelSettings");
        marker(folder + "/" + letter + ".done");
        waitFor([&] { return QFile::exists(folder + "/A.done") && QFile::exists(folder + "/B.done"); },
                "done barrier");
        window.close();
        GroupChatHistoryLJY history(path);
        check(history.load(service, own, group).size() == 5, "history count or duplicates");
        std::cout << "PASS GUI " << letter.toStdString()
                  << " automatic enter, three peers, takeover, history, shortcuts and 900x600\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
