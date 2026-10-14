#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/AvatarLJY.h"
#include "gui/FriendsPageLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isEnabled() && button->isVisible(), name);
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}

static void input(QWidget& window, const char* name, const QString& value)
{
    window.findChild<QLineEdit*>(name)->setText(value);
}

static void answer(QMessageBox::StandardButton choice) { answerDialogLJY(choice); }

// 根据存储在列表项里的账号号码选择，避免把昵称或行号误当业务编号。
static void select(QListWidget* list, const QString& number)
{
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == number) { list->setCurrentRow(i); return; }
    throw std::runtime_error("account missing from list");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        const auto directory = std::filesystem::u8path(JLU_GUI_TEST_DIR);
        std::filesystem::create_directories(directory);
        const auto path = directory / "friends.db";
        const bool reload = app.arguments().contains("--reload");
        if (!reload) std::filesystem::remove(path);
        PlatformLJY platform;
        SqliteStorageLJY storage(path.u8string());
        if (reload) storage.load(platform); else loadDemoDataLJY(platform);
        SessionManagerLJY session(platform);
        MainWindowLJY window(session, [&] { storage.save(platform); }, QString::fromStdWString(path.wstring()));
        window.show();
        input(window, "loginNumber", "10001");
        input(window, "loginPassword", "DemoOnly123!");
        click(window, "loginButton");
        click(window, "friendsNav");
        auto* list = window.findChild<QListWidget*>("friendsList");
        auto* tabs = window.findChild<QTabWidget*>("friendTabs");
        auto* mode = window.findChild<QComboBox*>("friendQueryType");
        auto* source = window.findChild<QComboBox*>("friendSourceService");
        auto* results = window.findChild<QListWidget*>("friendQueryResults");
        if (reload)
        {
            check(list->count() == 3, "reload list after add/remove");
            check(session.friendRemark("10002") == "课程同学", "reload remark");
            check(session.friendAccount("10005").nickname() == "演示EQQ", "reload added friend");
            check(!platform.findFriendRelation(session.currentAccount().accountId(), platform.findAccount(ServiceTypeLJY::QQ,"10003")->accountId()), "reload removal");
            answer(QMessageBox::Yes);
            click(window, "switchService1");
            check(session.friendAccount("wx_20002").nickname() == "演示B微信", "reload recommendation");
            window.close();
            std::cout << "PASS: GUI friend changes restored in a separate process\n";
            return 0;
        }
        check(list->count() == 3, "demo friend list");
        select(list, "10002");
        check(window.findChild<QLabel*>("friendDetails")->text().contains("演示B"), "friend profile shown");
        click(window, "friendRemarkButton");
        input(window, "friendRemark", "课程同学");
        click(window, "saveRemarkButton");
        check(session.friendRemark("10002") == "课程同学", "remark updated");
        check(!window.findChild<QLineEdit*>("friendRemark")->isVisible(), "remark editor collapses after save");
        click(window, "friendRemarkButton");
        check(window.findChild<QLineEdit*>("friendRemark")->isVisible(), "remark editor opens inline");
        check(!window.findChild<QPushButton*>("removeFriendButton")->isVisible(), "editing hides delete action");
        input(window, "friendRemark", "未提交的备注");
        window.grab().save(QString::fromStdWString((directory / "remark-editor.png").wstring()));
        click(window, "cancelRemarkButton");
        check(session.friendRemark("10002") == "课程同学" && !window.findChild<QLineEdit*>("friendRemark")->isVisible(), "cancel keeps original remark");
        check(window.findChild<QPushButton*>("removeFriendButton")->isVisible(), "cancel restores delete action");
        const auto* b = platform.findAccount(ServiceTypeLJY::QQ,"10002");
        const auto* relation = platform.findFriendRelation(session.currentAccount().accountId(), b->accountId());
        check(relation && relation->remarkFor(b->accountId()).empty(), "peer remark remains independent");
        click(window, "friendRemarkButton");
        input(window, "friendRemark", "");
        click(window, "saveRemarkButton");
        check(session.friendRemark("10002").empty(), "clear remark");
        click(window, "friendRemarkButton");
        input(window, "friendRemark", "课程同学");
        click(window, "saveRemarkButton");
        for (const auto& number : {"10001", "10002", "99999"})
        {
            input(window, "addFriendNumber", number);
            click(window, "addFriendButton");
            check(window.findChild<QLabel*>("saveStatus")->text().contains("操作失败"), "candidate rejected before confirmation");
            check(list->count() == 3, "failed addition changed list");
            check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#b42318"), "friend failure is red");
        }
        // 小窗口滚动只影响页面内容，操作结果仍在数据库路径上方。
        window.resize(900, 600);
        QApplication::processEvents();
        auto* feedback = window.findChild<QLabel*>("saveStatus");
        auto* databasePath = window.findChild<QLabel*>("databasePath");
        auto* page = window.findChild<QWidget*>("friendsPage");
        QScrollArea* scroll = nullptr;
        for (auto* area : window.findChildren<QScrollArea*>())
            if (area->widget() == page) scroll = area;
        check(scroll != nullptr, "friend scroll area exists");
        const QPoint feedbackPosition = feedback->mapTo(&window, QPoint());
        check(feedback->isVisible() && feedbackPosition.y() + feedback->height() <=
              databasePath->mapTo(&window, QPoint()).y(), "small-window feedback above database path");
        check(window.rect().contains(feedbackPosition) &&
              window.rect().contains(feedback->mapTo(&window, feedback->rect().bottomRight())), "entire feedback stays inside window");
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QApplication::processEvents();
        check(feedback->mapTo(&window, QPoint()) == feedbackPosition, "feedback stays fixed while friends scroll");
        check(scroll->horizontalScrollBar()->maximum() == 0, "friend cards fit small-window width");
        window.grab().save(QString::fromStdWString((directory / "friends-small-window.png").wstring()));
        list->setCurrentRow(-1);
        check(!window.findChild<QPushButton*>("friendRemarkButton")->isEnabled() &&
              !window.findChild<QPushButton*>("removeFriendButton")->isEnabled(), "no friend disables actions");
        select(list, "10002");
        window.resize(1100, 780);
        scroll->verticalScrollBar()->setValue(0);
        QApplication::processEvents();
        tabs->setCurrentIndex(1);
        input(window, "commonFriendNumber", "10002");
        click(window, "friendQueryButton");
        check(results->count() == 1 && results->item(0)->data(Qt::UserRole).toString() == "10003", "same-service common friends");
        mode->setCurrentIndex(1);
        source->setCurrentIndex(source->findData(static_cast<int>(ServiceTypeLJY::WeChat)));
        click(window, "friendQueryButton");
        check(results->count() == 1 && results->item(0)->data(Qt::UserRole).toString() == "10004", "cross-service common mapped to QQ");
        auto* addResult=window.findChild<QPushButton*>("addRecommendationButton");
        check(addResult->isVisible() && !addResult->isEnabled() && addResult->text()=="已是好友", "common result has visible existing-friend action");
        tabs->setCurrentIndex(0);
        input(window, "addFriendNumber", "10005");
        answer(QMessageBox::No);
        click(window, "addFriendButton");
        check(list->count() == 3, "cancel add");
        answer(QMessageBox::Yes);
        click(window, "addFriendButton");
        check(list->count() == 3, "sending request does not add friend");
        { SessionManagerLJY recipient(platform); recipient.login(ServiceTypeLJY::QQ,"10005","DemoOnly123!"); recipient.handleFriendRequest("10001",true); }
        window.findChild<FriendsPageLJY*>()->refresh();
        check(list->count()==4,"recipient approval adds friend");
        select(list, "10003");
        answer(QMessageBox::No);
        click(window, "removeFriendButton");
        check(list->count() == 4, "cancel remove");
        answer(QMessageBox::Yes);
        click(window, "removeFriendButton");
        check(list->count() == 3, "remove friend");
        select(list, "10002");
        QLabel sample;
        applyAvatarColorLJY(&sample,session.friendPerson("10002").personId());
        check(window.findChild<QLabel*>("friendAvatar")->styleSheet()==sample.styleSheet(), "friend uses same person color");
        window.grab().save(QString::fromStdWString((directory / "friends-page.png").wstring()));
        answer(QMessageBox::Yes);
        click(window, "switchService1");
        check(list->count() == 1 && results->count() == 0, "switch refreshes list and clears query");
        tabs->setCurrentIndex(1);
        mode->setCurrentIndex(2);
        source->setCurrentIndex(source->findData(static_cast<int>(ServiceTypeLJY::QQ)));
        click(window, "friendQueryButton");
        check(results->count() == 1 && results->item(0)->data(Qt::UserRole).toString() == "wx_20002", "recommend B to WeChat");
        check(addResult->isVisible() && addResult->isEnabled() && addResult->text()=="发送好友申请", "recommendation add action visible");
        const auto reasons = window.findChild<QPlainTextEdit*>("friendQueryReasons")->toPlainText();
        check(reasons.contains("未开通目标服务") && reasons.contains("已经是当前服务好友"), "show skip reasons");
        window.grab().save(QString::fromStdWString((directory / "recommendations-page.png").wstring()));
        source->setCurrentIndex(source->findData(static_cast<int>(ServiceTypeLJY::Weibo)));
        check(results->count() == 0 && !window.findChild<QPushButton*>("addRecommendationButton")->isEnabled(), "source change invalidates old results");
        source->setCurrentIndex(source->findData(static_cast<int>(ServiceTypeLJY::QQ)));
        click(window, "friendQueryButton");
        answer(QMessageBox::No);
        click(window, "addRecommendationButton");
        check(list->count() == 1, "cancel recommendation");
        answer(QMessageBox::Yes);
        click(window, "addRecommendationButton");
        check(list->count()==1 && results->count()==0,"recommendation sends pending request");
        { SessionManagerLJY recipient(platform); recipient.login(ServiceTypeLJY::WeChat,"wx_20002","DemoOnly123!"); recipient.handleFriendRequest("wx_20001",true); }
        window.findChild<FriendsPageLJY*>()->refresh();
        check(list->count()==2,"approved recommendation updates list");
        click(window, "saveButton");
        click(window, "logoutButton");
        check(list->count() == 0 && results->count() == 0, "logout clears previous account data");
        input(window, "loginNumber", "10003");
        input(window, "loginPassword", "DemoOnly123!");
        click(window, "loginButton");
        click(window, "friendsNav");
        check(list->count() == 1, "other identity gets its own list");
        tabs->setCurrentIndex(1);
        click(window, "friendQueryButton");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("开通另一项服务"), "no source service explained");
        window.close();
        std::cout << "PASS: friend widgets, CRUD, independent remarks, common queries, recommendation, cancellation, service/identity switch\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
