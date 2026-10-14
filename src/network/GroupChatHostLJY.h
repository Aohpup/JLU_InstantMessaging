#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QHash>
#include <QJsonObject>
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
    // 首选群默认端口，局域网可退回系统分配的空闲端口。
    bool start(const QString &room, int port, bool lan, const QString &token,
               const GroupChatMembersLJY &members);
    void stop();
    bool running() const
    {
        return server_.isListening();
    }
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
        int color = 0;
    };
    QTcpServer server_;
    QHash<QTcpSocket *, PeerLJY> peers_;
    GroupChatMembersLJY members_;
    QString room_, token_;
    long long sequence_ = 0;
    void acceptPeers();
    void receive(QTcpSocket *socket);
    void send(QTcpSocket *socket, const QJsonObject &frame);
    void broadcast(const QJsonObject &frame);
    void roster();
    void reject(QTcpSocket *socket, const QString &why);
};
