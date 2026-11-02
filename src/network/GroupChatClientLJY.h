#pragma once
#include "network/GroupChatHostLJY.h"
#include <QTimer>

// 异步群客户端：本机自动连接持久服务，内存练习由本机窗口临时开房。
class GroupChatClientLJY : public QObject
{
    Q_OBJECT
  public:
    explicit GroupChatClientLJY(QObject *parent = nullptr);
    ~GroupChatClientLJY() override;
    // 同一服务、群号在不同进程中得到相同默认端口。
    static int defaultPort(int service, int group);
    // 使用本机回环地址自动加入房间，所有窗口使用相同端口。
    void enter(const QString &room, const QString &account, const GroupChatMembersLJY &members, int port);
    // 停止客户端和临时主持端；独立持久服务不随页面退出。
    void leave();
    // 本机模式自动启动独立持久房间；目录为空时仅用原有临时房间。
    void setServerDirectory(const QString &directory) { serverDirectory_ = directory; }
    // 持久模式不能把旧版临时房间误当成完整历史来源。
    void requirePersistent(bool value) { requirePersistent_ = value; }
    bool historyComplete() const { return historyComplete_; }
    bool syncing() const { return syncing_; }
    // 手动重试完整同步，用于本地记录保存失败后重新取回。
    void syncHistory();
    bool active() const { return active_; }
    bool ready() const
    {
        return ready_;
    }
    bool hosting() const
    {
        return host_.running();
    }
    QString endpoint() const;
    // 返回提交编号；收到主持端回传才算房间接收，断线不自动重发。
    QString sendText(const QString &body);
    void updateMembers(const GroupChatMembersLJY &members);
  signals:
    void status(const QString &text, bool failed = false);
    void message(const QJsonObject &frame);
    void online(int count);
    // 完整名单由主持端返回，在线状态单独列出。
    void rosterReceived(const QJsonObject& roster);
    void historySynchronized();
    void connectionChanged(bool ready);

  private:
    GroupChatHostLJY host_;
    QTcpSocket socket_;
    QTimer tick_;
    QByteArray buffer_;
    GroupChatMembersLJY members_;
    QString room_, account_, token_, serverDirectory_;
    bool requirePersistent_ = false;
    bool durable_ = false, syncing_ = false, historyComplete_ = false;
    QVector<QJsonObject> liveDuringSync_;
    long long nextLaunch_ = 0;
    void launchServer();
    void writeMembers();
    bool active_ = false, ready_ = false, rejected_ = false;
    int port_ = 0;
    long long nextAttempt_ = 0, connectStarted_ = 0;
    // 本机先连接，必要时启动独立服务；临时模式沿用原有开房流程。
    void attempt();
    void connectTo(const QString &ip, int port);
    void receive();
    void disconnected();
    void tick();
};
