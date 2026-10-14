#include "network/GroupChatHostLJY.h"
#include "network/GroupChatProtocolLJY.h"
#include "network/ChatProtocolLJY.h"
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>
#include <algorithm>

GroupChatHostLJY::GroupChatHostLJY(QObject *parent) : QObject(parent)
{
    connect(&server_, &QTcpServer::newConnection, this, &GroupChatHostLJY::acceptPeers);
}
bool GroupChatHostLJY::start(const QString &room, int port, bool lan, const QString &token,
                             const GroupChatMembersLJY &members)
{
    stop();
    room_ = room;
    token_ = token;
    members_ = members;
    sequence_ = 0;
    const auto address = lan ? QHostAddress::AnyIPv4 : QHostAddress::LocalHost;
    return server_.listen(address, port) || (lan && server_.listen(address, 0));
}
void GroupChatHostLJY::stop()
{
    server_.close();
    for (auto *socket : peers_.keys())
    {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    peers_.clear();
}
void GroupChatHostLJY::send(QTcpSocket *socket, const QJsonObject &frame)
{
    if (socket->bytesToWrite() > 512 * 1024)
    {
        socket->abort();
        return;
    }
    socket->write(GroupChatProtocolLJY::encode(frame));
}
void GroupChatHostLJY::broadcast(const QJsonObject &frame)
{
    for (auto *socket : peers_.keys())
        if (!peers_.value(socket).account.isEmpty())
            send(socket, frame);
}
void GroupChatHostLJY::roster()
{
    QJsonArray accounts;
    for (const auto &peer : peers_)
        if (!peer.account.isEmpty())
            accounts.append(peer.account);
    broadcast({{"kind", "roster"}, {"accounts", accounts}});
}
void GroupChatHostLJY::reject(QTcpSocket *socket, const QString &why)
{
    send(socket, {{"kind", "reject"}, {"reason", why}});
    socket->disconnectFromHost();
}
void GroupChatHostLJY::updateMembers(const GroupChatMembersLJY &members)
{
    members_ = members;
    for (auto *socket : peers_.keys())
    {
        const auto account = peers_.value(socket).account;
        if (!account.isEmpty() && !members_.contains(account))
            reject(socket, "你已不在主持端的群成员名单中。");
    }
}
void GroupChatHostLJY::acceptPeers()
{
    while (auto *socket = server_.nextPendingConnection())
    {
        socket->setParent(this);
        socket->setReadBufferSize(128 * 1024);
        peers_.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { receive(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            peers_.remove(socket);
            socket->deleteLater();
            roster();
        });
        QTimer::singleShot(5000, socket, [this, socket] {
            if (peers_.contains(socket) && peers_.value(socket).account.isEmpty())
                reject(socket, "群身份握手超时。");
        });
    }
}
void GroupChatHostLJY::receive(QTcpSocket *socket)
{
    try
    {
        auto frames = GroupChatProtocolLJY::feed(peers_[socket].buffer, socket->readAll());
        for (const auto &frame : frames)
        {
            if (!peers_.contains(socket))
                return;
            const auto kind = frame.value("kind").toString();
            if (peers_[socket].account.isEmpty())
            {
                const auto account = frame.value("account").toString();
                if (kind != "hello" || frame.value("protocol").toInt() != 1 ||
                    frame.value("room").toString() != room_)
                {
                    reject(socket, "群聊协议、服务或群号不匹配，请核对房间端口。");
                    return;
                }
                if (!members_.contains(account))
                {
                    reject(socket, "主持端未找到你的群成员身份，请先同步群资料。");
                    return;
                }
                for (const auto &peer : peers_)
                    if (peer.account == account)
                    {
                        reject(socket, "此账号已经进入该群房间。");
                        return;
                    }
                peers_[socket].account = account;
                peers_[socket].color =
                    std::clamp(frame.value("color").toInt(members_.value(account).second), 0, 5);
                send(socket, {{"kind", "welcome"}, {"room", room_}, {"token", token_}});
                roster();
            }
            else if (kind == "text")
            {
                const auto account = peers_[socket].account;
                if (!members_.contains(account))
                {
                    reject(socket, "你已经不在该群中。");
                    return;
                }
                const auto body = frame.value("body").toString();
                ChatProtocolLJY::validateBody(body.toUtf8().toStdString());
                const auto request = frame.value("request").toString();
                if (request.isEmpty() || request.size() > 80)
                {
                    reject(socket, "消息编号无效。");
                    return;
                }
                const auto info = members_.value(account);
                broadcast({{"kind", "message"},
                           {"room", room_},
                           {"id", token_ + ":" + QString::number(++sequence_)},
                           {"sender", account},
                           {"nickname", info.first},
                           {"color", peers_[socket].color},
                           {"body", body},
                           {"request", request},
                           {"time", QString::number(QDateTime::currentMSecsSinceEpoch())}});
            }
            else
            {
                reject(socket, "不支持的群消息类型。");
                return;
            }
        }
    }
    catch (const std::exception &e)
    {
        reject(socket, QString::fromUtf8(e.what()));
    }
}
