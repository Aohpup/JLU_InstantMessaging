#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <sqlite3.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTest>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static void waitFor(const std::function<bool()>& ready, const char* text)
{
    QElapsedTimer clock; clock.start();
    while (!ready() && clock.elapsed() < 10000) QTest::qWait(40);
    check(ready(), text);
}
static void click(QWidget& window, const char* name)
{
    auto* button=window.findChild<QPushButton*>(name); check(button && button->isEnabled(),name); button->click();
}
static QListWidgetItem* item(QListWidget* list, const QString& number)
{
    for(int i=0;i<list->count();++i) if(list->item(i)->data(Qt::UserRole).toString()==number) return list->item(i);
    return nullptr;
}
static bool requestIs(MainWindowLJY& window, const QString& number, const QString& state)
{
    auto* row=item(window.findChild<QListWidget*>("friendRequestsList"),number);
    return row && row->text().contains(state);
}
static void marker(const QString& path) { QFile f(path); check(f.open(QIODevice::WriteOnly),"marker"); }

// 两个真实进程各有 Platform 和 SQLite 连接，使用实际页面控件发送、处理申请。
static void client(const QString& role, const QString& path)
{
    PlatformLJY platform; SqliteStorageLJY storage(path.toUtf8().toStdString()); storage.load(platform);
    SessionManagerLJY session(platform);
    MainWindowLJY window(session,[&]{storage.save(platform);},path,nullptr,[&]{return storage.refreshIfChanged(platform);});
    window.resize(1280,900);window.show();
    const bool sender=role=="sender"; const QString peer=sender?"10005":"10001";
    window.findChild<QLineEdit*>("loginNumber")->setText(sender?"10001":"10005");
    window.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");click(window,"loginButton");click(window,"friendsNav");
    auto* tabs=window.findChild<QTabWidget*>("friendTabs");
    if(sender)
    {
        waitFor([&]{return QFile::exists(path+".receiver-ready");},"receiver ready");
        window.findChild<QLineEdit*>("addFriendNumber")->setText(peer);
        answerDialogLJY(QMessageBox::Yes);click(window,"addFriendButton");tabs->setCurrentIndex(2);
        check(requestIs(window,peer,"待处理"),"sender pending");
        waitFor([&]{return requestIs(window,peer,"已同意");},"sender sees acceptance without restart");
        check(item(window.findChild<QListWidget*>("friendsList"),peer),"sender friend list updated");
        check(item(window.findChild<QListWidget*>("chatFriends"),peer),"sender chat list updated");
        check(session.isLoggedIn(),"login retained");
        // 对方修改资料时，正在输入的备注不应被后台列表刷新吃掉。
        tabs->setCurrentIndex(0);window.findChild<QListWidget*>("friendsList")->setCurrentItem(item(window.findChild<QListWidget*>("friendsList"),peer));
        click(window,"friendRemarkButton");window.findChild<QLineEdit*>("friendRemark")->setText("未提交的备注");
        click(window,"groupsNav");window.findChild<QLineEdit*>("groupNumber")->setText("1001");click(window,"findGroupButton");
        window.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(2);click(window,"createSubgroupButton");
        window.findChild<QLineEdit*>("subgroupName")->setText("未提交的子群");
        marker(path+".draft-ready");
        waitFor([&]{return session.friendAccount(peer.toStdString()).nickname()=="同步后的昵称";},"nickname sync");
        check(window.findChild<QLineEdit*>("friendRemark")->text()=="未提交的备注","remark draft retained");
        check(!window.findChild<QWidget*>("remarkEditor")->isHidden(),"editor retained");
        check(window.findChild<QLineEdit*>("subgroupName")->text()=="未提交的子群","subgroup draft retained");
        marker(path+".sender-done");
    }
    else
    {
        tabs->setCurrentIndex(2);marker(path+".receiver-ready");
        waitFor([&]{return requestIs(window,peer,"待处理");},"receiver sees new request without restart");
        auto* list=window.findChild<QListWidget*>("friendRequestsList");list->setCurrentItem(item(list,peer));
        answerDialogLJY(QMessageBox::Yes);click(window,"acceptFriendRequest");
        check(requestIs(window,peer,"已同意"),"receiver accepted");
        check(item(window.findChild<QListWidget*>("friendsList"),peer),"receiver friend updated");
        waitFor([&]{return QFile::exists(path+".draft-ready");},"draft ready");
        session.updateNickname("同步后的昵称");storage.save(platform);
        waitFor([&]{return QFile::exists(path+".sender-done");},"sender finished");
    }
    window.close();
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);app.setStyle("Fusion");
    // 群后台服务也使用临时目录，测试不访问用户聊天日志。
    QTemporaryDir directory;check(directory.isValid(),"temp dir");
    app.setProperty("groupServerDirectory",directory.path()+"/group-server");
    try
    {
        if(app.arguments().size()==3) { client(app.arguments()[1],app.arguments()[2]); return 0; }
        const QString path=directory.path()+"/shared.db";
        PlatformLJY original;loadDemoDataLJY(original);
        { SqliteStorageLJY seed(path.toStdString());seed.save(original); }
        // 模拟旧库，确认无需清空业务记录即可补版本字段。
        sqlite3* raw=nullptr;check(sqlite3_open(path.toUtf8().constData(),&raw)==SQLITE_OK,"open old db");
        check(sqlite3_exec(raw,"ALTER TABLE id_state DROP COLUMN revision",nullptr,nullptr,nullptr)==SQLITE_OK,"old schema");sqlite3_close(raw);
        SqliteStorageLJY stale(path.toStdString());stale.load(original);
        SessionManagerLJY session(original);session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");session.logout();
        QProcess sender,receiver;
        sender.setProcessChannelMode(QProcess::MergedChannels);receiver.setProcessChannelMode(QProcess::MergedChannels);
        receiver.start(app.applicationFilePath(),{"receiver",path});sender.start(app.applicationFilePath(),{"sender",path});
        const bool senderDone=sender.waitForFinished(22000),receiverDone=receiver.waitForFinished(22000);
        std::cout<<sender.readAll().toStdString()<<receiver.readAll().toStdString();
        check(senderDone && sender.exitStatus()==QProcess::NormalExit && sender.exitCode()==0,"sender process");
        check(receiverDone && receiver.exitStatus()==QProcess::NormalExit && receiver.exitCode()==0,"receiver process");
        bool rejected=false;try { stale.save(original); } catch(const std::exception&) { rejected=true; }
        check(rejected,"stale platform cannot overwrite accepted request");
        check(stale.refreshIfChanged(original),"reload stale platform");
        check(!stale.refreshIfChanged(original),"unchanged does not refresh");
        session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
        check(session.friendAccount("10005").nickname()=="同步后的昵称","accepted friend persisted");
        check(session.friendRequests().at(0).state()==1,"accepted request persisted");
        // 聊天写入不改变业务版本，不应引发列表反复重建或误报保存冲突。
        check(sqlite3_open(path.toUtf8().constData(),&raw)==SQLITE_OK,"open chat write");
        check(sqlite3_exec(raw,"CREATE TABLE chat_sync_probe(body TEXT);INSERT INTO chat_sync_probe VALUES('hello')",nullptr,nullptr,nullptr)==SQLITE_OK,"chat-only write");sqlite3_close(raw);
        check(!stale.refreshIfChanged(original),"chat write ignored by business sync");
        session.updateNickname("发送者的新昵称");stale.save(original);
        std::cout<<"PASS: two processes send/accept, both lists and chats refresh, drafts/login retained, old schema upgraded, stale write rejected, chat writes ignored\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
