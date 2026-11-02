#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QHash>
#include <QJsonObject>
#include "storage/GroupRoomStoreLJY.h"
#include <memory>
// 成员资料来自主持端现有群：账号映射到昵称和预设头像颜色。
using GroupChatMembersLJY = QMap<QString, QPair<QString, int>>;

class GroupChatHostLJY : public QObject
{
    Q_OBJECT
  public:
    explicit GroupChatHostLJY(QObject *parent = nullptr);
    ~GroupChatHostLJY() override
    {
        stop();
    }
    // 只监听本机回环地址，端口占用时交由客户端连接已有房间。
    bool start(const QString &room, int port, const QString &token,
               const GroupChatMembersLJY &members);
    void stop();
    // 启用独立房间日志；必须在启动前设置，空路径保留临时演示行为。
    void setDatabase(const QString &path) { databasePath_ = path; }
    bool running() const
    {
        return server_.isListening();
    }
    int connectedCount() const { return peers_.size(); }
    int port() const
    {
        return server_.serverPort();
    }
    // 更新允许入场的成员，移除成员时立即断开对应连接。
    void updateMembers(const GroupChatMembersLJY &members);

  private:
    struct PeerLJY
    {
        QByteArray buffer;
        QString account;
        long long historyThrough = 0;
        int color = 0;
    };
    QTcpServer server_;
    QHash<QTcpSocket *, PeerLJY> peers_;
    GroupChatMembersLJY members_;
    QString room_, token_, databasePath_;
    std::unique_ptr<GroupRoomStoreLJY> store_;
    long long sequence_ = 0;
    void acceptPeers();
    void receive(QTcpSocket *socket);
    void send(QTcpSocket *socket, const QJsonObject &frame);
    void broadcast(const QJsonObject &frame);
    void roster();
    void reject(QTcpSocket *socket, const QString &why);
};
