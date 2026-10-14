#pragma once
#include "network/GroupChatHostLJY.h"
#include <QUdpSocket>
#include <QTimer>

// 异步群客户端：进入即发现房间；无房间时开房，普通成员也可临时主持。
class GroupChatClientLJY : public QObject
{
    Q_OBJECT
  public:
    explicit GroupChatClientLJY(QObject *parent = nullptr);
    ~GroupChatClientLJY() override;
    // 同一服务、群号在不同进程中得到相同默认端口。
    static int defaultPort(int service, int group);
    // 自动入场；指定 IP 时只连接已有房间，不另开孤立房间。
    void enter(const QString &room, const QString &account, const GroupChatMembersLJY &members, bool lan,
               int port, const QString &fixedIp = {});
    // 停止定时发现、客户端连接及本机主持端。
    void leave();
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
    void connectionChanged(bool ready);

  private:
    GroupChatHostLJY host_;
    QTcpSocket socket_;
    QUdpSocket discovery_;
    QTimer tick_;
    QByteArray buffer_;
    GroupChatMembersLJY members_;
    QString room_, account_, token_, fixedIp_, connectedToken_;
    bool active_ = false, ready_ = false, lan_ = false, rejected_ = false;
    int port_ = 0;
    long long nextAttempt_ = 0, connectStarted_ = 0, lastAnnouncement_ = 0;
    // 无房间时尝试占用端口，成功则开房，失败则加入本机已有房间。
    void attempt();
    void connectTo(const QString &ip, int port);
    void receive();
    void disconnected();
    void announce(bool query);
    // 按群标识过滤发现报文，同时开房时收敛到标识较小的主持端。
    void discover();
    void tick();
};
