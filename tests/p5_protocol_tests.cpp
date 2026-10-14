#include "network/ChatProtocolLJY.h"
#include "network/TcpPeerLJY.h"
#include "services/ChatIdentityLJY.h"
#include "services/DemoDataLJY.h"
#include "services/SessionManagerLJY.h"
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

void require(bool value, const char* explanation)
{
    if (!value) throw std::runtime_error(explanation);
}
template<class Action> void rejects(Action action)
{
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Expected rejection");
}

int main()
{
    try
    {
        const ChatFrameLJY first{ChatFrameTypeLJY::Text, "test-1", {"test-1-10001-1", "10001", "10002", "中文\n第二行🙂", 123, ChatDeliveryLJY::Sending}};
        const auto encoded = ChatProtocolLJY::encode(first);
        ChatProtocolLJY split;
        std::vector<ChatFrameLJY> restored;
        for (char byte : encoded)
        {
            const auto frames = split.feed(std::string(1, byte));
            restored.insert(restored.end(), frames.begin(), frames.end());
        }
        require(restored.size() == 1 && restored[0].message.body == first.message.body && !split.hasPartialFrame(), "Split UTF-8 frame changed");
        ChatProtocolLJY combined;
        const auto all = combined.feed(encoded + encoded + encoded.substr(0, 8));
        require(all.size() == 2 && combined.hasPartialFrame(), "Merged frames or tail lost");
        require(combined.feed(encoded.substr(8)).size() == 1, "Tail did not resume");
        rejects([&] { ChatProtocolLJY parser; parser.feed(std::string(4, '\xff')); });
        rejects([&] { auto bad = encoded; bad[4] = '1'; ChatProtocolLJY parser; parser.feed(bad); });
        rejects([&] { ChatProtocolLJY::validateBody(std::string(16385, 'x')); });
        rejects([&] { ChatProtocolLJY::validateBody(std::string(1, '\xff')); });
        std::cout << "PASS framing: split UTF-8, combined frames, partial tail, invalid length/version/body\n";

        PlatformLJY platform;
        loadDemoDataLJY(platform);
        SessionManagerLJY session(platform);
        rejects([&] { makeChatIdentityLJY(session, "10002"); });
        session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
        rejects([&] { makeChatIdentityLJY(session, "10001"); });
        rejects([&] { makeChatIdentityLJY(session, "10005"); });
        const auto identity = makeChatIdentityLJY(session, "10002");
        require(identity.ownAccount() == "10001" && identity.peerAccount() == "10002", "Identity mismatch");
        session.enterService(ServiceTypeLJY::WeChat, true);
        const auto wechat = makeChatIdentityLJY(session, "wx_20003");
        require(wechat.service() == ServiceTypeLJY::WeChat, "WeChat identity service lost");
        auto wx = first; wx.message.service = ServiceTypeLJY::WeChat;
        wx.message.senderAccount = "wx_20001"; wx.message.receiverAccount = "wx_20003";
        ChatProtocolLJY wxParser;
        require(wxParser.feed(ChatProtocolLJY::encode(wx))[0].message.service == ServiceTypeLJY::WeChat, "WeChat framing");
        session.enterService(ServiceTypeLJY::Weibo, true);
        require(makeChatIdentityLJY(session, "10005").service() == ServiceTypeLJY::Weibo, "Weibo identity service lost");
        session.enterService(ServiceTypeLJY::QQ, true);
        std::cout << "PASS identity: login, QQ service, friend relation and self exclusion\n";

        TcpPeerLJY peer;
        const auto oldId = peer.startChat(true, "127.0.0.1", 0, identity);
        bool listening = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!listening && std::chrono::steady_clock::now() < deadline)
        {
            for (const auto& event : peer.takeEvents()) listening |= event.type == ChatEventTypeLJY::Listening;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        require(listening, "Listener not ready");
        const auto begin = std::chrono::steady_clock::now();
        peer.stopChat();
        require(std::chrono::steady_clock::now() - begin < std::chrono::seconds(1), "Stop listener blocked");
        const auto newId = peer.startChat(true, "127.0.0.1", 0, identity);
        require(oldId != newId, "Reconnect reused session id");
        peer.stopChat();
        for (const auto& event : peer.takeEvents()) require(event.connection == newId, "Old event leaked into new session");
        std::cout << "PASS lifecycle: waiting cancelled, unique reconnect id, stale event queue cleared\n";

        // 对端保持连接但不读取：排队文字在主动取消后必须有确定的终态。
        peer.startChat(true, "127.0.0.1", 0, identity);
        int port = 0;
        auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!port && std::chrono::steady_clock::now() < limit)
        {
            for (const auto& event : peer.takeEvents())
                if (event.type == ChatEventTypeLJY::Listening) port = event.port;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        require(port != 0, "Queue test listener not ready");
        TcpPeerLJY raw;
        raw.connectTo("127.0.0.1", port);
        raw.sendAll(ChatProtocolLJY::encode({ChatFrameTypeLJY::Hello, "held-peer",
            {"-", "10002", "10001", "", 123, ChatDeliveryLJY::Sending}}));
        bool ready = false;
        limit = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!ready && std::chrono::steady_clock::now() < limit)
        {
            for (const auto& event : peer.takeEvents()) ready |= event.type == ChatEventTypeLJY::Ready;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        require(ready, "Queue test handshake failed");
        for (int i = 0; i < 128; ++i) peer.sendMessage(std::string(16000, 'x'));
        const auto stopBegin = std::chrono::steady_clock::now();
        peer.stopChat();
        require(std::chrono::steady_clock::now() - stopBegin < std::chrono::seconds(1), "Queued send cancellation blocked");
        std::map<std::string, ChatDeliveryLJY> states;
        int failed = 0, unknown = 0;
        for (const auto& event : peer.takeEvents())
            if (event.type == ChatEventTypeLJY::Delivery) states[event.message.id] = event.message.delivery;
        require(states.size() == 128, "Queued messages lost their terminal states");
        for (const auto& item : states)
        {
            failed += item.second == ChatDeliveryLJY::Failed;
            unknown += item.second == ChatDeliveryLJY::Unknown;
        }
        require(failed > 0 && failed + unknown == 128, "Unsent messages not failed or sent messages falsely acknowledged");
        raw.close();
        std::cout << "PASS queued cancellation: failed=" << failed << " unknown=" << unknown << '\n';

    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
