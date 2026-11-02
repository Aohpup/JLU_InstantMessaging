#include "gui/MainWindowLJY.h"
#include "gui/MessagesPageLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include "services/ChatIdentityLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QSettings>
#include <QTest>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QStyleOptionTab>
#include <QComboBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QDateTime>
#include <iostream>
#include <functional>
void check(bool yes,const char* why) { if(!yes) throw std::runtime_error(why); }
void waitFor(const std::function<bool()>& f,const char* why) {
    QElapsedTimer t;t.start();while(!f()&&t.elapsed()<10000)QTest::qWait(15);check(f(),why);
}
int freePort() { QTcpServer s;s.listen(QHostAddress::LocalHost,0);return s.serverPort(); }
void click(QWidget& w,const char* name) {auto* b=w.findChild<QPushButton*>(name);check(b&&b->isEnabled(),name);b->click();QApplication::processEvents();}
int shown(QListWidget* list) {int n=0;for(int i=0;i<list->count();++i)n+=!list->item(i)->isHidden();return n;}
bool hasDate(QWidget& w,const QString& date) {for(auto* l:w.findChildren<QLabel*>("chatDateSeparator"))if(l->text()==date)return true;return false;}
int main(int argc,char** argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
 try {
    QTemporaryDir dir;const auto path=dir.path()+"/reading.db";
    PlatformLJY platform;loadDemoDataLJY(platform);SqliteStorageLJY store(path.toStdString());store.save(platform);
    SessionManagerLJY session(platform),other(platform);session.login(ServiceTypeLJY::QQ,"10001","DemoOnly123!");other.login(ServiceTypeLJY::QQ,"10002","DemoOnly123!");
    session.updateFriendRemark("10002","同桌小王");
    const auto today=QDate::currentDate().startOfDay().toMSecsSinceEpoch();
    const auto now=QDateTime::currentMSecsSinceEpoch();
    ChatHistoryLJY history(path.toStdString());GroupChatHistoryLJY groups(path);
    for(int i=0;i<115;++i) {
        const auto time=today-(i<30?3: i<70?1:0)*86400000LL+i;
        history.save("10001",{"reading"+std::to_string(i),"10002","10001","历史阅读 "+std::to_string(i),time,ChatDeliveryLJY::Received});
        GroupChatMessageLJY m;m.id="g"+QString::number(i);m.sender="10002";m.nickname="演示B";m.body="群历史阅读 "+QString::number(i);m.time=time;m.color=1;
        groups.save(0,"10001",1001,m);
    }
    history.save("10001",{"recent","10003","10001","另一位好友的最新消息",now-1000,ChatDeliveryLJY::Received});
    const int groupPort=freePort(),privatePort=freePort();
    QSettings config(path+".group.connection.ini",QSettings::IniFormat);
    for(int g=1001;g<=1007;++g){const auto pre="0/"+QString::number(g)+"/";config.setValue(pre+"mode",2);config.setValue(pre+"ip","127.0.0.1");config.setValue(pre+"port",g==1001?groupPort:freePort());}config.sync();
    QSettings unread(path+".unread.ini",QSettings::IniFormat); unread.setValue("0/10001/10002",115); unread.sync();
    QSettings privateConfig(path+".connection.ini",QSettings::IniFormat);privateConfig.setValue("mode",0);privateConfig.setValue("local/port",privatePort);privateConfig.sync();
    GroupChatMembersLJY roster{{"10001",{"A",0}},{"10002",{"B",1}}};
    GroupChatHostLJY host;host.setDatabase(dir.path()+"/server.db");check(host.start("0:1001",groupPort,"reading",roster),"host start");
    MainWindowLJY w(session,[&]{store.save(platform);},path);w.resize(1100,800);w.show();QTest::qWait(100);
    click(w,"friendsNav");auto* friends=w.findChild<QListWidget*>("friendsList");
    auto* search=w.findChild<QLineEdit*>("friendsListSearch");
    for(const auto& query:QStringList{"同桌小王","演示BQQ","10002"}){search->setText(query);check(shown(friends)==1,"friend filter nickname/remark/number");}
    search->setText("不存在的联系人");check(shown(friends)==0&&w.findChild<QLabel*>("friendsListSearchHint")->isVisible(),"no results hint");search->clear();
    click(w,"groupsNav");auto* operation=w.findChild<QComboBox*>("groupMemberOperation");auto* action=w.findChild<QPushButton*>("groupMemberAction");const auto size=action->size();
    for(int i=0;i<4;++i){operation->setCurrentIndex(i);check(action->text()==operation->currentText()&&action->size()==size,"action text or geometry");}
    auto* managed=w.findChild<QListWidget*>("groupsList");w.findChild<QLineEdit*>("groupsListSearch")->setText("1001");check(shown(managed)==1,"management group search");
    auto checkBadges=[&] {
        QTest::qWait(600); // 等待主窗口的 500 毫秒未读刷新定时器。
        auto* bar=w.findChild<QTabWidget*>("messageTabs")->tabBar();
        check(bar->tabRect(0).width()==bar->tabRect(1).width(),"unread changed tab widths");
        for(int i=0;i<2;++i) {
            auto* slot=bar->tabButton(i,QTabBar::RightSide);
            auto* badge=slot ? slot->findChild<QLabel*>() : nullptr;
            QStyleOptionTab option; option.initFrom(bar); option.rect=bar->tabRect(i);
            option.shape=bar->shape(); option.text=bar->tabText(i);
            option.rightButtonSize=slot ? slot->size() : QSize();
            const auto textRect=bar->style()->subElementRect(QStyle::SE_TabBarTabText,&option,bar);
            check(textRect.width()>=bar->fontMetrics().horizontalAdvance(option.text)+4,"tab text clipped by unread slot");
            if(!badge || !badge->isVisible())continue;
            const QRect rect(badge->mapTo(bar,QPoint()),badge->size());
            const auto tab=bar->tabRect(i);
            check(std::abs(rect.center().y()-tab.center().y())<=1,"unread badge vertically misaligned");
            check(tab.right()-6-rect.right()>=8,"unread badge touches right border");
            check(rect.top()-tab.top()>=5 && tab.bottom()-rect.bottom()>=5,"unread badge touches vertical border");
        }
        auto* nav=w.findChild<QPushButton*>("messagesNav");
        auto* unreadBadge=w.findChild<QLabel*>("messagesNavUnread");
        const int total=w.findChild<MessagesPageLJY*>()->unreadCount()+w.findChild<GroupMessagesPageLJY*>()->unreadCount();
        check(nav->text()=="消息","sidebar unread mixed into text");
        check(unreadBadge && unreadBadge->isVisible()==(total>0),"sidebar unread visibility");
        if(total>0) {
            check(unreadBadge->text()==(total>99 ? "99+" : QString::number(total)),"sidebar unread total");
            check(unreadBadge->styleSheet().contains("#e53935"),"sidebar unread not red");
            check(std::abs(unreadBadge->geometry().center().y()-nav->rect().center().y())<=1,"sidebar unread not centered");
            check(unreadBadge->x()>14+24+nav->fontMetrics().horizontalAdvance(nav->text())+8,"sidebar badge overlaps label");
        }
    };
    click(w,"messagesNav");auto* page=w.findChild<MessagesPageLJY*>();auto* list=w.findChild<QListWidget*>("chatFriends");
    check(list->item(0)->data(Qt::UserRole).toString()=="10003","recent private ordering");
    w.findChild<QLineEdit*>("chatFriendsSearch")->setText("同桌");check(shown(list)==1,"chat remark filter");
    page->openConversation("10002");QTest::qWait(300);checkBadges();
    check(page->unreadCount()==115,"persisted unread was cleared on selection");
    click(w,"chatLatest"); QTest::qWait(100);
    auto* privateArea=w.findChild<QScrollArea*>("chatScroll");
    auto locate=[&](QWidget* p,const char* bodyName,const QString& id,QScrollArea* area) {
        for(auto* label:p->findChildren<QLabel*>(bodyName)) if(label->property("messageId").toString()==id) {
            const int y=label->parentWidget()->parentWidget()->mapTo(area->viewport(),QPoint()).y();
            return y>=-2 && y<area->viewport()->height();
        }
        return false;
    };
    check(locate(page,"chatBody","reading0",privateArea),"private earliest unread across pages not visible");
    check(privateArea->verticalScrollBar()->value()<privateArea->verticalScrollBar()->maximum(),"private unread jumped to latest");
    privateArea->verticalScrollBar()->setValue(privateArea->verticalScrollBar()->maximum());page->unreadCount();
    check(hasDate(*page,"今天")&&hasDate(*page,"昨天")&&hasDate(*page,QDate::currentDate().addDays(-3).toString("yyyy-MM-dd")),"private date separators");
    click(w,"chatLoadEarlier");QTest::qWait(100);auto* scroll=w.findChild<QScrollArea*>("chatScroll")->verticalScrollBar();check(scroll->value()==0,"pagination position");
    scroll->setValue(120);const int pos=scroll->value();page->refresh();QTest::qWait(100);check(scroll->value()==pos,"refresh moved private reading");
    click(w,"chatStart");TcpPeerLJY sender;sender.startChat(false,"127.0.0.1",privatePort,makeChatIdentityLJY(other,"10001"));
    waitFor([&]{return w.findChild<QPushButton*>("chatSend")->isEnabled();},"private ready");
    sender.sendMessage("阅读历史时收到的新消息");
    waitFor([&]{return w.findChild<QPushButton*>("chatLatest")->text().contains("1 条未读消息");},"private new message cue");
    check(scroll->value()==pos,"private incoming moved reading");
    check(list->item(0)->data(Qt::UserRole).toString()=="10002","new message did not reorder private");
    check(page->findChildren<QLabel*>("chatPreview").size()==list->count(),"private previews missing");
    auto* jump=w.findChild<QPushButton*>("chatLatest");
    check(jump->fontMetrics().horizontalAdvance(jump->text())+16<=jump->width(),"new message button text clipped");
    page->unreadCount();
    bool privateRed=false;for(auto* l:list->findChildren<QLabel*>("chatUnread"))if(l->isVisible()&&l->styleSheet().contains("#e53935"))privateRed=true;
    check(privateRed,"private red unread badge missing");
    QTest::qWait(300);checkBadges();
    w.grab().save(QCoreApplication::applicationDirPath()+"/reading-private.png");
    for(int i=0;i<12;++i) sender.sendMessage("后续私聊 "+std::to_string(i));
    waitFor([&]{return page->unreadCount()==13;},"private unread sequence count");
    click(w,"chatLatest");QTest::qWait(100);
    QString firstPrivate;
    for(auto* l:page->findChildren<QLabel*>("chatBody"))if(l->text()=="阅读历史时收到的新消息")firstPrivate=l->property("messageId").toString();
    check(locate(page,"chatBody",firstPrivate,privateArea)&&scroll->value()<scroll->maximum(),"private banner did not locate first of 13 unread");
    check(page->unreadCount()==13,"jump marked unseen tail read");
    scroll->setValue(scroll->maximum());check(page->unreadCount()==0,"private latest read clearing");
    sender.stopChat();
    auto* gp=w.findChild<GroupMessagesPageLJY*>();w.findChild<QTabWidget*>("messageTabs")->setCurrentIndex(1);gp->openGroup(1001);
    waitFor([&]{return w.findChild<QPushButton*>("groupChatSend")->isEnabled();},"group ready");QTest::qWait(100);
    check(gp->unreadCount()==115,"group unread cleared before reading");
    click(w,"groupChatLatest");QTest::qWait(100);
    auto* groupArea=w.findChild<QScrollArea*>("groupChatScroll");
    check(locate(gp,"groupChatBody","g0",groupArea),"group first unread across pages not visible");
    groupArea->verticalScrollBar()->setValue(groupArea->verticalScrollBar()->maximum());gp->unreadCount();
    check(hasDate(*gp,"今天")&&hasDate(*gp,"昨天"),"group dates");
    click(w,"groupChatEarlier");QTest::qWait(100);
    auto* gs=w.findChild<QScrollArea*>("groupChatScroll")->verticalScrollBar();gs->setValue(130);const int gpos=gs->value();
    auto* gl=w.findChild<QListWidget*>("chatGroups");w.findChild<QLineEdit*>("chatGroupsSearch")->setText("1001");check(shown(gl)==1,"chat group search");
    GroupChatClientLJY client;client.requirePersistent(true);client.enter("0:1001","10002",roster, groupPort);
    waitFor([&]{return client.ready()&&client.historyComplete();},"sender group ready");client.sendText("群历史阅读时的新消息");
    waitFor([&]{return w.findChild<QPushButton*>("groupChatLatest")->text().contains("1 条未读消息");},"group new message cue");
    check(gs->value()==gpos,"group incoming moved reading");
    check(gl->item(0)->data(Qt::UserRole).toInt()==1001&&gl->item(0)->text().contains("群历史阅读时"),"group summary/order");
    w.grab().save(QCoreApplication::applicationDirPath()+"/reading-group.png");
    for(int i=0;i<12;++i)client.sendText("后续群聊 "+QString::number(i));
    waitFor([&]{return gp->unreadCount()==13;},"group unread sequence count");
    click(w,"groupChatLatest");QTest::qWait(100);
    QString firstGroup;
    for(auto* l:gp->findChildren<QLabel*>("groupChatBody"))if(l->text()=="群历史阅读时的新消息")firstGroup=l->property("messageId").toString();
    check(locate(gp,"groupChatBody",firstGroup,groupArea)&&gs->value()<gs->maximum(),"group banner did not locate first of 13 unread");
    check(gp->unreadCount()==13,"group jump marked unseen tail read");
    bool redBadge=false;for(auto* l:gl->findChildren<QLabel*>("groupUnread"))if(l->isVisible()&&l->styleSheet().contains("#e53935"))redBadge=true;
    check(redBadge,"group red unread badge missing");
    QTest::qWait(300);checkBadges();
    w.grab().save(QCoreApplication::applicationDirPath()+"/reading-group-unread.png");
    gs->setValue(gs->maximum());check(gp->unreadCount()==0,"group latest read clearing");
    w.resize(900,600);QTest::qWait(100);check(w.size()==QSize(900,600),"new controls forced window wider/taller");checkBadges();
    const auto* input=w.findChild<QWidget*>("groupChatEditor");
    const auto* send=w.findChild<QPushButton*>("groupChatSend");
    check(input->mapTo(&w,QPoint(0,input->height())).y()<=send->mapTo(&w,QPoint()).y(),"small composer overlaps send action");
    w.grab().save(QCoreApplication::applicationDirPath()+"/reading-small.png");
    client.leave();w.close();host.stop();
    // 新窗口从本地记录重建摘要和排序。
    MainWindowLJY reopened(session,[&]{store.save(platform);},path);
    check(reopened.findChild<QListWidget*>("chatFriends")->item(0)->data(Qt::UserRole).toString()=="10002","reopened recent list");
    reopened.close();
    std::cout<<"PASS filters, concrete actions, persisted previews/order, private/group dates, pagination, stable reading, new message cues, earliest unread across pages/read clearing, small window\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

