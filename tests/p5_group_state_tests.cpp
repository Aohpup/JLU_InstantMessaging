#include "gui/MainWindowLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include "InlineDialogTestLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTemporaryDir>
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
void waitFor(const std::function<bool()> &f, const char *why)
{
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < 8000)
        QTest::qWait(10);
    check(f(), why);
}
void click(QWidget &w, const char *name)
{
    auto *b = w.findChild<QPushButton *>(name);
    check(b && b->isVisible() && b->isEnabled(), name);
    b->click();
    QApplication::processEvents();
}
void select(QWidget &w, int number)
{
    auto *list = w.findChild<QListWidget *>("chatGroups");
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toInt() == number)
        {
            list->setCurrentRow(i);
            QApplication::processEvents();
            return;
        }
    throw std::runtime_error("group");
}
int freePort()
{
    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 0);
    return server.serverPort();
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        QTemporaryDir temp;
        const auto path = temp.path() + "/state.db";
        PlatformLJY platform;
        loadDemoDataLJY(platform);
        SqliteStorageLJY store(path.toStdString());
        store.save(platform);
        SessionManagerLJY session(platform);
        MainWindowLJY w(session, [&] { store.save(platform); }, path);
        QSettings config(path + ".group.connection.ini", QSettings::IniFormat);
        const int port = freePort();
        config.setValue("0/1001/port", port);
        config.setValue("0/1002/port", freePort());
        config.sync();
        GroupChatHistoryLJY history(path);
        for (int i = 0; i < 205; ++i)
        {
            GroupChatMessageLJY m;
            m.id = "history" + QString::number(i);
            m.body = "历史 " + QString::number(i);
            m.sender = "10002";
            m.nickname = "B";
            m.time = i;
            history.save(0, "10001", 1001, m);
            if (i < 100) history.save(0, "10001", 1003, m);
        }
        w.show();
        QTest::qWait(50);
        w.findChild<QLineEdit *>("loginNumber")->setText("10001");
        w.findChild<QLineEdit *>("loginPassword")->setText("DemoOnly123!");
        click(w, "loginButton");
        click(w, "messagesNav");
        w.findChild<QTabWidget *>("messageTabs")->setCurrentIndex(1);
        select(w, 1001);
        auto *page = w.findChild<GroupMessagesPageLJY *>();
        auto *send = w.findChild<QPushButton *>("groupChatSend");
        waitFor([&] { return send->isEnabled(); }, "auto host");
        check(w.findChildren<QLabel *>("groupChatBody").size() == 100, "first history page");
        click(w, "groupChatEarlier");
        check(w.findChildren<QLabel *>("groupChatBody").size() == 200, "older page");
        QTest::qWait(80);
        check(w.findChild<QScrollArea *>("groupChatScroll")->verticalScrollBar()->value() == 0,
              "older messages loaded but not brought into view");
        click(w, "groupChatEarlier");
        check(w.findChildren<QLabel *>("groupChatBody").size() == 205, "pagination duplicated");
        QTest::qWait(80);
        auto *earlierButton = w.findChild<QPushButton *>("groupChatEarlier");
        check(!earlierButton->isEnabled() && earlierButton->text() == "已到最早消息", "history end unclear");
        check(w.findChild<QLabel *>("saveStatus")->text().contains("5 条"), "page count feedback missing");
        auto *historyScroll = w.findChild<QScrollArea *>("groupChatScroll");
        auto *firstBody = w.findChildren<QLabel *>("groupChatBody").front();
        check(firstBody->text() == "历史 0" && historyScroll->viewport()->rect().contains(
            firstBody->mapTo(historyScroll->viewport(), QPoint(1, 1))), "oldest body not visible");
        GroupChatMembersLJY members{{"10001", {"A", 0}}, {"10002", {"B", 1}}, {"10003", {"C", 2}}};
        GroupChatClientLJY b, c;
        b.enter("0:1001", "10002", members, port);
        c.enter("0:1001", "10003", members, port);
        waitFor([&] { return b.ready() && c.ready(); }, "remote members");
        QString longText;
        for (int i = 0; i < 12; ++i)
            longText += "长消息第 " + QString::number(i) + " 行\n";
        b.sendText(longText);
        waitFor([&] { return w.findChildren<QLabel *>("groupChatBody").size() == 206; },
                "long message receive");
        QTest::qWait(50);
        QLabel *longBody = nullptr;
        for (auto *l : w.findChildren<QLabel *>("groupChatBody"))
            if (l->text() == longText)
                longBody = l;
        check(longBody, "long body");
        auto *expand = longBody->parentWidget()->findChild<QPushButton *>("chatExpand");
        check(expand->isVisible(), "expand missing");
        expand->click();
        QApplication::processEvents();
        check(longBody->height() >= longBody->heightForWidth(longBody->width()), "expand clipped");
        // 在账号页隐藏群聊天时，消息仍接收；写库失败保留并允许关闭取消。
        click(w, "accountNav");
        sqlite3 *lock = nullptr;
        sqlite3_open(path.toUtf8().constData(), &lock);
        sqlite3_exec(lock, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        b.sendText("保存失败也保留");
        waitFor([&] { return !w.findChild<QPushButton *>("groupChatRetryHistory")->isHidden(); },
                "pending retry not shown");
        click(w, "messagesNav");
        answerDialogLJY(QMessageBox::Cancel);
        w.close();
        check(w.isVisible(), "close cancel failed");
        sqlite3_exec(lock, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(lock);
        click(w, "groupChatRetryHistory");
        check(history.load(0, "10001", 1001, 0, 300).size() == 207, "pending save lost");
        b.leave();
        c.leave();
        page->openGroup(1001);
        waitFor([&] { return send->isEnabled(); }, "reopen after cancelled close");
        b.enter("0:1001", "10002", members, port);
        c.enter("0:1001", "10003", members, port);
        waitFor([&] { return b.ready() && c.ready(); }, "rejoin after close");
        session.kickGroupMember(1001, "10003");
        page->refresh();
        waitFor([&] { return !c.ready(); }, "business removal not applied to room");
        session.switchGroupMode(1001, GroupStyleLJY::WeChat);
        page->refresh();
        check(b.ready() && send->isEnabled(), "mode change disconnected same group");
        select(w, 1003);
        check(w.findChildren<QLabel *>("groupChatBody").size() == 100 && !earlierButton->isEnabled(),
              "exactly 100 records incorrectly offers earlier page");
        select(w, 1002);
        check(w.findChildren<QLabel *>("groupChatBody").empty(), "other group saw old history");
        check(!earlierButton->isEnabled() && earlierButton->text().contains("本地记录已加载完"), "empty history unclear");
        select(w, 1001);
        check(w.findChildren<QLabel *>("groupChatBody").size() == 100, "return history latest page");
        /* 业务操作成功后自动保存。 */
        check(history.load(0, "10001", 1001, 0, 300).size() == 207, "platform save deleted group history");
        click(w, "logoutButton");
        check(!send->isEnabled(), "logout connection active");
        b.leave();
        c.leave();
        w.close();
        std::cout << "PASS GUI history paging/fold, hidden receive, write failure retry and cancel close, "
                     "membership removal, mode preservation, group isolation, logout\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
