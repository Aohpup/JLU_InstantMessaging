#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/FriendsPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTest>
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
static void click(QWidget& w,const char* name)
{
    auto* b=w.findChild<QPushButton*>(name);check(b && b->isEnabled() && b->isVisible(),name);
    for(auto* s:w.findChildren<QScrollArea*>()) if(s->isAncestorOf(b)) s->ensureWidgetVisible(b);
    QTest::mouseClick(b,Qt::LeftButton);QApplication::processEvents();
}
static void choose(QListWidget* list,const QString& value)
{
    for(int i=0;i<list->count();++i) if(list->item(i)->data(Qt::UserRole).toString()==value) {list->setCurrentRow(i);return;}
    throw std::runtime_error("list item missing");
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv); app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        const auto directory=std::filesystem::u8path(JLU_GUI_TEST_DIR);std::filesystem::create_directories(directory);
        const auto path=directory/"social.db";
        const bool reload=app.arguments().contains("--reload");
        if(!reload) std::filesystem::remove(path);
        PlatformLJY platform;SqliteStorageLJY storage(path.u8string());
        if(reload) storage.load(platform); else loadDemoDataLJY(platform);
        SessionManagerLJY session(platform);
        if(!reload)
        {
            // 模拟旧库：删除仅新增的空表，再打开时自动补建并保留原数据。
            storage.save(platform);sqlite3* db=nullptr;sqlite3_open(path.u8string().c_str(),&db);
            check(sqlite3_exec(db,"DROP TABLE friend_requests;",nullptr,nullptr,nullptr)==SQLITE_OK,"prepare old schema");sqlite3_close(db);
            {SqliteStorageLJY upgraded(path.u8string());PlatformLJY old;upgraded.load(old);check(old.accountCount()==12,"old database upgrade preserves accounts");}
            for(int i=0;i<3;++i) session.registerAccount("同城测试","2000-01-01","长春","DemoOnly123!",ServiceTypeLJY::QQ,"新同学"+std::to_string(i));
        }
        MainWindowLJY window(session,[&]{storage.save(platform);},QString::fromStdWString(path.wstring()));window.show();
        auto input=[&](const char* name,const QString& text){window.findChild<QLineEdit*>(name)->setText(text);};
        auto login=[&](const QString& number){if(session.isLoggedIn())click(window,"logoutButton");input("loginNumber",number);input("loginPassword","DemoOnly123!");click(window,"loginButton");click(window,"friendsNav");};
        auto* tabs=window.findChild<QTabWidget*>("friendTabs");
        auto* mode=window.findChild<QComboBox*>("friendQueryType");
        auto* results=window.findChild<QListWidget*>("friendQueryResults");
        auto* requests=window.findChild<QListWidget*>("friendRequestsList");
        auto* status=window.findChild<QLabel*>("saveStatus");
        auto shot=[&](const char* name){app.processEvents();window.grab().save(QString::fromStdWString((directory/name).wstring()));};
        auto friends=[&](const char* a,const char* b){return platform.findFriendRelation(platform.findAccount(ServiceTypeLJY::QQ,a)->accountId(),platform.findAccount(ServiceTypeLJY::QQ,b)->accountId())!=nullptr;};
        login("10001");
        if(reload)
        {
            check(friends("10001","10005") && !friends("10001","10006"),"accepted and pending relations restored correctly");
            check(session.friendRequests().size()==2,"both request records persisted");
            login("10006");tabs->setCurrentIndex(2);choose(requests,"10001");answerDialogLJY(QMessageBox::Yes);click(window,"acceptFriendRequest");
            check(friends("10001","10006"),"restored pending request can be accepted");
            window.close();std::cout<<"PASS request states and pending acceptance restored in separate process\n";return 0;
        }
        check(!window.windowIcon().isNull(),"application icon available");
        click(window,"sidebarProfileButton");check(window.findChild<QPushButton*>("accountNav")->isChecked(),"identity card navigates to account");
        check(window.findChild<QLabel*>("sidebarNickname")->text()=="演示AQQ" && window.findChild<QLabel*>("sidebarNumber")->text()=="10001","sidebar name and number separated");
        app.processEvents();
        auto* identity=window.findChild<QPushButton*>("sidebarProfileButton");
        for (const auto* name : {"sidebarAvatar","sidebarNickname","sidebarNumber"})
        {
            const auto* label=window.findChild<QLabel*>(name);
            check(identity->rect().contains(QRect(label->mapTo(identity,QPoint()),label->size())),"sidebar labels fully inside identity card");
        }
        click(window,"friendsNav");tabs->setCurrentIndex(1);mode->setCurrentIndex(3);click(window,"friendQueryButton");
        check(results->count()==3,"city first batch three");const QString first=results->item(0)->data(Qt::UserRole).toString();
        shot("同城推荐.png");click(window,"friendQueryButton");check(results->count()==1,"city final batch fewer than three");
        click(window,"friendQueryButton");check(results->count()==3 && results->item(0)->data(Qt::UserRole).toString()==first,"city candidates loop from start");
        tabs->setCurrentIndex(0);input("addFriendNumber","10005");
        QTimer::singleShot(0,[&]{
            auto* panel=activeDialogLJY();check(panel && !panel->isWindow(),"confirmation embedded in main window");
            window.resize(900,600);app.processEvents();check(panel->size()==window.centralWidget()->size(),"overlay follows host resize");
            shot("内置好友申请确认.png");QTest::keyClick(panel,Qt::Key_Tab);
            check(panel->isAncestorOf(QApplication::focusWidget()),"Tab stays inside embedded dialog");
            QTest::keyClick(panel,Qt::Key_Escape);
        });
        click(window,"addFriendButton");check(session.friendRequests().empty(),"Escape cancels request");
        answerDialogLJY(QMessageBox::Yes);click(window,"addFriendButton");
        check(!friends("10001","10005") && session.friendRequests().size()==1,"sending does not create friendship");
        input("addFriendNumber","10005");answerDialogLJY(QMessageBox::Yes);click(window,"addFriendButton");
        check(session.friendRequests().size()==1 && status->text().contains("申请已发送"),"duplicate pending request rejected");
        tabs->setCurrentIndex(2);choose(requests,"10005");answerDialogLJY(QMessageBox::Yes);click(window,"acceptFriendRequest");
        check(!friends("10001","10005") && status->text().contains("只有收到"),"sender cannot accept own request");
        click(window,"saveButton");
        {PlatformLJY restored;SqliteStorageLJY reader(path.u8string());reader.load(restored);SessionManagerLJY receiver(restored);receiver.login(ServiceTypeLJY::QQ,"10005","DemoOnly123!");check(receiver.friendRequests().size()==1,"pending request saved before acceptance");}
        login("10005");tabs->setCurrentIndex(2);choose(requests,"10001");shot("收到好友申请.png");
        answerDialogLJY(QMessageBox::Yes);click(window,"rejectFriendRequest");check(!friends("10001","10005"),"rejection does not create relation");
        login("10001");tabs->setCurrentIndex(0);input("addFriendNumber","10005");answerDialogLJY(QMessageBox::Yes);click(window,"addFriendButton");
        login("10005");tabs->setCurrentIndex(2);choose(requests,"10001");answerDialogLJY(QMessageBox::Yes);click(window,"acceptFriendRequest");check(friends("10001","10005"),"recipient approval establishes friendship");
        login("10001");tabs->setCurrentIndex(1);mode->setCurrentIndex(3);click(window,"friendQueryButton");
        check(results->count()==3,"accepted friend removed from city candidates");choose(results,"10006");answerDialogLJY(QMessageBox::Yes);click(window,"addRecommendationButton");
        check(!friends("10001","10006"),"city add also requires approval");
        click(window,"friendQueryButton");check(results->count()==2,"pending requests excluded from city candidates");
        click(window,"groupsNav");input("groupNumber","1001");click(window,"findGroupButton");window.findChild<QTabWidget*>("groupTabs")->setCurrentIndex(2);app.processEvents();
        
        click(window,"switchGroupModeButton");
        check(status->text().contains("当前已是") && status->styleSheet().contains("#fdeaea"),"same mode is red failure");
        QTest::qWait(350); check(status->minimumHeight()==42,"original compact warning minimum restored");
        click(window,"saveButton");
        shot("侧栏与稳定提示.png");window.close();
        std::cout<<"PASS old database migration, city 3/1/repeat, request send/reject/accept, persistence, sidebar, icon, embedded dialog and stable feedback\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}
