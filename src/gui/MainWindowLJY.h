#pragma once

#include <QMainWindow>
#include <functional>
#include "services/SessionManagerLJY.h"

class LoginPageLJY;
class AccountPageLJY;
class FriendsPageLJY;
class GroupsPageLJY;
class MessagesPageLJY;
class GroupMessagesPageLJY;
class QTabWidget;
class QStackedWidget;
class QPushButton;
class QLabel;

// 图形入口主窗口：管理登录页、服务切换及保存关闭，不包含 SQL。
class MainWindowLJY : public QMainWindow
{
    Q_OBJECT
public:
    // 会话和保存动作引用的对象必须比窗口活得更久；空动作表示内存模式。
    MainWindowLJY(SessionManagerLJY& session, std::function<void()> saveAction,
                  const QString& databasePath, QWidget* parent = nullptr);

protected:
    // 正常关闭先保存；失败可重试、取消关闭或明确放弃。
    void closeEvent(QCloseEvent* event) override;

private:
    SessionManagerLJY& session_;
    std::function<void()> saveAction_;
    LoginPageLJY* login_;
    AccountPageLJY* account_;
    QStackedWidget* pages_;
    QStackedWidget* content_;
    FriendsPageLJY* friends_;
    GroupsPageLJY* groups_;
    MessagesPageLJY* messages_;
    GroupMessagesPageLJY* groupMessages_;
    QTabWidget* messageTabs_;
    QPushButton *messagesNav_, *accountNav_, *friendsNav_, *groupsNav_;
    QPushButton* serviceButtons_[3];
    QPushButton* logoutButton_;
    QLabel* status_;
    // 侧栏显示当前昵称与号码，未登录时显示引导文字。
    QLabel *identity_, *identityAvatar_, *identityNumber_;
    QPushButton* identityCard_;
    // 根据当前身份刷新服务按钮和账号页，当前服务不允许重复切换。
    void refresh();
    void switchService(ServiceTypeLJY type);
    // 修改发生在内存；保存成功后才提示已经写入数据库。
    void markChanged(const QString& message);
    bool save();
};
