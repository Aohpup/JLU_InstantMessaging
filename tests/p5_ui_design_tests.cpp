#include "gui/MainWindowLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include "gui/InlineDialogLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include "storage/ChatHistoryLJY.h"
#include "storage/GroupChatHistoryLJY.h"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QSettings>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>

void check(bool okay, const QString& why) { if (!okay) throw std::runtime_error(why.toStdString()); }
int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
    try {
        QTemporaryDir temp; const auto path = temp.path() + "/design.db";
        PlatformLJY platform; loadDemoDataLJY(platform); SqliteStorageLJY store(path.toStdString()); store.save(platform);
        ChatHistoryLJY privateHistory(path.toStdString());
        privateHistory.save("10001", {"design-1","10002","10001","明天一起核对课程设计的演示流程吧。",1700000000000,ChatDeliveryLJY::Received});
        privateHistory.save("10001", {"design-2","10001","10002","好的，我来准备账号、好友和群聊的演示。",1700000060000,ChatDeliveryLJY::Received});
        GroupChatHistoryLJY groupHistory(path);
        for (int i=0;i<3;++i) {
            GroupChatMessageLJY m; m.id="design-"+QString::number(i); m.sender=i==1?"10001":"10002";
            m.nickname=i==1?"演示AQQ":"演示BQQ"; m.color=i==1?0:1; m.time=1700000000000+i*60000;
            m.body=QStringList{"周五展示前，我们再检查一次群聊。", "已经准备好了，进入群就可以直接聊天。", "收到，晚一点一起试试。"}[i];
            groupHistory.save(0,"10001",1001,m);
        }
        QTcpServer port; port.listen(QHostAddress::LocalHost,0);
        QSettings config(path+".group.connection.ini",QSettings::IniFormat); config.setValue("0/1001/port",port.serverPort());config.sync();port.close();
        SessionManagerLJY session(platform); MainWindowLJY w(session,[&]{store.save(platform);},path);
        const auto evidence=QDir::currentPath()+"/ui_refresh";QDir().mkpath(evidence);
        auto button=[&](const char* name){auto* b=w.findChild<QPushButton*>(name);check(b,name);return b;};
        auto click=[&](const char* name){button(name)->click();QTest::qWait(40);};
        auto shot=[&](QString name){QTest::qWait(80);check(w.grab().save(evidence+"/"+name+".png"),"screenshot");};
        auto equal=[&](std::initializer_list<const char*> names){QSize size;for(auto* name:names){if(size.isEmpty())size=button(name)->size();check(button(name)->size()==size,QString("unequal: ")+name);}};
        auto bounds=[&]{
            check(w.size()==QSize(900,600),"page expanded minimum window size");
            for(auto* scroll:w.findChildren<QScrollArea*>())
                if(scroll->isVisible())check(scroll->horizontalScrollBar()->maximum()==0,"horizontal overflow: "+scroll->objectName());
            for(const auto& pair : {std::pair{"chatEditor","chatSend"},std::pair{"groupChatEditor","groupChatSend"}}) {
                auto* input=w.findChild<QWidget*>(pair.first);auto* send=button(pair.second);
                if(input->isVisible()&&send->isVisible())
                    check(input->mapTo(&w,QPoint(0,input->height())).y()+6<=send->mapTo(&w,QPoint()).y(),"composer input overlaps send button");
            }
            for(auto* b:w.findChildren<QPushButton*>())
                if(b->isVisible()&&b->property("sizedAction").toBool())
                    check(b->fontMetrics().horizontalAdvance(b->text())+16<=b->width(),"button text clipped: "+b->objectName()+" "+b->text());
        };
        w.resize(1280,860);w.show();shot("01-login");
        w.findChild<QTabWidget*>("loginTabs")->setCurrentIndex(1);shot("02-register");
        w.findChild<QTabWidget*>("loginTabs")->setCurrentIndex(0);
        w.findChild<QLineEdit*>("loginNumber")->setText("10001");w.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");click("loginButton");
        equal({"switchService0","switchService1","switchService2"});check(!w.findChild<QPushButton*>("saveButton"),"manual save removed");
        equal({"profileButton","nicknameButton","passwordButton","profileCancel","nicknameCancel","passwordCancel"});
        equal({"bindButton","unbindButton"});shot("03-account");
        click("profileButton");shot("04-profile-edit");click("profileCancel");click("passwordButton");shot("05-password");click("passwordCancel");
        click("friendsNav");w.findChild<QListWidget*>("friendsList")->setCurrentRow(0);QTest::qWait(40);
        equal({"friendMessageButton","friendRemarkButton","removeFriendButton"});shot("06-friends");
        click("friendRemarkButton");equal({"saveRemarkButton","cancelRemarkButton"});shot("07-remark");click("cancelRemarkButton");
        w.findChild<QTabWidget*>("friendTabs")->setCurrentIndex(1);shot("08-discover");
        w.findChild<QTabWidget*>("friendTabs")->setCurrentIndex(2);equal({"acceptFriendRequest","rejectFriendRequest"});shot("09-requests");
        click("groupsNav");w.findChild<QLineEdit*>("groupNumber")->setText("1001");click("findGroupButton");
        equal({"groupMessageButton","applyGroupButton","leaveGroupButton"});equal({"findGroupButton","refreshGroupsButton"});shot("10-groups");
        w.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(1);equal({"acceptGroupRequest","rejectGroupRequest"});shot("11-group-requests");
        w.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(2);equal({"createSubgroupButton","openSubgroupButton","parentGroupButton"});shot("12-subgroups");
        click("messagesNav");w.findChild<QListWidget*>("chatFriends")->setCurrentRow(0);shot("13-private");click("chatSettings");shot("14-private-settings");click("chatCancelSettings");
        w.findChild<QTabWidget*>("messageTabs")->setCurrentIndex(1);w.findChild<GroupMessagesPageLJY*>()->openGroup(1001);QTest::qWait(400);shot("15-group-chat");
        equal({"chatSettings","groupChatSettings","chatSend","groupChatSend"});click("groupChatSettings");shot("16-group-settings");click("groupChatCancelSettings");
        w.resize(900,600);QTest::qWait(100);bounds();shot("17-group-small");
        click("groupChatSettings");bounds();shot("18-group-settings-small");click("groupChatCancelSettings");
        w.findChild<QTabWidget*>("messageTabs")->setCurrentIndex(0);click("chatSettings");bounds();shot("19-private-settings-small");click("chatCancelSettings");
        auto* friendList=w.findChild<QListWidget*>("chatFriends");
        for(auto* more:friendList->findChildren<QPushButton*>("chatFriendMore")) {
            // 列表内容增多后允许纵向滚动；逐行滚入视口检查资料按钮可达。
            for(int i=0;i<friendList->count();++i)
                if(friendList->itemWidget(friendList->item(i))==more->parentWidget())
                    friendList->scrollToItem(friendList->item(i),QAbstractItemView::PositionAtCenter);
            QTest::qWait(20);
            check(friendList->viewport()->rect().contains(QRect(more->mapTo(friendList->viewport(),QPoint()),more->size())),
                QString("friend menu clipped: x=%1 y=%2 w=%3 viewport=%4 rowWidth=%5 min=%6").arg(more->mapTo(friendList->viewport(),QPoint()).x()).arg(more->mapTo(friendList->viewport(),QPoint()).y()).arg(more->width()).arg(friendList->viewport()->width()).arg(more->parentWidget()->width()).arg(more->parentWidget()->minimumSizeHint().width()));
        }
        click("groupsNav");w.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(0);bounds();shot("20-groups-small");
        click("friendsNav");w.findChild<QTabWidget*>("friendTabs")->setCurrentIndex(0);bounds();shot("21-friends-small");
        click("accountNav");bounds();shot("22-account-small");
        click("logoutButton");bounds();shot("23-login-small");
        auto* loginButton=button("loginButton");
        check(w.rect().contains(QRect(loginButton->mapTo(&w,QPoint()),loginButton->size())),"login action offscreen");
        w.findChild<QTabWidget*>("loginTabs")->setCurrentIndex(1);QTest::qWait(60);bounds();
        for(auto* scroll:w.findChildren<QScrollArea*>())
            if(scroll->isAncestorOf(button("registerButton")))scroll->ensureWidgetVisible(button("registerButton"));
        shot("24-register-small");
        auto* registration=button("registerButton");
        check(w.rect().contains(QRect(registration->mapTo(&w,QPoint()),registration->size())),"register action unreachable");
        w.close();std::cout<<"PASS unified button groups, all page screenshots, 900x600 geometry and labels\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
