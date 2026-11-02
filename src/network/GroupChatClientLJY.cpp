#include "network/GroupRosterLJY.h"
#include "network/GroupChatClientLJY.h"
#include "network/GroupChatProtocolLJY.h"
#include "network/ChatProtocolLJY.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <stdexcept>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QSaveFile>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace
{
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
                               const GroupChatMembersLJY &members, int port)
{
    leave();
    room_ = room;
    account_ = account;
    members_ = members;
    port_ = port;
    token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    active_ = true;
    rejected_ = false;
    nextLaunch_ = now() + 700;
    historyComplete_ = false;
    durable_ = false;
    if (!serverDirectory_.isEmpty()) writeMembers();
    nextAttempt_ = now() + 100;
    tick_.start();
    emit status("正在自动加入群聊…");
}
void GroupChatClientLJY::leave()
{
    active_ = false;
    tick_.stop();
    ready_ = false;
    socket_.abort();
    host_.stop();
    buffer_.clear();
    syncing_ = false; historyComplete_ = false; liveDuringSync_.clear();
    emit connectionChanged(false);
    emit online(0);
}
void GroupChatClientLJY::updateMembers(const GroupChatMembersLJY &members)
{
    if (active_ && !serverDirectory_.isEmpty() && members_ != members)
        saveGroupRosterLJY(serverDirectory_, room_, members_, members);
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
    if (!serverDirectory_.isEmpty())
    {
        if (now() >= nextLaunch_) { launchServer(); nextLaunch_ = now() + 3000; }
        connectTo("127.0.0.1", port_);
        return;
    }
    // 要求持久历史时只等待本机持久服务，不能退回内存房间。
    if (requirePersistent_)
    {
        connectTo("127.0.0.1", port_);
        return;
    }
    if (host_.running())
    {
        connectTo("127.0.0.1", host_.port());
        return;
    }
    // 先尝试占用默认端口；占用失败说明可能已有本机房间，直接尝试加入。
    token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (host_.start(room_, port_, token_, members_))
        connectTo("127.0.0.1", host_.port());
    else
        connectTo("127.0.0.1", port_);
}
void GroupChatClientLJY::disconnected()
{
    const bool wasReady = ready_;
    ready_ = false;
    syncing_ = false; historyComplete_ = false; liveDuringSync_.clear();
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
        emit status("群连接未完成，正在重新连接本机房间…");
    }
    if (!ready_ && socket_.state() == QAbstractSocket::UnconnectedState && now() >= nextAttempt_)
        attempt();
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
                durable_ = frame.value("history").toBool();
                if (requirePersistent_ && !durable_)
                    throw std::runtime_error("当前端口是旧版临时房间，请关闭旧房间后重试，确认本机持久群服务可用。");
                syncHistory();
                emit connectionChanged(true);
                emit status((durable_ ? "已连接持久群服务 · 正在同步 · " : "已加入临时房间（不支持离线补齐）· ") + endpoint());
            }
            else if (kind == "history_end" && ready_ && syncing_)
            {
                if (frame.value("more").toBool())
                    socket_.write(GroupChatProtocolLJY::encode({{"kind","history"},{"after",frame.value("after")}}));
                else
                {
                    syncing_ = false; historyComplete_ = true;
                    const auto queued = liveDuringSync_; liveDuringSync_.clear();
                    for (const auto &m : queued) emit message(m);
                    emit historySynchronized();
                }
            }
            else if (kind == "roster" && ready_) {
                emit online(frame.value("accounts").toArray().size());
                emit rosterReceived(frame);
            }
            else if (kind == "message" && ready_)
            {
                if (frame.value("room").toString() != room_)
                    throw std::runtime_error("收到其他群的消息。");
                ChatProtocolLJY::validateBody(frame.value("body").toString().toUtf8().toStdString());
                if (syncing_ && !frame.value("history").toBool()) liveDuringSync_.append(frame);
                else emit message(frame);
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

void GroupChatClientLJY::syncHistory()
{
    if (!ready_ || !durable_ || syncing_) return;
    syncing_ = true; historyComplete_ = false;
    socket_.write(GroupChatProtocolLJY::encode({{"kind","history"},{"after","0"}}));
}
void GroupChatClientLJY::writeMembers()
{
    saveGroupRosterLJY(serverDirectory_, room_, {}, members_, true);
}
void GroupChatClientLJY::launchServer()
{
    auto key=room_; key.replace(":","_");
    QProcess server;
    server.setStandardOutputFile(QProcess::nullDevice());
    server.setStandardErrorFile(QProcess::nullDevice());
    server.setProgram(QCoreApplication::applicationDirPath()+"/jlu_group_server.exe");
    server.setArguments({"--room",room_,"--port",QString::number(port_),"--database",serverDirectory_+"/"+key+".db",
        "--members",serverDirectory_+"/"+key+".json","--idle-seconds","60"});
#ifdef Q_OS_WIN
    server.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        // 后台群进程不继承私聊套接字，否则私聊关闭后对端仍以为连接存活。
        a->inheritHandles = false;
        a->flags |= CREATE_NO_WINDOW;
        a->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        a->startupInfo->wShowWindow = SW_HIDE;
    });
#endif
    if(!server.startDetached()) emit status("无法启动群服务，请确认 jlu_group_server.exe 与主程序放在一起。",true);
}
