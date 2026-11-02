#include "gui/GroupMessagesPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
void check(bool ok,const char *why) { if(!ok) throw std::runtime_error(why); }
void waitFor(const std::function<bool()> &f,const char *why,int ms=15000)
{
    QElapsedTimer t;t.start();while(!f() && t.elapsed()<ms) QTest::qWait(10);check(f(),why);
}
int port() { QTcpServer s;check(s.listen(QHostAddress::LocalHost,0),"free port");return s.serverPort(); }
int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    try
    {
        QTemporaryDir dir;check(dir.isValid(),"temp directory");
        const int service=app.arguments().size()>1 ? app.arguments()[1].toInt():0;
        const QString a=service==1 ? "wx_20001":"10001";
        const QString b=service==1 ? "wx_20002":service==2 ? "10004":"10002";
        const auto room=QString::number(service)+":1001";
        const auto db=dir.path()+"/client.db", log=dir.path()+"/server.db", file=dir.path()+"/members.json";
        GroupChatMembersLJY members{{a,{"A",0}},{b,{"B",1}}};
        QFile f(file);check(f.open(QIODevice::WriteOnly),"members file");
        f.write(QJsonDocument(QJsonObject{{"room",room},{"members",QJsonArray{
            QJsonObject{{"account",a},{"nickname","A"},{"color",0}},
            QJsonObject{{"account",b},{"nickname","B"},{"color",1}}}}}).toJson());f.close();
        const int endpoint=port();
        QProcess server;
#ifdef Q_OS_WIN
        server.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *x){x->flags|=CREATE_NO_WINDOW;});
#endif
        auto start=[&] {
            server.start(QCoreApplication::applicationDirPath()+"/jlu_group_server.exe",
                {"--room",room,"--port",QString::number(endpoint),"--database",log,"--members",file});
            check(server.waitForStarted(),"server process start");
            check(server.waitForReadyRead(5000),"server readiness output");
            check(server.readAllStandardOutput().contains("READY"),"server bind/persistence");
        };
        start();
        GroupChatClientLJY sender; sender.requirePersistent(true);
        QVector<QJsonObject> received;
        QObject::connect(&sender,&GroupChatClientLJY::message,[&](const QJsonObject &m){if(!m.value("history").toBool())received.append(m);});
        sender.enter(room,b,members, endpoint);
        waitFor([&]{return sender.ready() && sender.historyComplete();},"B joins persistent room");
        for(int i=0;i<205;++i)
        {
            sender.sendText("离线消息 "+QString::number(i));
            waitFor([&]{return received.size()==i+1;},"durable send acknowledgement");
        }
        PlatformLJY platform;loadDemoDataLJY(platform);SqliteStorageLJY storage(db.toStdString());storage.save(platform);
        SessionManagerLJY session(platform);session.login(static_cast<ServiceTypeLJY>(service),a.toStdString(),"DemoOnly123!");
        QSettings config(db+".group.connection.ini",QSettings::IniFormat);
        auto prefix=QString::number(service)+"/1001/";
        config.setValue(prefix+"mode",2);config.setValue(prefix+"port",endpoint);config.setValue(prefix+"ip","127.0.0.1");config.sync();
        GroupChatHistoryLJY history(db);
        // 模拟本地只保存了较新的一条，补回旧记录后仍必须正确排序。
        history.save(service,a,1001,GroupChatMessageLJY::fromFrame(received.back()));
        auto page=std::make_unique<GroupMessagesPageLJY>(session,db);page->resize(1000,700);page->show();page->openGroup(1001);
        auto done=[&]{return page->findChild<QPushButton *>("groupChatSend")->isEnabled();};
        waitFor(done,"A offline catch-up completion");
        check(history.load(service,a,1001,0,1000).size()==205,"A did not recover all offline messages");
        check(page->findChildren<QLabel *>("groupChatBody").size()==100,"recent page size");
        auto *earlier=page->findChild<QPushButton *>("groupChatEarlier");
        check(earlier->isEnabled(),"older page disabled before local history end");earlier->click();
        check(page->findChildren<QLabel *>("groupChatBody").size()==200,"second page");earlier->click();
        auto labels=page->findChildren<QLabel *>("groupChatBody");
        check(labels.size()==205,"third page/no duplicates");
        for(int i=0;i<205;++i) check(labels[i]->text()=="离线消息 "+QString::number(i),"chronological page order");
        check(!earlier->isEnabled() && earlier->text()=="已到最早消息","server-confirmed history end");
        // 真正切换到其他群，再由 B 发送；返回后无需手动点历史按钮。
        page->openGroup(1002);
        check(page->findChild<QLabel *>("groupChatTitle")->text().contains("1002"),"did not switch to another group");
        for(int i=0;i<7;++i) { sender.sendText("切换期间 "+QString::number(i));waitFor([&]{return received.size()==206+i;},"send while A in another group"); }
        page->openGroup(1001);waitFor(done,"return group sync");
        check(history.load(service,a,1001,0,1000).size()==212,"switch group lost messages");
        // 同步过程中同时有实时消息，最终只能出现一份。
        page->openGroup(1001);sender.sendText("同步期间的实时消息");
        waitFor([&]{return done() && history.load(service,a,1001,0,1000).size()==213;},"live and history overlap");
        server.kill();check(server.waitForFinished(5000),"kill server");
        waitFor([&]{return !sender.ready();},"disconnect noticed");
        start();
        waitFor([&]{return sender.ready() && sender.historyComplete() && done();},"server crash/restart reconnect");
        check(history.load(service,a,1001,0,1000).size()==213,"restart replay duplicated/lost records");
        sender.sendText("重启后消息");waitFor([&]{return history.load(service,a,1001,0,1000).size()==214;},"send after restart");
        // 清空客户端缓存，证明重启后读取的来源确实是服务端日志。
        page.reset();
        const auto second=dir.path()+"/new-client.db";SqliteStorageLJY secondStorage(second.toStdString());secondStorage.save(platform);
        QSettings secondConfig(second+".group.connection.ini",QSettings::IniFormat);
        secondConfig.setValue(prefix+"mode",2);secondConfig.setValue(prefix+"port",endpoint);secondConfig.setValue(prefix+"ip","127.0.0.1");secondConfig.sync();
        page=std::make_unique<GroupMessagesPageLJY>(session,second);page->show();page->openGroup(1001);waitFor(done,"new client replay after restart");
        GroupChatHistoryLJY fresh(second);auto all=fresh.load(service,a,1001,0,1000);
        check(all.size()==214 && all.front().body=="离线消息 0" && all.back().body=="重启后消息","durable restart recovery");
        page.reset();sender.leave();server.kill();server.waitForFinished();
        std::cout<<"PASS service "<<service<<": 205 offline messages, 3 ordered pages, cached newer row, group switch, concurrent live sync, hard restart, fresh client recovery and dedup\n";
    }
    catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
