#include "gui/MainWindowLJY.h"
#include "gui/MessagesPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <functional>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void waitFor(const std::function<bool()>& condition, const char* why, int ms = 7000)
{
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < ms) QTest::qWait(20);
    check(condition(), why);
}
void click(QWidget& w, const char* name)
{
    auto* button = w.findChild<QPushButton*>(name);
    check(button && button->isVisible() && button->isEnabled(), name);
    button->click(); QApplication::processEvents();
}
void select(QWidget& w, const QString& number)
{
    auto* list = w.findChild<QListWidget*>("chatFriends");
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == number) { list->setCurrentRow(i); QApplication::processEvents(); return; }
    throw std::runtime_error("friend not found");
}
void login(QWidget& w, const QString& own, const QString& other)
{
    w.findChild<QLineEdit*>("loginNumber")->setText(own);
    w.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");
    click(w, "loginButton"); click(w, "messagesNav"); select(w, other);
}
void settings(QWidget& w)
{
    if (!w.findChild<QWidget*>("chatConnectionPanel")->isVisible()) click(w, "chatSettings");
}
void config(QWidget& w, int mode, int port, const QString& ip = {})
{
    settings(w); w.findChild<QComboBox*>("chatMode")->setCurrentIndex(mode);
    w.findChild<QSpinBox*>("chatPort")->setValue(port);
    if (mode == 1) w.findChild<QLineEdit*>("chatAddress")->setText(ip);
    click(w, "chatSaveSettings");
}
bool ready(QWidget& w) { return w.findChild<QPushButton*>("chatSend")->isEnabled(); }
bool body(QWidget& w, const QString& text)
{
    for (auto* label : w.findChildren<QLabel*>("chatBody")) if (label->text() == text) return true;
    return false;
}
int freePort() { TcpPeerLJY peer; return peer.startListening("127.0.0.1", 0); }

int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
    try {
        QTemporaryDir temp(QDir::currentPath() + "/connection_XXXXXX"); check(temp.isValid(), "temp directory");
        const auto evidence = QDir::currentPath() + "/p5_connection_ui"; QDir().mkpath(evidence);
        PlatformLJY pa, pb; loadDemoDataLJY(pa); loadDemoDataLJY(pb);
        const auto aPath = temp.path() + "/a.db", bPath = temp.path() + "/b.db";
        SqliteStorageLJY sa(aPath.toStdString()), sb(bPath.toStdString()); sa.save(pa); sb.save(pb);
        SessionManagerLJY aSession(pa), bSession(pb);
        MainWindowLJY a(aSession, [&] { sa.save(pa); }, aPath), b(bSession, [&] { sb.save(pb); }, bPath);
        a.show(); b.show(); QTest::qWait(80); login(a, "10001", "10002"); login(b, "10002", "10001");
        const int port = freePort(); config(a, 0, port); config(b, 0, port);
        auto* editor = b.findChild<QPlainTextEdit*>("chatEditor");
        editor->setPlainText("草稿"); QTest::keyClick(editor, Qt::Key_Return);
        check(editor->toPlainText() == "草稿" && !ready(b), "offline Enter discarded draft");
        // 号码较大的一方先开始，经历至少一次拒绝和重试后再开启等待方。
        click(b, "chatStart"); QTest::qWait(3000);
        check(b.findChild<QPushButton*>("chatStart")->text() == "取消等待", "connector did not keep waiting");
        check(!b.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"), "ordinary wait is red error");
        b.grab().save(evidence + "/waiting.png");
        click(a, "chatStart"); waitFor([&] { return ready(a) && ready(b); }, "automatic roles cannot connect");
        check(!a.findChild<QWidget*>("chatConnectionBar")->isVisible(), "connected bar not collapsed");
        editor->setPlainText("第一行"); editor->moveCursor(QTextCursor::End);
        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier); editor->insertPlainText("第二行");
        QTest::keyClick(editor, Qt::Key_Return, Qt::ShiftModifier); editor->insertPlainText("第三行");
        check(editor->toPlainText() == "第一行\n第二行\n第三行", "modified Enter did not insert newline");
        check(!body(a, "第一行"), "newline sent message");
        QTest::keyClick(editor, Qt::Key_Return);
        waitFor([&] { return body(a, "第一行\n第二行\n第三行"); }, "Enter did not send whole message");
        check(editor->toPlainText().isEmpty(), "successful send kept draft");
        // 在线保存下一次使用的设置，不影响当前连接。
        config(b, 0, port + (port == 65535 ? -1 : 1)); check(ready(a) && ready(b), "settings disconnected chat");
        settings(b); check(b.findChild<QSpinBox*>("chatPort")->value() != port, "settings not saved");
        b.findChild<QSpinBox*>("chatPort")->setValue(port); click(b, "chatCancelSettings");
        settings(b); check(b.findChild<QSpinBox*>("chatPort")->value() != port, "cancel applied changes");
        click(b, "chatDisconnect"); waitFor([&] { return !ready(a) && !ready(b); }, "disconnect failed");
        check(b.findChild<QPushButton*>("chatStart")->text() == "重新连接", "reconnect label");
        // 两机模式在本机以 127.0.0.1 验证相同连接路径，不冒充实体两机实测。
        const int lanPort = freePort(); config(a, 1, lanPort); config(b, 1, lanPort, "invalid");
        check(b.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"), "invalid IP not rejected");
        check(b.findChild<QWidget*>("chatConnectionPanel")->isVisible(), "invalid settings closed editor");
        b.findChild<QLineEdit*>("chatAddress")->setText("127.0.0.1"); click(b, "chatSaveSettings");
        settings(b); b.resize(900, 600); QTest::qWait(60);
        check(b.size() == QSize(900, 600), "settings enlarge minimum size");
        check(b.findChild<QLineEdit*>("chatAddress")->height() >= 30 && b.findChild<QPushButton*>("chatSaveSettings")->height() >= 30, "settings controls crushed");
        b.grab().save(evidence + "/settings-small.png"); click(b, "chatCancelSettings");
        click(a, "chatStart"); click(b, "chatStart"); waitFor([&] { return ready(a) && ready(b); }, "LAN mode failed");
        a.grab().save(evidence + "/connected.png");
        settings(b); click(b, "chatDisconnect"); waitFor([&] { return !ready(a); }, "peer disconnect");
        // 取消和换好友都必须终止待重试任务。
        click(b, "chatStart"); QTest::qWait(200); click(b, "chatStart");
        QTest::qWait(2800); check(!b.findChild<QPushButton*>("chatDisconnect")->isEnabled(), "cancel restarted connection");
        click(b, "chatStart"); QTest::qWait(200); select(b, "10003"); QTest::qWait(2800);
        check(!b.findChild<QPushButton*>("chatDisconnect")->isEnabled(), "friend change restarted connection");
        a.close(); b.close();
        SessionManagerLJY restoredSession(pb); MainWindowLJY restored(restoredSession, {}, bPath);
        restored.show(); QTest::qWait(30); login(restored, "10002", "10001"); settings(restored);
        check(restored.findChild<QComboBox*>("chatMode")->currentIndex() == 1, "mode not restored");
        check(restored.findChild<QSpinBox*>("chatPort")->value() == lanPort, "LAN port not restored");
        check(restored.findChild<QLineEdit*>("chatAddress")->text() == "127.0.0.1", "IP not restored");
        restored.findChild<QComboBox*>("chatMode")->setCurrentIndex(0);
        check(restored.findChild<QSpinBox*>("chatPort")->value() != lanPort, "mode configs mixed");
        restored.close();
        std::cout << "PASS auto roles, connector first/retry/cancel, LAN path, Enter/Shift/Ctrl, draft, settings validation/persistence/cancel, online settings, friend change, 900x600\n";
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
