#pragma once
#include "domain/ChatMessageLJY.h"
#include <vector>

enum class ChatFrameTypeLJY { Hello, Text, Ack };

// 第 2 版帧包含版本、类型、服务、连接标识、消息编号、双方号码、时间和原样正文。
struct ChatFrameLJY
{
    ChatFrameTypeLJY type = ChatFrameTypeLJY::Hello;
    std::string connection;
    ChatMessageLJY message;
};

// 只负责字节格式，不操作 Socket 或账号。一个连接配一个接收缓冲区。
class ChatProtocolLJY
{
public:
    static constexpr int maxBody = 16 * 1024;
    static constexpr int maxFrame = 64 * 1024;
    // 编码为四字节大端长度和 UTF-8 内容；长度不含自身四字节。
    static std::string encode(const ChatFrameLJY& frame);
    // 追加一段字节，取出其中所有完整帧，半帧继续保留。
    std::vector<ChatFrameLJY> feed(const std::string& bytes);
    // 断线时用来识别尚未接收完整的帧。
    bool hasPartialFrame() const { return !buffer_.empty(); }
    // 发出前检查正文：不为空白、有效 UTF-8、至多 16 KiB。
    static void validateBody(const std::string& body);
private:
    std::string buffer_;
    static ChatFrameLJY decode(const std::string& payload);
};
