#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <string>
#include "domain/ChatMessageLJY.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// 每次连接的事件带独立标识；界面只处理当前连接的事件。
enum class ChatEventTypeLJY { Listening, Connecting, Handshaking, Ready, Incoming, Delivery, Error, Closed };
struct ChatEventLJY
{
    ChatEventTypeLJY type;
    std::string connection;
    std::string detail;
    int port = 0;
    ChatMessageLJY message;
};

// 管理一个 TCP 对端；连续聊天用后台接口，第一批阻塞接口保留供学习演示。
class TcpPeerLJY
{
public:
    // 初始化 Windows 网络环境；失败时抛出含原因的异常。
    TcpPeerLJY();
    // 关闭本对象持有的套接字，并配对清理网络环境。
    ~TcpPeerLJY();
    TcpPeerLJY(const TcpPeerLJY&) = delete;
    TcpPeerLJY& operator=(const TcpPeerLJY&) = delete;

    // 以下后台接口由同一个主线程调用。身份须先由会话核对；返回本次连接标识。
    // 等待连接可一直等待；主动连接 10 秒、握手 5 秒、发送或 ACK 等待 10 秒超时。
    std::string startChat(bool listening, const std::string& ipv4, int port, ChatIdentityLJY identity);
    // 握手成功后提交文字，返回编号；事件随后更新发送中、接收、失败或未知状态。
    std::string sendMessage(const std::string& body);
    // 一次取走全部事件，由调用者在主线程处理；不在网络线程调用控件或数据库。
    std::vector<ChatEventLJY> takeEvents();
    // 设置停止标志并回收工作线程；网络等待每 50 毫秒检查停止，不使用强制终止。
    void stopChat();

    // 以下是第一批阻塞接口，不与同一对象的后台聊天混用。
    // 绑定 IPv4 地址并开始监听，返回实际端口；端口 0 供测试自动分配。
    int startListening(const std::string& ipv4, int port);
    // 等待一个对端；连接成功后关闭监听口，只保留本次连接。
    void acceptConnection();
    // 主动连接对方的 IPv4 地址和端口。
    void connectTo(const std::string& ipv4, int port);

    // 循环发送全部字节；返回只代表本地发送成功，不代表对端应用已确认。
    void sendAll(const std::string& text);
    // 接收一段字节；空串表示对端正常结束发送，不保证一次收到完整文字。
    std::string receiveSome();
    // 只结束本端发送，仍能接收对端数据；第一批以此表示一段文字结束。
    void finishSending();
    // 停止后台聊天并释放阻塞接口资源，可重复调用；第一批阻塞等待不支持跨线程取消。
    void close() noexcept;

private:
    static sockaddr_in makeAddress(const std::string& ipv4, int port, bool allowZero);
    [[noreturn]] static void throwSocketError(const std::string& action, int code);
    // 工作线程内部持有独立的 TcpPeerLJY 传输对象，外部只通过队列交换值。
    void runChat(bool listening, std::string ipv4, int port, ChatIdentityLJY identity);
    void postEvent(ChatEventTypeLJY type, const std::string& detail = "",
                   const ChatMessageLJY& message = {}, int port = 0);
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::mutex queueMutex_;
    std::deque<ChatMessageLJY> commands_;
    std::vector<ChatEventLJY> events_;
    bool ready_ = false;
    std::string chatId_, ownAccount_, peerAccount_;
    ServiceTypeLJY service_ = ServiceTypeLJY::QQ;
    unsigned long long nextMessage_ = 0;
    // 监听口负责接入，连接口负责双向收发，两者不能混用。
    SOCKET listener_ = INVALID_SOCKET;
    SOCKET connection_ = INVALID_SOCKET;
};
