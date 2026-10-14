#include "network/GroupChatClientLJY.h"
#include "network/GroupChatProtocolLJY.h"
#include "network/ChatProtocolLJY.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUuid>
#include <stdexcept>
namespace
{
const QHostAddress discoveryAddress("239.255.42.99");
constexpr int discoveryPort = 45045;
long long now()
{
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace
GroupChatClientLJY::GroupChatClientLJY(QObject *parent) : QObject(parent)
{
    socket_.setReadBufferSize(128 * 1024);
    tick_.setInterval(250);
    connect(&tick_, &QTimer::timeout, this, &GroupChatClientLJY::tick);
    connect(&socket_, &QTcpSocket::connected, this, [this] {
        socket_.write(GroupChatProtocolLJY::encode({{"kind", "hello"},
                                                    {"protocol", 1},
                                                    {"room", room_},
                                                    {"account", account_},
                                                    {"color", members_.value(account_).second}}));
    });
    connect(&socket_, &QTcpSocket::readyRead, this, &GroupChatClientLJY::receive);
    connect(&socket_, &QTcpSocket::disconnected, this, &GroupChatClientLJY::disconnected);
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { disconnected(); });
    connect(&discovery_, &QUdpSocket::readyRead, this, &GroupChatClientLJY::discover);
}
GroupChatClientLJY::~GroupChatClientLJY()
{
    leave();
}
int GroupChatClientLJY::defaultPort(int service, int group)
{
    return 32000 + (service * 997 + group) % 20000;
}
QString GroupChatClientLJY::endpoint() const
{
    return socket_.peerAddress().toString() + ":" + QString::number(socket_.peerPort());
}
void GroupChatClientLJY::enter(const QString &room, const QString &account,
                               const GroupChatMembersLJY &members, bool lan, int port, const QString &fixedIp)
{
    leave();
    room_ = room;
    account_ = account;
    members_ = members;
    lan_ = lan;
    port_ = port;
    fixedIp_ = fixedIp;
    token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    active_ = true;
    rejected_ = false;
    lastAnnouncement_ = 0;
    nextAttempt_ = now() + (lan ? 1000 : 100);
    if (lan && fixedIp.isEmpty())
    {
        if (!discovery_.bind(QHostAddress::AnyIPv4, discoveryPort,
                             QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        {
            active_ = false;
            emit status("局域网发现端口不可用，可在更多设置指定房间 IP。", true);
            return;
        }
        discovery_.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
        for (const auto &adapter : QNetworkInterface::allInterfaces())
            if ((adapter.flags() & QNetworkInterface::IsUp) &&
                (adapter.flags() & QNetworkInterface::CanMulticast))
                discovery_.joinMulticastGroup(discoveryAddress, adapter);
        announce(true);
    }
    tick_.start();
    emit status("正在自动加入群聊…");
}
void GroupChatClientLJY::leave()
{
    active_ = false;
    tick_.stop();
    ready_ = false;
    socket_.abort();
    discovery_.close();
    host_.stop();
    buffer_.clear();
    connectedToken_.clear();
    emit connectionChanged(false);
    emit online(0);
}
void GroupChatClientLJY::updateMembers(const GroupChatMembersLJY &members)
{
    members_ = members;
    host_.updateMembers(members);
}
void GroupChatClientLJY::connectTo(const QString &ip, int port)
{
    socket_.abort();
    buffer_.clear();
    ready_ = false;
    connectStarted_ = now();
    socket_.connectToHost(ip, port);
}
void GroupChatClientLJY::attempt()
{
    nextAttempt_ = now() + 1500;
    if (!fixedIp_.isEmpty())
    {
        connectTo(fixedIp_, port_);
        return;
    }
    if (host_.running())
    {
        connectTo("127.0.0.1", host_.port());
        return;
    }
    // 先尝试占用默认端口；占用失败说明可能已有本机房间，直接尝试加入。
    token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (host_.start(room_, port_, lan_, token_, members_))
    {
        connectTo("127.0.0.1", host_.port());
        if (lan_)
            announce(false);
    }
    else if (!lan_)
        connectTo("127.0.0.1", port_);
    else
        emit status("无法开启群房间，请检查端口或网络权限。", true);
}
void GroupChatClientLJY::disconnected()
{
    const bool wasReady = ready_;
    ready_ = false;
    buffer_.clear();
    emit connectionChanged(false);
    emit online(0);
    if (active_ && !rejected_)
    {
        nextAttempt_ = now() + 750;
        if (wasReady)
            emit status("群房间已断开，正在自动重新加入；未确认消息不会重发。");
    }
}
void GroupChatClientLJY::tick()
{
    if (!active_)
        return;
    if (lan_ && fixedIp_.isEmpty() && now() - lastAnnouncement_ > 1000)
        announce(!host_.running());
    if (!ready_ && socket_.state() != QAbstractSocket::UnconnectedState && now() - connectStarted_ > 5000)
    {
        if (socket_.state() == QAbstractSocket::ConnectedState)
        {
            active_ = false;
            tick_.stop();
            socket_.abort();
            host_.stop();
            emit status("群握手超时，该端口可能被其他程序占用。请检查房间端口后重新选择群。", true);
            return;
        }
        socket_.abort();
        emit status("群连接未完成，正在重新发现房间…");
    }
    if (!ready_ && socket_.state() == QAbstractSocket::UnconnectedState && now() >= nextAttempt_)
        attempt();
}
void GroupChatClientLJY::announce(bool query)
{
    lastAnnouncement_ = now();
    const auto bytes = QJsonDocument(QJsonObject{{"kind", query ? "find" : "room"},
                                                 {"protocol", 1},
                                                 {"room", room_},
                                                 {"token", token_},
                                                 {"port", host_.port()}})
                           .toJson(QJsonDocument::Compact);
    // 在每张可组播网卡发送，避免默认路由恰好指向虚拟网卡。
    for (const auto &adapter : QNetworkInterface::allInterfaces())
        if ((adapter.flags() & QNetworkInterface::IsUp) &&
            (adapter.flags() & QNetworkInterface::CanMulticast))
        {
            discovery_.setMulticastInterface(adapter);
            discovery_.writeDatagram(bytes, discoveryAddress, discoveryPort);
        }
}
void GroupChatClientLJY::discover()
{
    while (discovery_.hasPendingDatagrams())
    {
        auto packet = discovery_.receiveDatagram(4096);
        auto doc = QJsonDocument::fromJson(packet.data());
        if (!doc.isObject())
            continue;
        auto frame = doc.object();
        if (frame.value("protocol").toInt() != 1 || frame.value("room").toString() != room_)
            continue;
        if (frame.value("kind") == "find")
        {
            if (host_.running())
                announce(false);
            continue;
        }
        auto token = frame.value("token").toString();
        int port = frame.value("port").toInt();
        if (frame.value("kind") != "room" || token.isEmpty() || token == token_ || port < 1 || port > 65535)
            continue;
        // 同时开房时选标识较小者，其他候选释放房间并加入它。
        if (host_.running() && token < token_)
        {
            host_.stop();
            socket_.abort();
            ready_ = false;
        }
        if (!host_.running() && !ready_ && socket_.state() == QAbstractSocket::UnconnectedState)
        {
            connectedToken_ = token;
            connectTo(packet.senderAddress().toString(), port);
        }
    }
}
void GroupChatClientLJY::receive()
{
    try
    {
        for (const auto &frame : GroupChatProtocolLJY::feed(buffer_, socket_.readAll()))
        {
            const auto kind = frame.value("kind").toString();
            if (kind == "reject")
            {
                rejected_ = true;
                active_ = false;
                tick_.stop();
                socket_.abort();
                host_.stop();
                emit status(frame.value("reason").toString(), true);
                continue;
            }
            if (kind == "welcome")
            {
                if (frame.value("room").toString() != room_)
                    throw std::runtime_error("连接到了其他群房间。");
                ready_ = true;
                connectedToken_ = frame.value("token").toString();
                emit connectionChanged(true);
                emit status((host_.running() ? "已加入 · 本机主持 · " : "已加入 · ") + endpoint());
            }
            else if (kind == "roster" && ready_)
                emit online(frame.value("accounts").toArray().size());
            else if (kind == "message" && ready_)
            {
                if (frame.value("room").toString() != room_)
                    throw std::runtime_error("收到其他群的消息。");
                ChatProtocolLJY::validateBody(frame.value("body").toString().toUtf8().toStdString());
                emit message(frame);
            }
        }
    }
    catch (const std::exception &e)
    {
        active_ = false;
        tick_.stop();
        socket_.abort();
        host_.stop();
        emit status(QString::fromUtf8(e.what()), true);
    }
}
QString GroupChatClientLJY::sendText(const QString &body)
{
    if (!ready_)
        throw std::runtime_error("正在自动加入群聊，请稍候；文字已保留。");
    ChatProtocolLJY::validateBody(body.toUtf8().toStdString());
    if (socket_.bytesToWrite() > 512 * 1024)
        throw std::runtime_error("网络发送繁忙，请稍后再试。");
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    socket_.write(GroupChatProtocolLJY::encode({{"kind", "text"}, {"request", id}, {"body", body}}));
    return id;
}
