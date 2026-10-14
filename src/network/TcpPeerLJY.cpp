#include "network/TcpPeerLJY.h"

#include <ws2tcpip.h>
#include <algorithm>
#include <stdexcept>

// 保留系统错误码，常见连接问题补充简短中文原因。
[[noreturn]] void TcpPeerLJY::throwSocketError(const std::string& action, int code)
{
    std::string reason;
    switch (code)
    {
    case WSAEADDRINUSE: reason = "端口已被占用"; break;
    case WSAEACCES: reason = "地址或端口不可使用，可能已被其他程序独占"; break;
    case WSAECONNREFUSED: reason = "连接被拒绝，请先启动对方的监听程序并核对端口"; break;
    case WSAECONNRESET: reason = "对端异常断开了连接"; break;
    case WSAETIMEDOUT: reason = "网络等待超时"; break;
    default: reason = "网络操作未完成"; break;
    }
    throw std::runtime_error(action + "失败：" + reason + "（Winsock " + std::to_string(code) + "）");
}

sockaddr_in TcpPeerLJY::makeAddress(const std::string& ipv4, int port, bool allowZero)
{
    if (port < (allowZero ? 0 : 1) || port > 65535)
        throw std::invalid_argument("端口应为 1—65535；仅监听测试允许使用 0。");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (inet_pton(AF_INET, ipv4.c_str(), &address.sin_addr) != 1)
        throw std::invalid_argument("IP 应为 IPv4 地址，例如 127.0.0.1。");
    return address;
}

TcpPeerLJY::TcpPeerLJY()
{
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
        throwSocketError("初始化网络", result);
}

TcpPeerLJY::~TcpPeerLJY()
{
    close();
    WSACleanup();
}

int TcpPeerLJY::startListening(const std::string& ipv4, int port)
{
    if (worker_.joinable() || listener_ != INVALID_SOCKET || connection_ != INVALID_SOCKET)
        throw std::logic_error("请先关闭现有连接或监听。");
    const auto address = makeAddress(ipv4, port, true);
    listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener_ == INVALID_SOCKET)
        throwSocketError("创建监听套接字", WSAGetLastError());
    try
    {
        // 明确占用该端口，避免两份演示程序意外监听同一个地址。
        const BOOL exclusive = TRUE;
        if (setsockopt(listener_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
            throwSocketError("设置监听端口", WSAGetLastError());
        if (bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            throwSocketError("绑定地址", WSAGetLastError());
        if (listen(listener_, 1) == SOCKET_ERROR)
            throwSocketError("开始监听", WSAGetLastError());
        sockaddr_in actual{};
        int length = sizeof(actual);
        if (getsockname(listener_, reinterpret_cast<sockaddr*>(&actual), &length) == SOCKET_ERROR)
            throwSocketError("读取监听端口", WSAGetLastError());
        return ntohs(actual.sin_port);
    }
    catch (...)
    {
        close();
        throw;
    }
}

void TcpPeerLJY::acceptConnection()
{
    if (listener_ == INVALID_SOCKET)
        throw std::logic_error("请先开始监听。");
    connection_ = accept(listener_, nullptr, nullptr);
    if (connection_ == INVALID_SOCKET)
    {
        const int code = WSAGetLastError();
        close();
        throwSocketError("接受连接", code);
    }
    closesocket(listener_);
    listener_ = INVALID_SOCKET;
}

void TcpPeerLJY::connectTo(const std::string& ipv4, int port)
{
    if (worker_.joinable() || listener_ != INVALID_SOCKET || connection_ != INVALID_SOCKET)
        throw std::logic_error("请先关闭现有连接或监听。");
    const auto address = makeAddress(ipv4, port, false);
    connection_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection_ == INVALID_SOCKET)
        throwSocketError("创建连接套接字", WSAGetLastError());
    if (connect(connection_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        const int code = WSAGetLastError();
        close();
        throwSocketError("连接对端", code);
    }
}

void TcpPeerLJY::sendAll(const std::string& text)
{
    std::size_t offset = 0;
    while (offset < text.size())
    {
        const int length = static_cast<int>(std::min<std::size_t>(text.size() - offset, 4096));
        const int count = send(connection_, text.data() + offset, length, 0);
        if (count == SOCKET_ERROR)
            throwSocketError("发送", WSAGetLastError());
        if (count == 0)
            throw std::runtime_error("发送失败：连接未能继续发送数据。");
        offset += count;
    }
}

std::string TcpPeerLJY::receiveSome()
{
    char buffer[4096];
    const int count = recv(connection_, buffer, sizeof(buffer), 0);
    if (count == SOCKET_ERROR)
        throwSocketError("接收", WSAGetLastError());
    // 按实际字节数构造字符串，不能把接收缓冲区当作以零结尾的字符串。
    return std::string(buffer, count);
}

void TcpPeerLJY::finishSending()
{
    if (shutdown(connection_, SD_SEND) == SOCKET_ERROR)
        throwSocketError("结束发送", WSAGetLastError());
}

void TcpPeerLJY::close() noexcept
{
    stopChat();
    if (connection_ != INVALID_SOCKET)
    {
        closesocket(connection_);
        connection_ = INVALID_SOCKET;
    }
    if (listener_ != INVALID_SOCKET)
    {
        closesocket(listener_);
        listener_ = INVALID_SOCKET;
    }
}
