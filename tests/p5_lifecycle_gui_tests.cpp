#include "gui/MessagesPageLJY.h"
#include "gui/MainWindowLJY.h"
#include "InlineDialogTestLJY.h"
#include "services/ChatIdentityLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTest>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void waitFor(const std::function<bool()>& f,const char* text){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<5000)QTest::qWait(10);check(f(),text);}
void click(QWidget& w,const char* name){auto* b=w.findChild<QPushButton*>(name);check(b&&b->isEnabled()&&b->isVisible(),name);b->click();QApplication::processEvents();}
void select(QWidget& w,const char* name,const QString& number){auto* l=w.findChild<QListWidget*>(name);for(int i=0;i<l->count();++i)if(l->item(i)->data(Qt::UserRole).toString()==number){l->setCurrentRow(i);QApplication::processEvents();return;}throw std::runtime_error("friend selection");}
void login(QWidget& w){w.findChild<QComboBox*>("loginService")->setCurrentIndex(0);w.findChild<QLineEdit*>("loginNumber")->setText("10001");w.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");click(w,"loginButton");click(w,"messagesNav");select(w,"chatFriends","10002");}
int freePort(){TcpPeerLJY reservation;return reservation.startListening("127.0.0.1",0);}
void listen(QWidget& w,int port){if(!w.findChild<QWidget*>("chatConnectionPanel")->isVisible())click(w,"chatSettings");w.findChild<QSpinBox*>("chatPort")->setValue(port);click(w,"chatSaveSettings");click(w,"chatStart");}
void connectPeer(QWidget& w,TcpPeerLJY& peer,const ChatIdentityLJY& id){int port=freePort();listen(w,port);waitFor([&]{return w.findChild<QLabel*>("chatState")->text().contains("等待连接");},"listen ready");peer.startChat(false,"127.0.0.1",port,id);waitFor([&]{return w.findChild<QPushButton*>("chatSend")->isEnabled();},"GUI connected");bool ready=false;waitFor([&]{for(auto& e:peer.takeEvents())ready|=e.type==ChatEventTypeLJY::Ready;return ready;},"peer ready");}
int main(int argc,char** argv){QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");try{
    QTemporaryDir folder(QDir::currentPath()+"/p5_lifecycle_XXXXXX");check(folder.isValid(),"temp folder");
    PlatformLJY remote;loadDemoDataLJY(remote);SessionManagerLJY remoteSession(remote);remoteSession.login(ServiceTypeLJY::QQ,"10002","DemoOnly123!");auto id=makeChatIdentityLJY(remoteSession,"10001");
    {
        PlatformLJY p;loadDemoDataLJY(p);SessionManagerLJY s(p);MainWindowLJY w(s,{},{});w.show();QTest::qWait(30);login(w);
        TcpPeerLJY occupied;int port=occupied.startListening("127.0.0.1",0);listen(w,port);
        waitFor([&]{return w.findChild<QLabel*>("saveStatus")->text().contains("端口");},"occupied error");QTest::qWait(120);
        check(w.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"),"error overwritten by Closed");occupied.close();
        TcpPeerLJY peer;connectPeer(w,peer,id);peer.sendMessage("切好友前消息");
        select(w,"chatFriends","10003");check(!w.findChild<QPushButton*>("chatSend")->isEnabled(),"switch peer kept connection");
        check(w.findChildren<QLabel*>("chatBody").empty(),"old peer message leaked");peer.stopChat();select(w,"chatFriends","10002");
        connectPeer(w,peer,id);peer.sendMessage("切服务前消息");answerDialogLJY(QMessageBox::Yes);click(w,"switchService2");
        check(s.currentAccount().serviceType()==ServiceTypeLJY::Weibo,"switch service");
        check(w.findChildren<QLabel*>("chatBody").empty()&&!w.findChild<QPushButton*>("chatSend")->isEnabled(),"same account number service isolation");
        peer.stopChat();answerDialogLJY(QMessageBox::Yes);click(w,"switchService0");select(w,"chatFriends","10002");
        connectPeer(w,peer,id);peer.sendMessage("注销前消息");click(w,"logoutButton");login(w);
        check(w.findChildren<QLabel*>("chatBody").empty()&&!w.findChild<QPushButton*>("chatSend")->isEnabled(),"logout old events");peer.stopChat();
        connectPeer(w,peer,id);peer.sendMessage("删除好友前消息");click(w,"friendsNav");select(w,"friendsList","10002");answerDialogLJY(QMessageBox::Yes);click(w,"removeFriendButton");
        click(w,"messagesNav");check(!w.findChild<QPushButton*>("chatSend")->isEnabled()&&w.findChildren<QLabel*>("chatBody").empty(),"removed peer still active");peer.stopChat();
        select(w,"chatFriends","10003");listen(w,freePort());QTest::qWait(50);QElapsedTimer closing;closing.start();w.close();check(!w.isVisible()&&closing.elapsed()<1000,"close listener blocked");
        std::cout<<"PASS GUI lifecycle: occupied errors, peer/service/account change, friend deletion, waiting close\n";
    }
    // 分别通过实际关闭卡片验证取消、解锁后重试和明确放弃。
    for(int choice : {0,1,2}){
        const auto path=folder.path()+QString("/pending%1.db").arg(choice);PlatformLJY p;loadDemoDataLJY(p);SqliteStorageLJY storage(path.toUtf8().toStdString());storage.save(p);
        SessionManagerLJY s(p);MainWindowLJY w(s,[&]{storage.save(p);},path);w.show();QTest::qWait(30);login(w);
        TcpPeerLJY peer;connectPeer(w,peer,id);sqlite3* lock=nullptr;check(sqlite3_open(path.toUtf8().constData(),&lock)==SQLITE_OK,"lock open");check(sqlite3_exec(lock,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr)==SQLITE_OK,"lock hold");
        peer.sendMessage("待保存消息");waitFor([&]{return w.findChild<QPushButton*>("chatRetryHistory")->isVisible();},"pending notice");
        QTimer answer;answer.setInterval(10);QObject::connect(&answer,&QTimer::timeout,[&]{if(auto* panel=activeDialogLJY()){
            if(choice==1&&lock){sqlite3_exec(lock,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_close(lock);lock=nullptr;}
            dialogButtonLJY(panel,choice==0?QMessageBox::Cancel:(choice==1?QMessageBox::Retry:QMessageBox::Discard))->click();}});answer.start();w.close();answer.stop();peer.stopChat();
        check(w.isVisible()==(choice==0),"close choice wrong");
        if(lock){sqlite3_exec(lock,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_close(lock);lock=nullptr;}
        if(choice==0){check(w.findChild<QLabel*>("chatState")->text().contains("已断开"),"cancel close not disconnected");click(w,"chatRetryHistory");w.close();}
        ChatHistoryLJY history(path.toUtf8().toStdString());check(history.load("10001","10002").size()==(choice==2?0:1),"pending close persistence");
    }
    {
        PlatformLJY p;loadDemoDataLJY(p);SessionManagerLJY s(p);bool fail=true;
        MainWindowLJY w(s,[&]{if(fail)throw std::runtime_error("验收模拟平台保存失败");},{});w.show();QTest::qWait(20);login(w);listen(w,freePort());
        // 自动保存只在资料发生变化时触发；先实际修改备注，再验证失败后的关闭选择。
        click(w,"friendsNav");select(w,"friendsList","10002");click(w,"friendRemarkButton");
        w.findChild<QLineEdit*>("friendRemark")->setText("待保存备注");click(w,"saveRemarkButton");
        answerDialogLJY(QMessageBox::Cancel);w.close();check(w.isVisible(),"platform save cancellation");
        QTimer::singleShot(0,[&]{fail=false;if(auto* panel=activeDialogLJY())dialogButtonLJY(panel,QMessageBox::Retry)->click();});w.close();check(!w.isVisible(),"platform retry close");
    }
    std::cout<<"PASS close cards: pending cancel/retry/discard; platform failure cancel/retry\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
