#include "network/TcpPeerLJY.h"

#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
// Windows 命令行使用宽字符，转换成 UTF-8 后再交给网络层。
std::string utf8(const std::wstring& value)
{
    if (value.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0)
        throw std::runtime_error("命令行文字转换失败。");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

void showUsage()
{
    std::cout << "P5 第一批：两个独立进程各发送一段文字，然后退出。\n"
              << "等待：p5_socket_demo.exe listen 127.0.0.1 27015 [文字]\n"
              << "连接：p5_socket_demo.exe connect 127.0.0.1 27015 [文字]\n"
              << "先运行等待端，再在另一个终端运行连接端。文字含空格时加双引号。\n"
              << "本批尚不支持连续聊天，也不使用账号或数据库。\n";
}

int runDemo(int argc, wchar_t* argv[])
{
    if (argc == 1 || (argc == 2 && std::wstring(argv[1]) == L"--help"))
    {
        showUsage();
        return 0;
    }
    if (argc < 4 || argc > 5)
        throw std::invalid_argument("参数数量不正确，请使用 --help 查看示例。");
    const std::wstring role = argv[1];
    if (role != L"listen" && role != L"connect")
        throw std::invalid_argument("角色只能为 listen 或 connect。");
    const std::wstring portText = argv[3];
    if (portText.empty() || portText.find_first_not_of(L"0123456789") != std::wstring::npos)
        throw std::invalid_argument("端口必须为整数。");
    const int port = std::stoi(portText);
    const std::string message = argc == 5 ? utf8(argv[4]) :
        (role == L"listen" ? "你好，我是等待连接的一端。" : "你好，我是主动连接的一端。");
    // 本批双方先发送再接收，仅演示短文字；连续长消息留待第二批。
    if (message.empty() || message.size() > 4096)
        throw std::invalid_argument("演示文字应为 1—4096 个 UTF-8 字节。");

    TcpPeerLJY peer;
    if (role == L"listen")
    {
        const int actualPort = peer.startListening(utf8(argv[2]), port);
        std::cout << "LISTENING " << actualPort << std::endl;
        std::cout << "正在等待对端连接……" << std::endl;
        peer.acceptConnection();
    }
    else
    {
        peer.connectTo(utf8(argv[2]), port);
    }
    std::cout << "连接成功。" << std::endl;
    peer.sendAll(message);
    peer.finishSending();
    std::cout << "本端已发送：" << message << std::endl;

    // 累积到对端结束发送为止，绝不假设一次 recv 就得到完整文字。
    std::string received;
    while (true)
    {
        const std::string part = peer.receiveSome();
        if (part.empty())
            break;
        received += part;
        if (received.size() > 4096)
            throw std::runtime_error("对端文字超过本批演示的 4096 字节范围。");
    }
    if (received.empty())
        throw std::runtime_error("对端已结束发送，但没有收到文字，本次双向交换未完成。");
    peer.close();
    std::cout << "收到对端：" << received << "\n双向交换完成，连接已关闭。" << std::endl;
    return 0;
}
}

int wmain(int argc, wchar_t* argv[])
{
    const UINT oldCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    int result = 0;
    try
    {
        result = runDemo(argc, argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << "操作失败：" << error.what() << std::endl;
        result = 1;
    }
    if (oldCodePage != 0)
        SetConsoleOutputCP(oldCodePage);
    return result;
}
