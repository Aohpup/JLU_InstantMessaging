#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isEnabled() && button->isVisible(), name);
    // 小窗口先滚动到操作位置，再点击真实控件。
    for (auto* scroll : window.findChildren<QScrollArea*>())
        if (scroll->isAncestorOf(button)) scroll->ensureWidgetVisible(button);
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}
static void answer(bool yes) { answerDialogLJY(yes ? QMessageBox::Yes : QMessageBox::No); }
static void input(QWidget& w, const char* name, const QString& text) { w.findChild<QLineEdit*>(name)->setText(text); }
static void row(QTableWidget* table, const QString& number)
{
    for (int i=0; i<table->rowCount(); ++i)
        if (table->item(i,0)->data(Qt::UserRole).toString()==number) { table->selectRow(i); return; }
    throw std::runtime_error("requested member/request missing");
}

int main(int argc, char** argv)
{
    QApplication app(argc,argv);
    app.setStyle("Fusion");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        const auto directory = std::filesystem::u8path(JLU_GUI_TEST_DIR);
        std::filesystem::create_directories(directory);
        const auto path = directory / "groups.db";
        const bool reload = app.arguments().contains("--reload");
        if (!reload) std::filesystem::remove(path);
        PlatformLJY platform;
        SqliteStorageLJY storage(path.u8string());
        if (reload) storage.load(platform); else loadDemoDataLJY(platform);
        SessionManagerLJY session(platform);
        if (!reload) session.registerAccount("申请人","2000-01-01","长春","DemoOnly123!",ServiceTypeLJY::QQ,"新同学");
        MainWindowLJY window(session,[&]{storage.save(platform);},QString::fromStdWString(path.wstring()));
        window.show();
        auto* members=window.findChild<QTableWidget*>("groupMembers");
        auto* requests=window.findChild<QTableWidget*>("groupRequests");
        auto* list=window.findChild<QListWidget*>("groupsList");
        auto* tabs=window.findChild<QTabWidget*>("groupTabs");
        auto* operation=window.findChild<QComboBox*>("groupMemberOperation");
        auto enabled=[&](const char* name){return window.findChild<QPushButton*>(name)->isEnabled();};
        auto login=[&](int service,const QString& number) {
            if(session.isLoggedIn()) click(window,"logoutButton");
            check(list->count()==0 && members->rowCount()==0 && requests->rowCount()==0,"logout clears group state");
            window.findChild<QComboBox*>("loginService")->setCurrentIndex(service);
            input(window,"loginNumber",number);input(window,"loginPassword","DemoOnly123!");
            click(window,"loginButton");click(window,"groupsNav");
        };
        auto selectGroup=[&](const QString& number) {input(window,"groupNumber",number);click(window,"findGroupButton");};
        auto shot=[&](const char* name) {
            app.processEvents();
            if(window.height()>600)
                for(auto* scroll:window.findChildren<QScrollArea*>())
                    if(scroll->widget()->objectName()=="groupsPage") scroll->verticalScrollBar()->setValue(0);
            app.processEvents();
            window.grab().save(QString::fromStdWString((directory/name).wstring()));
        };
        const auto* qq=platform.findGroup(ServiceTypeLJY::QQ,1001);
        auto id=[&](ServiceTypeLJY type,const std::string& number){return platform.findAccount(type,number)->accountId();};
        if(reload)
        {
            login(0,"10003");selectGroup("1001");
            check(qq->ownerAccountId()==id(ServiceTypeLJY::QQ,"10003"),"transfer persisted");
            check(!qq->findMember(id(ServiceTypeLJY::QQ,"10001")) && !qq->findMember(id(ServiceTypeLJY::QQ,"10004")),"leave and kick persisted");
            check(qq->findMember(id(ServiceTypeLJY::QQ,"10002"))->isAdministrator(),"unrelated role persisted");
            login(1,"wx_20004");selectGroup("1002");
            check(!platform.findGroup(ServiceTypeLJY::WeChat,1002)->findMember(id(ServiceTypeLJY::WeChat,"wx_20004")),"WX leave persisted");
            const auto history=session.groupRequests(1002);
            check(history.size()==2 && history[0].state()==GroupRequestStateLJY::Rejected && history[1].state()==GroupRequestStateLJY::Accepted,"invitation history persisted");
            window.close(); std::cout<<"PASS: group membership, owner, roles and request history restored in separate process\n";return 0;
        }
        check(!enabled("groupsNav"),"group nav requires login");
        login(0,"10006");
        check(list->count()==0,"new user has no joined group");
        selectGroup("1001");
        check(members->rowCount()==0 && enabled("applyGroupButton"),"nonmember can select/apply without member disclosure");
        answer(false);click(window,"applyGroupButton");check(session.groupRequests().empty(),"cancel application");
        answer(true);click(window,"applyGroupButton");
        const int application=session.groupRequests(1001).back().id();
        check(!qq->findMember(id(ServiceTypeLJY::QQ,"10006")) && enabled("applyGroupButton"),"application pending, button remains available");
        row(requests,QString::number(application));
        click(window,"acceptGroupRequest");
        check(enabled("acceptGroupRequest") && window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"),"applicant rejected on click");
        login(0,"10001"); selectGroup("1002");check(requests->rowCount()==0,"requests filtered by selected group");
        selectGroup("1001");tabs->setCurrentIndex(1);row(requests,QString::number(application));
        shot("群_申请审批.png");
        answer(false);click(window,"acceptGroupRequest");check(!qq->findMember(id(ServiceTypeLJY::QQ,"10006")),"cancel approval");
        answer(true);click(window,"acceptGroupRequest");check(qq->findMember(id(ServiceTypeLJY::QQ,"10006")),"approved member appears");
        row(requests,QString::number(application));click(window,"acceptGroupRequest");check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"),"handled request rejected on click");
        login(0,"10006");selectGroup("1001");
        answer(false);click(window,"leaveGroupButton");check(qq->findMember(id(ServiceTypeLJY::QQ,"10006")),"cancel leave");
        answer(true);click(window,"leaveGroupButton");check(members->rowCount()==0 && list->count()==0,"leave clears selected group");
        selectGroup("1001");answer(true);click(window,"applyGroupButton");
        const int second=session.groupRequests(1001).back().id();
        login(0,"10001");selectGroup("1001");tabs->setCurrentIndex(1);row(requests,QString::number(second));
        answer(true);click(window,"rejectGroupRequest");check(!qq->findMember(id(ServiceTypeLJY::QQ,"10006")),"rejection does not join");
        tabs->setCurrentIndex(0);row(members,"10001");
        check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection does not report permission failure");
        click(window,"groupMemberAction");
        check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("不可移除自己"),"self removal explains restriction");
        row(members,"10002");check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection clears old error");click(window,"groupMemberAction");check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("先取消"),"owner cannot directly kick admin");
        check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"),"unavailable member operation is a fixed red error");
        shot("群_点击后权限错误.png");
        row(members,"10004");operation->setCurrentIndex(1);answer(true);click(window,"groupMemberAction");
        check(qq->findMember(id(ServiceTypeLJY::QQ,"10004"))->isAdministrator(),"appoint admin");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("已完成") && window.findChild<QLabel*>("saveStatus")->styleSheet().contains("transparent"),"refresh does not overwrite successful action with selection error");
        login(0,"10002");selectGroup("1001");row(members,"10001");
        check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection does not report permission failure");
        click(window,"groupMemberAction");
        check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("管理员不可移除群主"),"admin cannot kick owner");
        row(members,"10004");check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection clears old error");click(window,"groupMemberAction");check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("管理员不可移除管理员"),"admin cannot kick admin");
        login(0,"10001");selectGroup("1001");row(members,"10004");operation->setCurrentIndex(2);
        answer(true);click(window,"groupMemberAction");check(!qq->findMember(id(ServiceTypeLJY::QQ,"10004"))->isAdministrator(),"remove admin role");
        operation->setCurrentIndex(0);answer(false);click(window,"groupMemberAction");check(qq->findMember(id(ServiceTypeLJY::QQ,"10004")),"cancel kick");
        answer(true);click(window,"groupMemberAction");check(!qq->findMember(id(ServiceTypeLJY::QQ,"10004")),"kick member");
        row(members,"10003");operation->setCurrentIndex(3);answer(false);click(window,"groupMemberAction");
        check(qq->ownerAccountId()==id(ServiceTypeLJY::QQ,"10001"),"cancel transfer");
        answer(true);click(window,"groupMemberAction");check(qq->ownerAccountId()==id(ServiceTypeLJY::QQ,"10003"),"transfer owner");
        answer(true);click(window,"leaveGroupButton");check(!qq->findMember(id(ServiceTypeLJY::QQ,"10001")),"former owner can leave");
        selectGroup("1002");shot("群_成员管理.png");
        for(const auto* invalid:{"9999","11wds"})
        {
            selectGroup(invalid);
            check(!tabs->isEnabled() && members->rowCount()==0 && !window.findChild<QLineEdit*>("groupInviteNumber")->isEnabled(),"invalid group clears all target entry");
            check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#fdeaea"),"group error uses fixed red bar");
        }
        selectGroup("1002");answer(true);click(window,"switchService1");
        check(members->rowCount()==0 && requests->rowCount()==0 && window.findChild<QLineEdit*>("groupNumber")->text().isEmpty(),"service switch clears group selection even same group number");
        selectGroup("1002");row(members,"wx_20002");
        check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection does not report permission failure");
        click(window,"groupMemberAction");
        check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("仅群主"),"WX ordinary member cannot kick");
        operation->setCurrentIndex(1);check(window.findChild<QLabel*>("saveStatus")->text().isEmpty(),"selection clears old error");click(window,"groupMemberAction");check(enabled("groupMemberAction") && window.findChild<QLabel*>("saveStatus")->text().contains("不设管理员"),"WX has no administrator operations");
        tabs->setCurrentIndex(1);input(window,"groupInviteNumber","missing");click(window,"inviteGroupButton");
        check(session.groupRequests(1002).empty(),"invalid invite does not create request");
        input(window,"groupInviteNumber","wx_20004");answer(false);click(window,"inviteGroupButton");check(session.groupRequests(1002).empty(),"cancel invite");
        answer(true);click(window,"inviteGroupButton");
        const int invitation=session.groupRequests(1002).back().id();
        row(requests,QString::number(invitation));click(window,"acceptGroupRequest");check(enabled("acceptGroupRequest") && window.findChild<QLabel*>("saveStatus")->text().contains("被邀请人本人"),"inviter cannot accept for target");
        login(1,"wx_20004");
        bool pendingListed=false;for(int i=0;i<list->count();++i) if(list->item(i)->data(Qt::UserRole).toInt()==1002) pendingListed=true;
        check(pendingListed,"incoming invitation discoverable in group list");
        for(int i=0;i<list->count();++i)
            if(list->item(i)->data(Qt::UserRole).toInt()==1002) { list->setCurrentRow(i); break; }
        app.processEvents();
        check(window.findChild<QLabel*>("groupSummary")->text().contains("1002"),"select pending group from list");
        check(members->rowCount()==0 && enabled("applyGroupButton"),"WX nonmember sees action but no members");
        tabs->setCurrentIndex(1);row(requests,QString::number(invitation));answer(true);click(window,"rejectGroupRequest");
        check(!platform.findGroup(ServiceTypeLJY::WeChat,1002)->findMember(id(ServiceTypeLJY::WeChat,"wx_20004")),"reject invitation");
        login(1,"wx_20001");selectGroup("1002");tabs->setCurrentIndex(1);input(window,"groupInviteNumber","wx_20004");
        answer(true);click(window,"inviteGroupButton");const int next=session.groupRequests(1002).back().id();
        login(1,"wx_20004");selectGroup("1002");tabs->setCurrentIndex(1);row(requests,QString::number(next));
        shot("群_接受邀请.png");
        window.resize(900,600);app.processEvents();
        for(auto* scroll:window.findChildren<QScrollArea*>())
            if(scroll->widget()->objectName()=="groupsPage")
            {
                scroll->ensureWidgetVisible(window.findChild<QPushButton*>("acceptGroupRequest"));
                check(scroll->horizontalScrollBar()->maximum()==0,"selected invitation fits small-window width");
            }
        shot("群_小窗口邀请.png");
        window.resize(1100,780);app.processEvents();
        answer(false);click(window,"acceptGroupRequest");check(members->rowCount()==0,"cancel invitation acceptance");
        answer(true);click(window,"acceptGroupRequest");check(members->rowCount()==4,"invitation acceptance refreshes membership");
        answer(true);click(window,"leaveGroupButton");check(members->rowCount()==0,"WX leave clears members");
        selectGroup("9999");window.resize(900,600);app.processEvents();
        auto* feedback=window.findChild<QLabel*>("saveStatus");
        const QPoint position=feedback->mapTo(&window,QPoint());
        check(window.rect().contains(feedback->mapTo(&window,feedback->rect().bottomRight())),"small window contains error bar");
        for(auto* scroll:window.findChildren<QScrollArea*>())
            if(scroll->widget()->objectName()=="groupsPage")
            {
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                check(scroll->horizontalScrollBar()->maximum()==0,"group page fits small window width");
            }
        app.processEvents();check(feedback->mapTo(&window,QPoint())==position,"group scroll leaves feedback fixed");
        shot("群_小窗口提示.png");
        /* 业务操作成功后自动保存。 */window.close();
        std::cout<<"PASS: group selection, privacy, applications, invitations, detailed permissions, member changes, cancellation, service isolation and small-window feedback\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
