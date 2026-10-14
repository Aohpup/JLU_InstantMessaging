#include "gui/MessagesPageLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/FriendsPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void click(QWidget& w,const char* name){auto* b=w.findChild<QPushButton*>(name);check(b&&b->isVisible()&&b->isEnabled(),name);b->click();QApplication::processEvents();}
int main(int argc,char** argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");try{
    const auto evidence=QDir::currentPath()+"/p5_ui_1014";QDir().mkpath(evidence);
    QTemporaryDir dir(QDir::currentPath()+"/p5_ui_XXXXXX");check(dir.isValid(),"temp dir");const auto path=dir.path()+"/data.db";
    PlatformLJY p;loadDemoDataLJY(p);SqliteStorageLJY storage(path.toUtf8().toStdString());storage.save(p);
    std::string longText;for(int i=1;i<=12;++i)longText+="中文长消息第 "+std::to_string(i)+" 行\n";
    std::string wrapped;for(int i=0;i<90;++i)wrapped+="自动换行中文🙂 ";
    ChatHistoryLJY history(path.toUtf8().toStdString());history.save("10001",{"long","10002","10001",longText,123,ChatDeliveryLJY::Received});history.save("10001",{"wrap","10001","10002",wrapped,124,ChatDeliveryLJY::Received});history.save("10001",{"short","10002","10001","简短消息",125,ChatDeliveryLJY::Received});
    SessionManagerLJY session(p);MainWindowLJY w(session,[&]{storage.save(p);},path);w.show();QTest::qWait(80);
    auto navY=[&](const char* name){return w.findChild<QPushButton*>(name)->mapTo(&w,QPoint()).y();};
    check(navY("messagesNav")<navY("groupsNav")&&navY("groupsNav")<navY("friendsNav")&&navY("friendsNav")<navY("accountNav"),"sidebar order");
    w.findChild<QLineEdit*>("loginNumber")->setText("10001");w.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");click(w,"loginButton");click(w,"messagesNav");
    auto* list=w.findChild<QListWidget*>("chatFriends");check(list->count()==3,"friend list");
    auto* avatar=list->itemWidget(list->item(0))->findChild<QLabel*>("chatFriendAvatar");check(avatar&&avatar->text()=="演","friend avatar");
    QTest::mouseClick(avatar,Qt::LeftButton);QTest::qWait(50);check(list->currentItem()&&list->currentItem()->data(Qt::UserRole).toString()=="10002","avatar row does not select friend");
    auto body=[&](const QString& id){for(auto* b:w.findChildren<QLabel*>("chatBody"))if(b->property("messageId")==id)return b;return static_cast<QLabel*>(nullptr);};
    auto* b=body("long");check(b&&b->text().toUtf8().toStdString()==longText,"full body changed");
    auto* expand=b->parentWidget()->findChild<QPushButton*>("chatExpand");
    check(expand->isVisible()&&b->height()<=b->fontMetrics().lineSpacing()*6,"newline preview");
    check(body("wrap")->parentWidget()->findChild<QPushButton*>("chatExpand")->isVisible(),"wrapped preview missing");
    check(!body("short")->parentWidget()->findChild<QPushButton*>("chatExpand")->isVisible(),"short shows unnecessary expand");
    auto* bar=w.findChild<QScrollArea*>("chatScroll")->verticalScrollBar();bar->setValue(0);w.grab().save(evidence+"/collapsed.png");
    expand->click();QTest::qWait(60);check(b->height()>=b->heightForWidth(b->width())&&expand->text()=="收起","expand did not reveal full text");w.grab().save(evidence+"/expanded.png");
    click(w,"accountNav");click(w,"messagesNav");b=body("long");expand=b->parentWidget()->findChild<QPushButton*>("chatExpand");check(expand->text()=="收起","refresh forgot expansion");
    expand->click();QTest::qWait(30);check(b->height()<=b->fontMetrics().lineSpacing()*6,"collapse failed");
    w.resize(900,600);QTest::qWait(80);check(w.width()==900&&w.height()==600,"small window minimum increased");check(w.findChild<QScrollArea*>("chatScroll")->horizontalScrollBar()->maximum()==0,"message overflow");
    bar->setValue(0);w.grab().save(evidence+"/small.png");
    // 正在等待连接时，查看另一个好友资料不应改变当前聊天对象。
    TcpPeerLJY reservation;int port=reservation.startListening("127.0.0.1",0);reservation.close();
    click(w,"chatSettings");w.findChild<QSpinBox*>("chatPort")->setValue(port);click(w,"chatSaveSettings");click(w,"chatStart");QTest::qWait(80);
    auto* more=list->itemWidget(list->item(1))->findChild<QPushButton*>("chatFriendMore");check(more->property("account")=="10003","more target");
    w.findChild<QTabWidget*>("friendTabs")->setCurrentIndex(1);more->click();QTest::qWait(30);
    check(w.findChild<FriendsPageLJY*>()->isVisible()&&w.findChild<QTabWidget*>("friendTabs")->currentIndex()==0,"friend page/tab navigation");
    check(w.findChild<QListWidget*>("friendsList")->currentItem()->data(Qt::UserRole)=="10003","wrong friend detail");
    check(list->currentItem()->data(Qt::UserRole)=="10002"&&w.findChild<QPushButton*>("chatDisconnect")->isEnabled(),"detail navigation stopped or changed chat");
    const char* actions[]={"friendMessageButton","friendRemarkButton","removeFriendButton"};int y=-1;
    for(auto name:actions){auto* button=w.findChild<QPushButton*>(name);check(button->width()==88&&button->height()==34,"compact action size");int now=button->mapTo(&w,QPoint()).y();check(y<0||now==y,"actions not on one row");y=now;}
    w.resize(1100,900);QTest::qWait(40);w.grab().save(evidence+"/friend-actions.png");w.close();
    check(history.load("10001","10002")[0].message.body==longText,"fold changed stored message");
    std::cout<<"PASS six-line preview/expand/collapse/wrap/refresh, sidebar order, avatar row selection, friend detail without disconnect, compact actions, 900x600\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
