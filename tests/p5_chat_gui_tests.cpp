#include "gui/MessagesPageLJY.h"
#include "gui/MainWindowLJY.h"
#include "InlineDialogTestLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QFile>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTest>
#include <sqlite3.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void waitFor(const std::function<bool()>& condition, const char* reason, int timeout = 8000)
{
    QElapsedTimer elapsed; elapsed.start();
    while (!condition() && elapsed.elapsed() < timeout) QTest::qWait(20);
    check(condition(), reason);
}
void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isEnabled() && button->isVisible(), name);
    QTest::mouseClick(button, Qt::LeftButton); QApplication::processEvents();
}
void input(QWidget& window, const char* name, const QString& text)
{
    auto* field = window.findChild<QLineEdit*>(name); check(field, name); field->setText(text);
}
void select(QWidget& window, const char* name, const QString& number)
{
    auto* list = window.findChild<QListWidget*>(name); check(list, name);
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == number)
        { list->setCurrentRow(i); QApplication::processEvents(); return; }
    throw std::runtime_error("Friend not found");
}
bool containsBody(QWidget& window, const QString& text)
{
    for (auto* label : window.findChildren<QLabel*>("chatBody"))
        if (label->text() == text) { check(label->textFormat() == Qt::PlainText, "Body must be plain text"); return true; }
    return false;
}
void marker(const QString& path)
{
    QFile file(path); check(file.open(QIODevice::WriteOnly), "marker write"); file.write("ok");
}

// 每个进程创建生产 MainWindow 并点击真实控件，两端只通过 Socket 交换消息。
int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    // offscreen 插件不会自动发现 Windows 字体，显式加载以便检查真实中文字形。
    check(QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc") >= 0, "Microsoft YaHei unavailable");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguiemj.ttf");
    QFont font("Microsoft YaHei"); font.setPixelSize(13); app.setFont(font);
    try
    {
        const auto args = app.arguments();
        check(args.size() >= 3, "Expected arguments");
        if (args[1] == "--prepare")
        {
            PlatformLJY base; loadDemoDataLJY(base);
            SqliteStorageLJY storage(args[2].toUtf8().toStdString()); storage.save(base);
            return 0;
        }
        check(args.size() == 5, "Expected role database port evidence-directory");
        const bool a = args[1] == "--listen" || args[1] == "--reload-a";
        const int service = qEnvironmentVariableIntValue("JLU_TEST_SERVICE");
        const QString first = service == 1 ? "wx_20001" : "10001";
        const QString second = service == 1 ? "wx_20003" : (service == 2 ? "10005" : "10002");
        const QString own = a ? first : second, other = a ? second : first;
        const QString folder = args[4], role = a ? "A" : "B";
        PlatformLJY platform; SqliteStorageLJY storage(args[2].toUtf8().toStdString()); storage.load(platform);
        SessionManagerLJY session(platform);
        MainWindowLJY window(session, [&] { storage.save(platform); }, args[2]);
        window.show(); QTest::qWait(80);
        window.findChild<QComboBox*>("loginService")->setCurrentIndex(service);
        input(window, "loginNumber", own); input(window, "loginPassword", "DemoOnly123!"); click(window, "loginButton");
        check(session.isLoggedIn(), "Login failed");
        if (a)
        {
            click(window, "friendsNav"); select(window, "friendsList", other); click(window, "friendMessageButton");
        }
        else { click(window, "messagesNav"); select(window, "chatFriends", other); }
        check(window.findChild<MessagesPageLJY*>()->isVisible(), "Friend shortcut did not open messages");
        check(window.findChild<QLabel*>("chatPeerTitle")->text().contains(other), "Selected wrong friend");
        auto* editor = window.findChild<QPlainTextEdit*>("chatEditor");
        auto* send = window.findChild<QPushButton*>("chatSend");
        check(!send->isEnabled(), "Send enabled before handshake");
        if (args[1].startsWith("--reload"))
        {
            check(window.findChildren<QLabel*>("chatBody").size() == 6, "Restart lost saved messages");
            int received = 0;
            for (auto* label : window.findChildren<QLabel*>("chatDelivery")) received += label->text().contains("对端已接收");
            check(received == 3, "Restart lost ACK status");
            QTest::qWait(60);
            for (auto* body : window.findChildren<QLabel*>("chatBody"))
                check(body->height() >= body->heightForWidth(body->width()), "Restored message text clipped");
            window.grab().save(folder + "/" + role + "-reopened.png");
            click(window, "saveButton"); window.close();
            std::cout << "PASS reopened GUI " << role.toStdString() << ": six messages restored without connecting" << std::endl;
            return 0;
        }
        sqlite3* lock = nullptr;
        click(window, "chatSettings"); if (qEnvironmentVariable("JLU_TEST_ADDRESS", "127.0.0.1") != "127.0.0.1") window.findChild<QComboBox*>("chatMode")->setCurrentIndex(1); input(window, "chatAddress", qEnvironmentVariable("JLU_TEST_ADDRESS", "127.0.0.1"));
        window.findChild<QSpinBox*>("chatPort")->setValue(args[3].toInt());
        click(window, "chatSaveSettings"); click(window, "chatStart");
        if (a)
        {
            waitFor([&] { return window.findChild<QLabel*>("chatState")->text().contains("等待连接"); }, "Listener never ready");
            window.resize(900, 600); QTest::qWait(80);
            check(window.width() == 900 && window.height() == 600, "Connection panel expands minimum window size");
            check(window.findChild<QPushButton*>("chatStart")->height() >= 36, "Connection button text clipped");
            check(window.grab().save(folder + "/connection-small.png"), "Panel screenshot failed");
            std::cout << "LISTENING" << std::endl;
        }
        waitFor([&] { return send->isEnabled(); }, "Handshake did not enable GUI send");
        check(!window.findChild<QWidget*>("chatConnectionPanel")->isVisible(), "Settings did not collapse after handshake");
        if (a)
        {
            editor->setPlainText(" \n "); click(window, "chatSend");
            check(editor->toPlainText() == " \n ", "Rejected message discarded input");
            check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"), "Failure not in fixed red feedback");
        }
        const QString firstA = "来自 A 的中文消息 <b>保持原文</b>\nShift+Enter 保留换行 🙂";
        const QString firstB = "来自 B 的回复\n切到其他页面也能收到。";
        const QString secondA = "回到消息页后，继续发送第二条。";
        const QString secondB = "收到。";
        if (a)
        {
            editor->setPlainText("来自 A 的中文消息 <b>保持原文</b>");
            editor->moveCursor(QTextCursor::End); QTest::keyClick(editor, Qt::Key_Return, Qt::ShiftModifier);
            editor->insertPlainText("Shift+Enter 保留换行 🙂");
            check(editor->toPlainText() == firstA, "Shift+Enter did not insert newline");
            QTest::keyClick(editor, Qt::Key_Return);
            check(editor->toPlainText().isEmpty(), "Enter did not submit");
            waitFor([&] {
                for (auto* label : window.findChildren<QLabel*>("chatDelivery"))
                    if (label->text().contains("对端已接收")) return true;
                return false;
            }, "First ACK before lock missing");
            check(sqlite3_open(args[2].toUtf8().constData(), &lock) == SQLITE_OK, "Open isolated lock");
            check(sqlite3_exec(lock, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "Acquire isolated write lock");
            click(window, "accountNav"); marker(folder + "/A-hidden.ready");
            waitFor([&] { return containsBody(window, firstB); }, "Hidden page did not receive B message");
            check(!window.findChild<MessagesPageLJY*>()->isVisible(), "Hidden-page check ran on visible page");
            check(window.findChild<QLabel*>("saveStatus")->text().contains("消息已收到，记录保存失败"), "History failure confused with network failure");
            click(window, "messagesNav");
            check(window.findChild<QPushButton*>("chatRetryHistory")->isVisible(), "Retry entry missing");
            check(containsBody(window, firstA) && containsBody(window, firstB), "Navigation lost messages");
            editor->setPlainText(secondA); click(window, "chatSend");
        }
        else
        {
            waitFor([&] { return QFile::exists(folder + "/A-hidden.ready"); }, "A did not navigate away");
            editor->setPlainText(firstB); click(window, "chatSend");
            waitFor([&] { return containsBody(window, secondA); }, "Second A message missing");
            editor->setPlainText(secondB); QTest::keyClick(editor, Qt::Key_Return);
        }
        waitFor([&] { return containsBody(window, firstA) && containsBody(window, firstB) && containsBody(window, secondA) && containsBody(window, secondB); }, "Message body mismatch");
        waitFor([&] {
            int confirmed = 0;
            for (auto* label : window.findChildren<QLabel*>("chatDelivery")) confirmed += label->text().contains("对端已接收");
            return confirmed == 2;
        }, "ACK did not update two outgoing bubbles");
        if (a)
        {
            window.grab().save(folder + "/history-write-failed.png");
            answerDialogLJY(QMessageBox::Cancel); window.close();
            check(window.isVisible(), "Unsaved history cancel did not keep window open");
            check(window.findChild<QPushButton*>("chatRetryHistory")->isVisible(), "Cancelled close lost pending records");
            sqlite3_exec(lock, "ROLLBACK", nullptr, nullptr, nullptr); sqlite3_close(lock); lock = nullptr;
            click(window, "chatRetryHistory");
            check(!window.findChild<QPushButton*>("chatRetryHistory")->isVisible(), "Retry did not flush pending records");
            click(window, "saveButton");
        }
        // 两端保持现有历史，手动重新监听/连接，再互发一条。
        marker(folder + "/" + role + "-first-round.ready");
        waitFor([&] { return QFile::exists(folder + "/" + (a ? "B" : "A") + "-first-round.ready"); }, "First round not finished");
        if (!window.findChild<QWidget*>("chatConnectionPanel")->isVisible()) click(window, "chatSettings");
        if (a)
        {
            click(window, "chatStart");
            waitFor([&] { return window.findChild<QLabel*>("chatState")->text().contains("等待连接"); }, "Reconnect listener not ready");
            marker(folder + "/relisten.ready");
        }
        else
        {
            waitFor([&] { return QFile::exists(folder + "/relisten.ready") && !window.findChild<QPushButton*>("chatDisconnect")->isEnabled(); }, "Old connection not stopped");
            click(window, "chatStart");
        }
        waitFor([&] { return send->isEnabled(); }, "Manual reconnect handshake");
        editor->setPlainText(role + "：手动重连后的消息"); click(window, "chatSend");
        waitFor([&] { return window.findChildren<QLabel*>("chatBody").size() == 6; }, "Reconnect lost/duplicated messages");
        waitFor([&] { int n=0; for (auto* label : window.findChildren<QLabel*>("chatDelivery")) n += label->text().contains("对端已接收"); return n==3; }, "Reconnect ACK missing");
        window.resize(1280, 820); QTest::qWait(100);
        check(window.grab().save(folder + "/" + role + "-wide.png"), "Wide screenshot failed");
        window.resize(900, 600); QTest::qWait(100);
        check(window.width() == 900 && window.height() == 600, "Chat minimum size expanded");
        check(window.rect().contains(send->mapTo(&window, send->rect().bottomRight())), "Send button clipped");
        auto* status = window.findChild<QLabel*>("saveStatus");
        check(window.rect().contains(status->mapTo(&window, status->rect().bottomRight())), "Fixed status clipped");
        check(window.findChild<QScrollArea*>("chatScroll")->horizontalScrollBar()->maximum() == 0, "Chat horizontal overflow");
        auto* bar = window.findChild<QScrollArea*>("chatScroll")->verticalScrollBar();
        check(bar->value() == bar->maximum(), "Resizing lost the latest message");
        check(window.grab().save(folder + "/" + role + "-small.png"), "Small screenshot failed");
        marker(folder + "/" + role + "-done.ready");
        waitFor([&] { return QFile::exists(folder + "/" + (a ? "B" : "A") + "-done.ready"); }, "Other GUI not finished");
        QTest::qWait(150);
        if (a)
        {
            click(window, "logoutButton");
            input(window, "loginNumber", other); input(window, "loginPassword", "DemoOnly123!"); click(window, "loginButton");
            click(window, "messagesNav"); select(window, "chatFriends", own);
            check(window.findChildren<QLabel*>("chatBody").empty(), "Old account chat leaked after login change");
            answerDialogLJY(QMessageBox::Yes); click(window, service == 0 ? "switchService1" : "switchService0");
            check(window.findChildren<QLabel*>("chatBody").empty(), "History leaked after service change");
            check(!send->isEnabled(), "Old connection survived service change");
        }
        window.close(); check(!window.isVisible(), "Window did not close normally");
        std::cout << "PASS GUI " << role.toStdString() << ": login, selection, two sent/two received, ACK, page navigation, resize, close" << std::endl;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << std::endl; return 1; }
}
