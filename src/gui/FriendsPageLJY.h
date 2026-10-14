#pragma once

#include <QWidget>
#include "services/SessionManagerLJY.h"

class QListWidget;
class QLineEdit;
class QLabel;
class QComboBox;
class QPushButton;
class QPlainTextEdit;

// 当前服务的好友页面；界面只保存号码，好友关系和映射规则由会话处理。
class FriendsPageLJY : public QWidget
{
    Q_OBJECT
public:
    explicit FriendsPageLJY(SessionManagerLJY& session, QWidget* parent = nullptr);
    // 修改或切换服务后刷新；切换身份时清除旧选择及查询结果。
    void refresh();
    // 从消息列表跳转到“我的好友”页签并选中指定好友。
    void openFriend(const QString& number);

signals:
    // 打开当前好友的消息页，具体连接操作由消息页完成。
    void messageRequested(const QString& number);
    // 将操作结果发送到主窗口固定提示区，不随页面滚动。
    void feedback(const QString& message, bool failed = false);
    // 通知主窗口有内存修改，仍使用原来的统一保存动作。
    void changed(const QString& message);

private:
    SessionManagerLJY& session_;
    int accountId_ = 0;
    QListWidget *friends_, *results_, *requests_;
    QPushButton* queryButton_;
    int cityOffset_ = 0;
    // 收发记录共用最近一条申请，只有收件人可同意或拒绝。
    void showRequests();
    void handleRequest(bool accept);
    QLineEdit *number_, *remark_, *commonNumber_;
    QLabel *identity_, *details_, *commonLabel_, *sourceLabel_;
    // 好友资料卡中的昵称首字头像、昵称和号码。
    QLabel *avatar_, *nickname_, *friendNumber_;
    QComboBox *queryType_, *source_;
    QPushButton *remarkButton_, *removeButton_, *recommendButton_, *messageButton_;
    QPlainTextEdit* reasons_;
    QWidget* remarkEditor_;
    bool recommendationReady_ = false;
    ServiceTypeLJY resultSource_ = ServiceTypeLJY::QQ;
    // 读取选中好友的本人可见资料和独立备注。
    void showFriend();
    // 搜索添加只发送申请，收到申请的账号在申请页处理。
    void addFriend();
    void removeFriend();
    // 在原页面展开备注输入并隐藏其他操作；保存后收起，取消或换人不提交。
    void beginRemarkEdit();
    void saveRemark();
    // 查询结果中的号码属于当前服务；未入选原因直接展示业务返回值。
    void query();
    void addRecommendation();
    // 查询条件改变后清空旧结果，避免把旧号码用于新条件。
    void clearQuery();
    bool confirm(const QString& text);
};
