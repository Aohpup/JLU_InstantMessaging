#pragma once
#include "services/SessionManagerLJY.h"
#include "network/GroupChatClientLJY.h"
#include "storage/GroupChatHistoryLJY.h"
#include <QWidget>
#include <QMap>
#include <QSet>
#include <memory>
class QListWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QComboBox;
class QSpinBox;
class QLineEdit;
// 群聊页只协调现有群资料、房间连接和显示，不复制群管理业务。
class GroupMessagesPageLJY : public QWidget
{
    Q_OBJECT
  public:
    explicit GroupMessagesPageLJY(SessionManagerLJY &session, const QString &databasePath,
                                  QWidget *parent = nullptr);
    ~GroupMessagesPageLJY() override;
    // 刷新本人群列表；身份变化或被移出当前群时停止连接。
    void refresh();
    // 群管理入口调用，选择对应群后自动加入。
    void openGroup(int number);
    // 停止自动找房和转发，保存当前草稿到内存。
    void stop();
    // 重试各群待保存记录，记录仍按原账号和群号写入。
    bool retryHistory(bool report = true);
    // 退出前处理待保存记录，允许重试、取消或明确放弃。
    bool prepareToClose();
  signals:
    void feedback(const QString &text, bool failed = false);

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;

  private:
    SessionManagerLJY &session_;
    GroupChatClientLJY room_;
    QString databasePath_, owner_, settingsPath_;
    int service_ = 0, group_ = 0, online_ = 0;
    long long oldest_ = 0;
    std::unique_ptr<GroupChatHistoryLJY> history_;
    struct PendingGroupHistoryLJY
    {
        int service, group;
        QString owner;
        GroupChatMessageLJY message;
    };
    std::vector<PendingGroupHistoryLJY> pending_;
    std::vector<GroupChatMessageLJY> messages_;
    QMap<QString, QString> drafts_, sending_;
    QMap<QString, std::vector<GroupChatMessageLJY>> cached_;
    QSet<QString> expanded_;
    QMap<QString, long long> sendTimes_;
    QListWidget *groups_;
    QLabel *title_, *state_, *count_, *historyHint_;
    QPlainTextEdit *editor_;
    QPushButton *send_, *settings_, *earlier_, *retry_;
    QComboBox *mode_;
    QSpinBox *port_;
    QLineEdit *ip_;
    QWidget *settingsPanel_, *composer_, *timeline_;
    QScrollArea *scroll_;
    QVBoxLayout *bubbles_;
    bool followLatest_ = true;
    QString draftKey() const;
    // 从现有群业务取得允许入场的账号和头像资料。
    GroupChatMembersLJY members() const;
    void selectGroup();
    // 读取该群连接配置，再交给网络模块自动开房或加入。
    void join();
    void loadSettings();
    void sendText();
    // 房间回传确认后显示并保存，去掉自己的待确认编号。
    void receive(const QJsonObject &frame);
    // 每批读取 100 条；主动加载更早记录后定位到旧消息，并标明是否已到最早。
    void loadHistory(bool earlier = false);
    void render();
    // 随窗口宽度重新排版，长正文只限制显示高度。
    void fitBubbles();
    // 断线或确认超时，把结果未知的正文放回输入框供检查。
    void preserveUnconfirmed();
    GroupChatHistoryLJY &history();
};
