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
    // 汇总未读；当前页面位于最新消息时同时更新已读位置。
    int unreadCount();
    // 供主窗口固定显示未落库的聊天记录数量。
    int pendingHistoryCount() const { return static_cast<int>(pending_.size()); }
    // 成员操作后合并名单；普通刷新不能重建已有权威名单。
    void publishMemberChanges();
    // 群管理入口调用，选择对应群后自动加入。
    void openGroup(int number);
    // 停止前台和后台连接并保存草稿；持久房间继续保存其他成员消息。
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
    QMap<int, GroupChatClientLJY*> background_;
    QMap<int, GroupChatMembersLJY> rosterSnapshots_;
    QString serverDirectory_;
    QMap<int,int> memoryUnread_;
    QMap<int,QSet<QString>> seen_;
    void refreshBackground();
    GroupChatMembersLJY membersFor(int number) const;
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
    QListWidget *groups_, *roomMembers_;
    QLabel *title_, *state_, *count_, *historyHint_;
    QPlainTextEdit *editor_;
    QPushButton *send_, *settings_, *earlier_, *retry_;
    QSpinBox *port_;
    QWidget *settingsPanel_, *composer_, *timeline_;
    QScrollArea *scroll_;
    QVBoxLayout *bubbles_;
    bool followLatest_ = true;
    // 新操作使旧的延迟定位失效，避免未读定位被上一轮布局撤销。
    int scrollRevision_ = 0;
    QPushButton* latest_ = nullptr;
    int newMessages_ = 0;
    QMap<int, QPair<long long,QString>> previews_;
    // 刷新摘要不重建聊天，也不改变当前选群。
    void updatePreview(int group, const GroupChatMessageLJY& message);
    void updateReading();
    // 按已读位置定位第一条未读；跨页时自动补读本地记录。
    void jumpToUnread();
    QString draftKey() const;
    // 从现有群业务取得允许入场的账号和头像资料。
    GroupChatMembersLJY members() const;
    void selectGroup();
    // 读取群配置，自动连接持久服务或进入兼容的临时房间。
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
