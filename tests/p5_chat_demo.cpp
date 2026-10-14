#include "network/TcpPeerLJY.h"
#include "services/ChatIdentityLJY.h"
#include "services/DemoDataLJY.h"
#include "services/SessionManagerLJY.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
const char* eventName(ChatEventTypeLJY type)
{
    switch(type)
    {
    case ChatEventTypeLJY::Listening: return "LISTENING";
    case ChatEventTypeLJY::Connecting: return "CONNECTING";
    case ChatEventTypeLJY::Handshaking: return "HANDSHAKING";
    case ChatEventTypeLJY::Ready: return "READY";
    case ChatEventTypeLJY::Incoming: return "INCOMING";
    case ChatEventTypeLJY::Delivery: return "DELIVERY";
    case ChatEventTypeLJY::Error: return "ERROR";
    case ChatEventTypeLJY::Closed: return "CLOSED";
    }
    return "EVENT";
}
}

// 课堂演示使用固定内存账号，不读正式数据库；自动连发三条便于观察 ACK。
int main(int argc, char* argv[])
{
    const UINT oldCP = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    int result = 0;
    try
    {
        if (argc < 6 || argc > 7)
        {
            std::cout << "用法：p5_chat_demo.exe listen/connect IPv4 端口 本人QQ 好友QQ [hold]\n"
                      << "例：listen 127.0.0.1 27015 10001 10002\n"
                      << "例：connect 127.0.0.1 27015 10002 10001\n"
                      << "默认连续互发三条测试文字后结束；hold 供自动验收保持连接。\n";
        }
        else
        {
            const std::string role = argv[1];
            if (role != "listen" && role != "connect") throw std::invalid_argument("角色应为 listen 或 connect。");
            const std::string portText = argv[3];
            if (portText.empty() || portText.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("端口必须为整数。");
            PlatformLJY platform;
            loadDemoDataLJY(platform);
            SessionManagerLJY session(platform);
            session.login(ServiceTypeLJY::QQ, argv[4], "DemoOnly123!");
            const auto identity = makeChatIdentityLJY(session, argv[5]);
            TcpPeerLJY peer;
            const auto connection = peer.startChat(role == "listen", argv[2], std::stoi(portText), identity);
            std::cout << "SESSION " << connection << std::endl;
            const bool hold = argc == 7 && std::string(argv[6]) == "hold";
            int received = 0, acknowledged = 0;
            bool finished = false, sent = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
            auto completedAt = deadline;
            while (!finished && std::chrono::steady_clock::now() < deadline)
            {
                // 自动验收可通过管道输入 stop；不增加一个阻塞 stdin 的线程。
                DWORD available = 0;
                const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
                if (GetFileType(input) == FILE_TYPE_PIPE && PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr) && available)
                {
                    char command[32]; DWORD count = 0;
                    ReadFile(input, command, std::min<DWORD>(available, sizeof(command)), &count, nullptr);
                    if (std::string(command, count).find("stop") != std::string::npos) peer.stopChat();
                }
                for (const auto& event : peer.takeEvents())
                {
                    if (event.connection != connection) continue;
                    std::cout << eventName(event.type) << " " << event.port << " " << event.detail;
                    if (!event.message.id.empty())
                        std::cout << " | " << event.message.id << " | " << event.message.body;
                    std::cout << std::endl;
                    if (event.type == ChatEventTypeLJY::Ready && !sent)
                    {
                        sent = true;
                        peer.sendMessage(identity.ownAccount() + "：第一条中文消息。\n保留第二行。🙂");
                        peer.sendMessage(identity.ownAccount() + "：第二条紧接着发送。");
                        std::string large = identity.ownAccount() + "：第三条较长消息：";
                        for (int i = 0; i < 1000; ++i) large += "中文";
                        peer.sendMessage(large);
                    }
                    if (event.type == ChatEventTypeLJY::Incoming) ++received;
                    if (event.type == ChatEventTypeLJY::Delivery && event.message.delivery == ChatDeliveryLJY::Received) ++acknowledged;
                    if (event.type == ChatEventTypeLJY::Error) result = 1;
                    if (event.type == ChatEventTypeLJY::Closed) finished = true;
                }
                if (!hold && received == 3 && acknowledged == 3)
                {
                    if (completedAt == deadline) completedAt = std::chrono::steady_clock::now();
                    // 给已入队的对端 ACK 留出发送时间，仅为自动演示结束条件。
                    if (std::chrono::steady_clock::now() - completedAt >= std::chrono::milliseconds(250))
                    {
                        peer.stopChat();
                        finished = true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            peer.stopChat();
            for (const auto& event : peer.takeEvents())
                std::cout << eventName(event.type) << " " << event.detail << std::endl;
            if (!hold && (received != 3 || acknowledged != 3)) result = 1;
            if (!finished) { std::cerr << "演示等待超时。\n"; result = 1; }
            std::cout << "SUMMARY received=" << received << " ack=" << acknowledged << std::endl;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "操作失败：" << error.what() << std::endl;
        result = 1;
    }
    if (oldCP) SetConsoleOutputCP(oldCP);
    return result;
}
