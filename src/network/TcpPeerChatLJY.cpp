#include "network/TcpPeerLJY.h"
#include "network/ChatProtocolLJY.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <stdexcept>

namespace
{
using Clock = std::chrono::steady_clock;

void nonBlocking(SOCKET socket)
{
    u_long enabled = 1;
    if (ioctlsocket(socket, FIONBIO, &enabled) == SOCKET_ERROR)
        throw std::runtime_error("设置非阻塞套接字失败。");
}

// 最多等待 50 毫秒，结果位分别表示可读、可写和连接异常。
int waitSocket(SOCKET socket, bool read, bool write)
{
    fd_set reads, writes, errors;
    FD_ZERO(&reads); FD_ZERO(&writes); FD_ZERO(&errors);
    if (read) FD_SET(socket, &reads);
    if (write) FD_SET(socket, &writes);
    FD_SET(socket, &errors);
    timeval timeout{0, 50000};
    if (select(0, read ? &reads : nullptr, write ? &writes : nullptr, &errors, &timeout) == SOCKET_ERROR)
        throw std::runtime_error("等待网络事件失败（Winsock " + std::to_string(WSAGetLastError()) + "）。");
    return (FD_ISSET(socket, &reads) ? 1 : 0) | (FD_ISSET(socket, &writes) ? 2 : 0) |
           (FD_ISSET(socket, &errors) ? 4 : 0);
}

struct ChatWriteLJY
{
    std::string bytes;
    std::string messageId;
    std::size_t offset = 0;
};
struct ChatPendingLJY
{
    ChatMessageLJY message;
    Clock::time_point queuedAt = Clock::now();
    bool started = false;
    bool fullySent = false;
};
}

std::string TcpPeerLJY::startChat(bool listening, const std::string& ipv4, int port, ChatIdentityLJY identity)
{
    makeAddress(ipv4, port, listening);
    if (listener_ != INVALID_SOCKET || connection_ != INVALID_SOCKET)
        throw std::logic_error("请先关闭第一批的阻塞连接。");
    stopChat();
    static std::atomic<unsigned long long> serial{0};
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    const unsigned long long ticks = (static_cast<unsigned long long>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        // 时间、进程号和进程内序号用于避免重连时重复编号，不承担身份认证。
        chatId_ = std::to_string(ticks) + "-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++serial);
        ownAccount_ = identity.ownAccount();
        service_ = identity.service();
        peerAccount_ = identity.peerAccount();
        nextMessage_ = 0;
        ready_ = false;
        commands_.clear();
        events_.clear();
    }
    stopRequested_ = false;
    worker_ = std::thread(&TcpPeerLJY::runChat, this, listening, ipv4, port, identity);
    return chatId_;
}

std::string TcpPeerLJY::sendMessage(const std::string& body)
{
    ChatProtocolLJY::validateBody(body);
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!ready_ || stopRequested_) throw std::logic_error("尚未与好友完成握手，不能发送消息。");
    ChatMessageLJY message;
    message.id = chatId_ + "-" + ownAccount_ + "-" + std::to_string(++nextMessage_);
    message.senderAccount = ownAccount_; message.receiverAccount = peerAccount_;
    message.body = body; message.time = chatTimeLJY(); message.service = service_;
    commands_.push_back(message);
    events_.push_back({ChatEventTypeLJY::Delivery, chatId_, "发送中", 0, message});
    return message.id;
}

std::vector<ChatEventLJY> TcpPeerLJY::takeEvents()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::vector<ChatEventLJY> result;
    result.swap(events_);
    return result;
}

void TcpPeerLJY::stopChat()
{
    stopRequested_ = true;
    if (worker_.joinable()) worker_.join();
}

void TcpPeerLJY::postEvent(ChatEventTypeLJY type, const std::string& detail,
                          const ChatMessageLJY& message, int port)
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    events_.push_back({type, chatId_, detail, port, message});
}

void TcpPeerLJY::runChat(bool listening, std::string ipv4, int port, ChatIdentityLJY identity)
{
    std::map<std::string, ChatPendingLJY> pending;
    std::string ending = "连接已停止。";
    try
    {
        // 此局部对象的套接字只被当前工作线程创建、使用和释放。
        TcpPeerLJY transport;
        if (listening)
        {
            const int actualPort = transport.startListening(ipv4, port);
            nonBlocking(transport.listener_);
            postEvent(ChatEventTypeLJY::Listening, "正在等待好友连接。", {}, actualPort);
            while (!stopRequested_)
            {
                if (!(waitSocket(transport.listener_, true, false) & 1)) continue;
                transport.connection_ = accept(transport.listener_, nullptr, nullptr);
                if (transport.connection_ != INVALID_SOCKET) break;
                if (WSAGetLastError() != WSAEWOULDBLOCK) throwSocketError("接受连接", WSAGetLastError());
            }
            closesocket(transport.listener_);
            transport.listener_ = INVALID_SOCKET;
        }
        else
        {
            postEvent(ChatEventTypeLJY::Connecting, "正在连接好友。", {}, port);
            transport.connection_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (transport.connection_ == INVALID_SOCKET) throwSocketError("创建连接", WSAGetLastError());
            nonBlocking(transport.connection_);
            const auto address = makeAddress(ipv4, port, false);
            if (connect(transport.connection_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != WSAEWOULDBLOCK) throwSocketError("连接对端", WSAGetLastError());
                const auto deadline = Clock::now() + std::chrono::seconds(10);
                while (!stopRequested_)
                {
                    if (Clock::now() >= deadline) throw std::runtime_error("连接超时，请核对 IP、端口和对方监听状态。");
                    const int state = waitSocket(transport.connection_, false, true);
                    if (!(state & 6)) continue;
                    int error = 0, length = sizeof(error);
                    if (getsockopt(transport.connection_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == SOCKET_ERROR)
                        throwSocketError("读取连接结果", WSAGetLastError());
                    if (error != 0) throwSocketError("连接对端", error);
                    if (state & 4) throw std::runtime_error("连接对端失败。");
                    break;
                }
            }
        }
        if (!stopRequested_)
        {
            nonBlocking(transport.connection_);
            postEvent(ChatEventTypeLJY::Handshaking, "TCP 已连接，正在核对好友身份。");
            ChatProtocolLJY parser;
            std::deque<ChatWriteLJY> writes;
            auto control = [&](ChatFrameTypeLJY type, const std::string& id)
            {
                ChatFrameLJY frame;
                frame.type = type; frame.connection = chatId_;
                frame.message = {id, identity.ownAccount(), identity.peerAccount(), "", chatTimeLJY(), ChatDeliveryLJY::Sending};
                frame.message.service = identity.service();
                writes.push_back({ChatProtocolLJY::encode(frame), "", 0});
            };
            control(ChatFrameTypeLJY::Hello, "-");
            std::string remoteConnection;
            unsigned long long receiveSequence = 0;
            const auto handshakeDeadline = Clock::now() + std::chrono::seconds(5);
            while (!stopRequested_)
            {
                if (remoteConnection.empty() && Clock::now() >= handshakeDeadline)
                    throw std::runtime_error("账号握手超时，对端未使用兼容的聊天协议。");
                std::deque<ChatMessageLJY> commands;
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    commands.swap(commands_);
                }
                for (const auto& message : commands)
                {
                    pending.emplace(message.id, ChatPendingLJY{message, Clock::now(), false, false});
                    writes.push_back({ChatProtocolLJY::encode({ChatFrameTypeLJY::Text, chatId_, message}), message.id, 0});
                }
                for (const auto& entry : pending)
                    if (Clock::now() - entry.second.queuedAt >= std::chrono::seconds(10))
                        throw std::runtime_error("消息发送或接收确认超时，本次连接已结束；不会自动重发。");
                const int state = waitSocket(transport.connection_, true, !writes.empty());
                if (stopRequested_) break;
                if (state & 4) throw std::runtime_error("连接发生网络异常。");
                if ((state & 2) && !writes.empty())
                {
                    auto& item = writes.front();
                    const int length = static_cast<int>(std::min<std::size_t>(4096, item.bytes.size() - item.offset));
                    const int count = send(transport.connection_, item.bytes.data() + item.offset, length, 0);
                    if (count == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() != WSAEWOULDBLOCK) throwSocketError("发送消息", WSAGetLastError());
                    }
                    else
                    {
                        if (count == 0) throw std::runtime_error("连接未能继续发送消息。");
                        item.offset += count;
                        if (!item.messageId.empty())
                        {
                            auto& status = pending.at(item.messageId);
                            status.started = true;
                            status.fullySent = item.offset == item.bytes.size();
                        }
                        if (item.offset == item.bytes.size()) writes.pop_front();
                    }
                }
                if (state & 1)
                {
                    char bytes[4096];
                    const int count = recv(transport.connection_, bytes, sizeof(bytes), 0);
                    if (count == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() != WSAEWOULDBLOCK) throwSocketError("接收消息", WSAGetLastError());
                        continue;
                    }
                    if (count == 0)
                    {
                        if (parser.hasPartialFrame()) throw std::runtime_error("对端断开，尚有消息未接收完整。");
                        ending = "对端已断开连接。";
                        break;
                    }
                    for (const auto& frame : parser.feed(std::string(bytes, count)))
                    {
                        const auto& message = frame.message;
                        if (message.service != identity.service())
                            throw std::runtime_error("聊天服务不匹配，请在双方窗口选择同一服务（QQ、微信或微博）。");
                        if (message.senderAccount != identity.peerAccount() || message.receiverAccount != identity.ownAccount())
                            throw std::runtime_error("账号握手或消息身份不匹配，请核对双方当前服务的账号号码。");
                        if (frame.type == ChatFrameTypeLJY::Hello)
                        {
                            if (!remoteConnection.empty() || message.id != "-") throw std::runtime_error("重复或无效的账号握手。");
                            remoteConnection = frame.connection;
                            {
                                std::lock_guard<std::mutex> lock(queueMutex_);
                                ready_ = true;
                            }
                            postEvent(ChatEventTypeLJY::Ready, "好友身份核对完成，可以发送消息。");
                            continue;
                        }
                        if (remoteConnection.empty() || frame.connection != remoteConnection)
                            throw std::runtime_error("消息不属于本次已握手连接。");
                        if (frame.type == ChatFrameTypeLJY::Text)
                        {
                            const std::string expected = remoteConnection + "-" + identity.peerAccount() + "-" + std::to_string(++receiveSequence);
                            if (message.id != expected) throw std::runtime_error("消息编号不符合本次连接顺序。");
                            auto incoming = message;
                            incoming.delivery = ChatDeliveryLJY::Received;
                            postEvent(ChatEventTypeLJY::Incoming, "收到好友消息。", incoming);
                            control(ChatFrameTypeLJY::Ack, message.id);
                        }
                        else
                        {
                            const auto it = pending.find(message.id);
                            // 无对应待确认消息的 ACK 不改变任何状态，例如已处理过的重复确认。
                            if (it != pending.end() && it->second.fullySent)
                            {
                                auto acknowledged = it->second.message;
                                acknowledged.delivery = ChatDeliveryLJY::Received;
                                postEvent(ChatEventTypeLJY::Delivery, "对端已接收", acknowledged);
                                pending.erase(it);
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& error)
    {
        ending = error.what();
        if (!stopRequested_) postEvent(ChatEventTypeLJY::Error, ending);
    }
    // 先禁止新发送，再收拢主线程尚未提交到 Socket 的消息。
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        ready_ = false;
        for (const auto& message : commands_)
            pending.emplace(message.id, ChatPendingLJY{message, Clock::now(), false, false});
        commands_.clear();
    }
    for (auto& entry : pending)
    {
        auto& item = entry.second;
        item.message.delivery = item.started ? ChatDeliveryLJY::Unknown : ChatDeliveryLJY::Failed;
        postEvent(ChatEventTypeLJY::Delivery, chatDeliveryTextLJY(item.message.delivery), item.message);
    }
    postEvent(ChatEventTypeLJY::Closed, ending);
}
