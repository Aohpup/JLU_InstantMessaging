#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include "gui/MessagesPageLJY.h"
#include "gui/FriendsPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include "network/GroupRosterLJY.h"
#include <QApplication>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QSettings>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QComboBox>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <QMessageBox>
#include <iostream>
#include <functional>
void check(bool x,const char* why) { if(!x) throw std::runtime_error(why); }
void waitFor(const std::function<bool()>& f,const char* why) {
    QElapsedTimer t;t.start();while(!f()&&t.elapsed()<12000)QTest::qWait(10);check(f(),why);
}
int freePort() { QTcpServer s;s.listen(QHostAddress::LocalHost,0);return s.serverPort(); }
void click(QWidget& w,const char* name) {
    auto* b=w.findChild<QPushButton*>(name);check(b&&b->isEnabled(),name);b->click();QApplication::processEvents();
}
void type(QWidget& w,const char* name,const QString& value) {
    auto* e=w.findChild<QLineEdit*>(name);check(e,name);e->setText(value);
}
int main(int argc,char** argv) {
 QApplication app(argc,argv);
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
 try {
    QTemporaryDir dir; check(dir.isValid(),"temp");
    GroupChatMembersLJY original{{"10001",{"A",0}},{"10002",{"B",1}},{"10003",{"C",2}}};
    saveGroupRosterLJY(dir.path(),"0:1001",{},original,true);
    auto removed=original;removed.remove("10003");
    saveGroupRosterLJY(dir.path(),"0:1001",original,removed);
    saveGroupRosterLJY(dir.path(),"0:1001",{},original,true);
    auto added=original;added["10004"]={"D",3};
    saveGroupRosterLJY(dir.path(),"0:1001",original,added);
    QFile roster(dir.path()+"/0_1001.json");roster.open(QIODevice::ReadOnly);
    const auto data=roster.readAll();check(!data.contains("10003")&&data.contains("10004"),"stale roster resurrected removed member");
    PlatformLJY platform;loadDemoDataLJY(platform);
    const auto path=dir.path()+"/ui.db";SqliteStorageLJY store(path.toStdString());store.save(platform);
    SessionManagerLJY session(platform);session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");
    QSettings config(path+".group.connection.ini",QSettings::IniFormat);
    auto endpoint=[&](int group,int port) {auto prefix="0/"+QString::number(group)+"/";
        config.setValue(prefix+"mode",2);config.setValue(prefix+"ip","127.0.0.1");config.setValue(prefix+"port",port);};
    for(int i=1001;i<=1007;++i) endpoint(i,freePort());
    const int port1=freePort(),port2=freePort();endpoint(1001,port1);endpoint(1002,port2);config.sync();
    GroupChatMembersLJY members{{"10001",{"A",0}},{"10002",{"B",1}},{"10003",{"C",2}}};
    GroupChatHostLJY host1,host2;host1.setDatabase(dir.path()+"/room1.db");host2.setDatabase(dir.path()+"/room2.db");
    check(host1.start("0:1001",port1,"one",members),"host1");check(host2.start("0:1002",port2,"two",members),"host2");
    bool fail=false;int saved=0;
    MainWindowLJY w(session,[&]{if(fail)throw std::runtime_error("验收保存失败");store.save(platform);++saved;},path);
    w.resize(1100,800);w.show();QTest::qWait(100);
    check(!w.findChild<QPushButton*>("saveButton"),"manual save still present");
    click(w,"profileButton");type(w,"profileName","草稿名称");
    click(w,"friendsNav");click(w,"accountNav");
    check(w.findChild<QLineEdit*>("profileName")->text()=="草稿名称"&&w.findChild<QLineEdit*>("profileName")->isVisible(),"navigation lost profile draft");
    click(w,"profileButton");check(saved==1,"profile not automatically saved");
    {PlatformLJY p;SqliteStorageLJY read(path.toStdString());read.load(p);check(p.findPerson(1)->name()=="草稿名称","profile missing from database");}
    fail=true;click(w,"profileButton");type(w,"profileName","重试名称");click(w,"profileButton");
    check(w.findChild<QPushButton*>("retryAutoSave")->isVisible(),"save failure retry absent");
    check(w.findChild<QLabel*>("saveStatus")->text().contains("保存失败"),"save failure hidden");
    QTest::qWait(550);
    auto* persistence=w.findChild<QLabel*>("persistenceStatus");
    check(persistence&&persistence->text().contains("未保存"),"fixed unsaved status missing");
    w.findChild<MessagesPageLJY*>()->feedback("另一项普通通知",false);
    check(persistence->text().contains("未保存"),"ordinary feedback erased persistence state");
    fail=false;click(w,"retryAutoSave");check(saved==2,"retry not saved");
    check(persistence->text().contains("已自动保存")&&!w.findChild<QPushButton*>("retryAutoSave")->isVisible(),"persistence retry did not clear");
    click(w,"groupsNav");click(w,"createGroupButton");type(w,"newGroupName","新建课程群");click(w,"confirmCreateGroup");
    check(session.selectedGroup(1007).name()=="新建课程群"&&session.selectedGroup(1007).members().size()==1,"normal group creation");
    check(saved==3,"group not automatically saved");
    click(w,"friendsNav");auto* tabs=w.findChild<QTabWidget*>("friendTabs");tabs->setCurrentIndex(1);
    w.findChild<QComboBox*>("friendQueryType")->setCurrentIndex(3);click(w,"friendQueryButton");
    auto* results=w.findChild<QListWidget*>("friendQueryResults");check(results->count()>0,"city candidate fixture");
    const int count=results->count();const auto candidate=results->item(0)->data(Qt::UserRole).toString();results->setCurrentRow(0);
    answerDialogLJY(QMessageBox::Yes);click(w,"addRecommendationButton");
    check(results->count()==count&&results->item(0)->data(Qt::UserRole).toString()==candidate,"recommendation batch cleared");
    check(results->item(0)->text().contains("已申请")&&!w.findChild<QPushButton*>("addRecommendationButton")->isEnabled(),"pending row not marked");
    tabs->setCurrentIndex(0);auto* list=w.findChild<QListWidget*>("friendsList");list->setCurrentRow(0);
    const auto peer=list->currentItem()->data(Qt::UserRole).toString();click(w,"friendRemarkButton");type(w,"friendRemark","同桌");click(w,"saveRemarkButton");
    click(w,"friendMessageButton");check(w.findChild<QLabel*>("chatPeerTitle")->text().contains("同桌"),"header lost friend remark");
    auto* groupPage=w.findChild<GroupMessagesPageLJY*>();w.findChild<QTabWidget*>("messageTabs")->setCurrentIndex(1);groupPage->openGroup(1002);
    waitFor([&]{return w.findChild<QPushButton*>("groupChatSend")->isEnabled();},"current group connection");
    GroupChatClientLJY sender;sender.enter("0:1001","10002",members, port1);
    waitFor([&]{return sender.ready()&&sender.historyComplete();},"sender ready");
    sender.sendText("其他群的未读消息");
    GroupChatHistoryLJY history(path);
    waitFor([&]{return history.unread(0,"10001",1001)==1;},"background group did not receive");
    check(groupPage->unreadCount()>=1,"unread counter missing");
    waitFor([&]{return w.findChild<QPushButton*>("messagesNav")->text().contains("1");},"navigation badge missing");
    groupPage->openGroup(1001);waitFor([&]{return w.findChild<QPushButton*>("groupChatSend")->isEnabled();},"open unread room");
    click(w,"groupChatLatest"); QTest::qWait(100);
    waitFor([&]{return groupPage->unreadCount()==0;},"read did not clear count");
    check(w.findChild<QListWidget*>("groupChatMembers")->count()==3,"full authoritative member list missing");
    auto* body=w.findChild<QLabel*>("groupChatBody");check(body&&body->text()=="其他群的未读消息","message missing");
    groupPage->openGroup(1001);check(w.findChild<QPushButton*>("groupChatSend")->isEnabled()&&body==w.findChild<QLabel*>("groupChatBody"),"same room reconnected/rebuilt");
    sender.sendText("已读中的消息");waitFor([&]{return history.load(0,"10001",1001).size()==2;},"second message");
    groupPage->unreadCount();check(history.unread(0,"10001",1001)==0,"visible message unread");
    click(w,"groupChatMembersButton");QTest::qWait(100);
    w.grab().save(QCoreApplication::applicationDirPath()+"/usability-groups.png");
    click(w,"accountNav");sender.sendText("隐藏页面未读");waitFor([&]{return groupPage->unreadCount()==1;},"hidden current room unread");
    const int saves=saved;w.close();check(saved==saves,"unchanged close rewrote database");
    {GroupChatHistoryLJY read(path);check(read.unread(0,"10001",1001)==1,"unread did not persist");}
    sender.leave();host1.stop();host2.stop();
    session.logout();session.login(ServiceTypeLJY::QQ,candidate.toStdString(),"DemoOnly123!");
    MainWindowLJY receiver(session,[&]{store.save(platform);},path);receiver.show();
    waitFor([&]{return receiver.findChild<QPushButton*>("friendsNav")->text().contains("1");},"incoming friend request badge missing");
    check(receiver.findChild<QTabWidget*>("friendTabs")->tabText(2).contains("1"),"friend request tab badge missing");
    receiver.close();
    std::cout<<"PASS roster delta, autosave/retry, drafts, normal group, recommendations, remark, background receive, full roster, same-room stability, unread persistence\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
