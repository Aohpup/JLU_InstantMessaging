#include "gui/MessagesPageLJY.h"
#include "gui/MainWindowLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <QDir>
#include <QTemporaryDir>
#include <sqlite3.h>
#include <iostream>
#include <stdexcept>
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv)
{
    QApplication app(argc, argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        QTemporaryDir folder(QDir::currentPath() + "/history_ui_XXXXXX"); check(folder.isValid(), "Create isolated folder");
        const auto path = folder.path() + "/history.db";
        PlatformLJY platform;loadDemoDataLJY(platform);SqliteStorageLJY storage(path.toUtf8().toStdString());storage.save(platform);
        ChatHistoryLJY history(path.toUtf8().toStdString());
        for (int i=1;i<=205;++i)
            history.save("10001", {"page-"+std::to_string(i),"10001","10002","历史消息 "+std::to_string(i),1700000000000+i,
                i==205?ChatDeliveryLJY::Sending:ChatDeliveryLJY::Received});
        history.save("10002", {"other-owner","10002","10001","本人B独立记录",1700000000300,ChatDeliveryLJY::Received});
        SessionManagerLJY session(platform);MainWindowLJY window(session,[&]{storage.save(platform);},path);
        window.show();QTest::qWait(60);
        auto click=[&](const char* name){auto* b=window.findChild<QPushButton*>(name);check(b && b->isVisible() && b->isEnabled(),name);QTest::mouseClick(b,Qt::LeftButton);app.processEvents();};
        auto login=[&](const char* owner){window.findChild<QLineEdit*>("loginNumber")->setText(owner);window.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");click("loginButton");click("messagesNav");};
        login("10001");
        sqlite3* lock=nullptr;check(sqlite3_open(path.toUtf8().constData(),&lock)==SQLITE_OK,"Open read lock");
        check(sqlite3_exec(lock,"BEGIN EXCLUSIVE",nullptr,nullptr,nullptr)==SQLITE_OK,"Exclusive lock");
        window.findChild<QListWidget*>("chatFriends")->setCurrentRow(0);app.processEvents();
        check(window.findChild<QLabel*>("saveStatus")->text().contains("聊天记录读取失败"),"Read failure feedback");
        check(window.findChild<QPushButton*>("chatLoadEarlier")->isVisible(),"Read retry entry missing");
        sqlite3_exec(lock,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_close(lock);
        click("chatLoadEarlier");
        check(window.findChildren<QLabel*>("chatBody").size()==100,"Initial history limit");
        check(window.findChild<QLabel*>("chatBody")->text()=="历史消息 106","Latest page starts at 106");
        click("chatLoadEarlier");QTest::qWait(50);
        check(window.findChildren<QLabel*>("chatBody").size()==200,"Earlier page not added");
        check(window.findChild<QScrollArea*>("chatScroll")->verticalScrollBar()->value()==0,"Earlier page scrolled to bottom");
        click("chatLoadEarlier");QTest::qWait(50);
        check(window.findChildren<QLabel*>("chatBody").size()==205,"Final page duplicate/missing");
        check(!window.findChild<QPushButton*>("chatLoadEarlier")->isVisible(),"End of history button not hidden");
        bool unknown=false;for(auto* l:window.findChildren<QLabel*>("chatDelivery"))unknown|=l->text().contains("结果未知");
        check(unknown,"Interrupted sending must show unknown");
        click("saveButton");check(history.load("10001","10002",0,300).size()==205,"Platform save lost history");
        click("logoutButton");login("10002");window.findChild<QListWidget*>("chatFriends")->setCurrentRow(0);app.processEvents();
        check(window.findChildren<QLabel*>("chatBody").size()==1 && window.findChild<QLabel*>("chatBody")->text()=="本人B独立记录","Owner history mixed");
        window.close();
        SessionManagerLJY memorySession(platform);MainWindowLJY memory(memorySession,{},{});memory.show();QTest::qWait(30);
        memory.findChild<QLineEdit*>("loginNumber")->setText("10001");memory.findChild<QLineEdit*>("loginPassword")->setText("DemoOnly123!");
        memory.findChild<QPushButton*>("loginButton")->click();memory.findChild<QPushButton*>("messagesNav")->click();
        memory.findChild<QListWidget*>("chatFriends")->setCurrentRow(0);app.processEvents();
        check(memory.findChildren<QLabel*>("chatBody").isEmpty(),"Memory mode read persistent history");
        check(!memory.findChild<QPushButton*>("chatLoadEarlier")->isVisible(),"Memory mode offers database history");
        check(memory.findChild<QLabel*>("chatHistoryHint")->text().contains("内存模式"),"Memory mode explanation missing");
        memory.close();
        std::cout<<"PASS GUI history: read failure/retry, 100/200/205 pagination, unknown status, owner isolation, save compatibility, memory mode\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
