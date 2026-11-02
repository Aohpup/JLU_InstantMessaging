#pragma once
// 网络头放在 Qt 头之前，避免 Windows Socket 头文件重复定义。
#include "network/TcpPeerLJY.h"
#include "services/SessionManagerLJY.h"
#include <QWidget>
#include <QMap>
#include <QDate>
#include <map>
#include <memory>
#include <set>
#include "storage/ChatHistoryLJY.h"

class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QPlainTextEdit;
class QScrollArea;
class QVBoxLayout;
class QTimer;

// 三服务共用文字聊天页：主线程处理控件和会话，网络线程通过事件队列返回结果。
class MessagesPageLJY : public QWidget
{
    Q_OBJECT
public:
    explicit MessagesPageLJY(SessionManagerLJY& session, const QString& databasePath = {}, QWidget* parent = nullptr);
    ~MessagesPageLJY() override;
    // 刷新好友及昵称；身份改变时停止旧连接、清空旧账号的内存聊天。
    void refresh();
    // 当前服务已收到但尚未浏览的消息数。
    int unreadCount();
    // 供主窗口固定显示未落库的聊天记录数量。
    int pendingHistoryCount() const { return static_cast<int>(pending_.size()); }
    // 好友资料页跳转后选择对应会话；换人只切换显示，点击新好友的连接按钮才替换连接。
    void openConversation(const QString& number);
    // 主窗口确认关闭后调用；页面隐藏不停止收发。
    void stopConnection();
    // 重试所有账号待写入的记录；失败仍保留内存，可由保存按钮或退出流程调用。
    bool retryHistory(bool reportSuccess = true);
    // 先停止收发，再处理待保存记录；取消时留在窗口，不静默丢弃。
    bool prepareToClose();
signals:
    // 所有操作反馈交给主窗口已有的固定提示区。
    void feedback(const QString& text, bool failed = false);
    // 查看资料只跳转页面，不改变正在聊天的好友或连接。
    void friendRequested(const QString& number);
protected:
    // Enter 发送，Shift/Ctrl+Enter 换行；窗口变化时重新约束气泡宽度。
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    SessionManagerLJY& session_;
    TcpPeerLJY peer_;
    QString databasePath_;
    std::unique_ptr<ChatHistoryLJY> history_;
    // 保留失败消息所属账号，注销换人后也不会写到新账号名下。
    struct PendingHistoryLJY { std::string owner; ChatMessageLJY message; };
    std::vector<PendingHistoryLJY> pending_;
    std::map<std::string, long long> oldest_;
    std::set<std::string> historyEnded_;
    bool loadingHistory_ = false;
    // 当前身份中已展开的消息；切页和刷新保留，换账号或服务时清空。
    std::set<std::string> expanded_;
    // 浏览对象和连接对象分开，后台消息始终归入实际连接的好友。
    std::string owner_, selected_, connectedPeer_, connection_;
    QString connectionState_ = "尚未连接";
    bool active_ = false;
    bool ready_ = false;
    bool supported_ = false;
    bool connectionFailed_ = false;
    ServiceTypeLJY service_ = ServiceTypeLJY::QQ;
    // 缓存头像资料，删除好友或切换会话时仍可安全处理最后一批网络事件。
    QString ownNickname_, peerNickname_;
    unsigned long long ownPerson_ = 0, peerPerson_ = 0;
    int ownColor_ = 0, peerColor_ = 0;
    // 停留在末尾时跟随新消息；向上翻阅时保留当前位置。
    bool followLatest_ = true;
    // 新操作使旧的延迟定位失效，避免未读定位被上一轮布局撤销。
    int scrollRevision_ = 0;
    QDate lastDate_;
    QPushButton* latest_ = nullptr;
    // 最近摘要只缓存一条，历史正文仍按页读取。
    QMap<QString, QPair<long long,QString>> previews_;
    void updatePreview(const QString& number, const ChatMessageLJY& message);
    void updateReading();
    // 按未读数量找到第一条未读，必要时补读更早的一页。
    void jumpToUnread();
    QListWidget* friends_;
    QLabel *listTitle_, *identity_, *title_, *state_, *avatar_, *empty_, *historyHint_;
    QWidget *panel_, *timeline_, *connectionBar_, *composer_;
    QLabel *connectionHint_, *role_;
    QSpinBox* port_;
    QPushButton *settings_, *start_, *stop_, *send_, *earlier_, *retry_;
    QPlainTextEdit* editor_;
    QScrollArea* scroll_;
    QVBoxLayout* bubbles_;
    QTimer *timer_, *retryTimer_;
    // 自动分工只在开始时确定；保存新设置不会改变正在使用的连接。
    bool autoWaiting_ = false, listening_ = false, handshaking_ = false, attempted_ = false;
    long long waitStarted_ = 0;
    QString attemptAddress_, settingsPath_;
    int attemptPort_ = 27015, localPort_ = 27015;
    // 当前显示缓存和草稿按好友分开；正常模式的记录同时逐条写库。
    std::map<std::string, std::vector<ChatMessageLJY>> conversations_;
    QMap<QString, QString> drafts_;
    QMap<QString,int> unread_;
    QMap<QString, QLabel*> deliveryLabels_;
    // 按需打开历史库；失败可再次尝试，不阻断网络消息显示。
    ChatHistoryLJY& history();
    bool storeMessage(const ChatMessageLJY& message);
    void loadHistory(bool earlier = false);
    void updateHistoryControls();
    void selectFriend();
    // 数字账号按数值比较，其他账号按字符串比较，双方得到互补的角色。
    bool shouldListen() const;
    void beginConnection();
    void startConnection();
    // 配置使用独立 INI；本机与两机分别保存，不修改聊天数据库。
    void loadConnectionFields();
    void saveConnectionSettings();
    void sendText();
    void pollEvents();
    void updateControls();
    void renderConversation();
    void appendBubble(const ChatMessageLJY& message);
    void fitBubbles();
    void updateDeliveryLabel(QLabel* label, const ChatMessageLJY& message);
};
