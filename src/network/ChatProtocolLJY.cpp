#include "network/ChatProtocolLJY.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdexcept>

namespace
{
void checkMetadata(const std::string& value)
{
    if (value.empty() || value.size() > 128 ||
        value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") != std::string::npos)
        throw std::invalid_argument("消息元数据格式错误。");
}
void checkNumber(const std::string& value, ServiceTypeLJY service)
{
    if (service == ServiceTypeLJY::WeChat) { checkMetadata(value); return; }
    if (value.empty() || value.size() > 20 || value[0] == '0' ||
        value.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("消息中的账号号码无效。");
}
const char* typeText(ChatFrameTypeLJY type)
{
    switch (type)
    {
    case ChatFrameTypeLJY::Hello: return "HELLO";
    case ChatFrameTypeLJY::Text: return "TEXT";
    case ChatFrameTypeLJY::Ack: return "ACK";
    }
    throw std::invalid_argument("未知消息类型。");
}
}

void ChatProtocolLJY::validateBody(const std::string& body)
{
    if (body.empty() || body.size() > maxBody || body.find_first_not_of(" \r\n\t") == std::string::npos)
        throw std::invalid_argument("消息不能为空白，且最多为 16 KiB UTF-8 字节。");
    if (body.find('\0') != std::string::npos ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, body.data(), static_cast<int>(body.size()), nullptr, 0) == 0)
        throw std::invalid_argument("消息正文不是有效的 UTF-8 文字。");
}

std::string ChatProtocolLJY::encode(const ChatFrameLJY& frame)
{
    checkMetadata(frame.connection);
    checkMetadata(frame.message.id);
    checkNumber(frame.message.senderAccount, frame.message.service);
    checkNumber(frame.message.receiverAccount, frame.message.service);
    const int service = static_cast<int>(frame.message.service);
    if (service < 0 || service > 2) throw std::invalid_argument("未知聊天服务。");
    if (frame.message.time < 0) throw std::invalid_argument("消息时间无效。");
    if (frame.type == ChatFrameTypeLJY::Text) validateBody(frame.message.body);
    else if (!frame.message.body.empty()) throw std::invalid_argument("控制消息不能携带正文。");
    const auto& m = frame.message;
    const std::string payload = std::string("2\n") + typeText(frame.type) + "\n" + std::to_string(service) + "\n" + frame.connection +
        "\n" + m.id + "\n" + m.senderAccount + "\n" + m.receiverAccount + "\n" + std::to_string(m.time) + "\n" + m.body;
    const unsigned int length = static_cast<unsigned int>(payload.size());
    if (length > maxFrame) throw std::invalid_argument("消息帧过长。");
    std::string output;
    for (int shift : {24, 16, 8, 0}) output.push_back(static_cast<char>((length >> shift) & 255));
    /*
        output.push_back(static_cast<char>((length >> 24) & 255)); // 第1字节
        output.push_back(static_cast<char>((length >> 16) & 255)); // 第2字节
        output.push_back(static_cast<char>((length >>  8) & 255)); // 第3字节
        output.push_back(static_cast<char>( length        & 255)); // 第4字节
    */
    return output + payload;
}

ChatFrameLJY ChatProtocolLJY::decode(const std::string& payload)
{
    std::string fields[8];
    std::size_t begin = 0;
    for (auto& field : fields)
    {
        const auto end = payload.find('\n', begin);
        if (end == std::string::npos) throw std::invalid_argument("消息头不完整。");
        field = payload.substr(begin, end - begin);
        begin = end + 1;
    }
    if (fields[0] != "2") throw std::invalid_argument("聊天协议版本不匹配，请将双方程序更新到支持三服务的 P5 版本。");
    ChatFrameLJY frame;
    if (fields[1] == "HELLO") frame.type = ChatFrameTypeLJY::Hello;
    else if (fields[1] == "TEXT") frame.type = ChatFrameTypeLJY::Text;
    else if (fields[1] == "ACK") frame.type = ChatFrameTypeLJY::Ack;
    else throw std::invalid_argument("未知消息类型。");
    if (fields[2] != "0" && fields[2] != "1" && fields[2] != "2") throw std::invalid_argument("未知聊天服务。");
    frame.message.service = static_cast<ServiceTypeLJY>(std::stoi(fields[2]));
    frame.connection = fields[3];
    frame.message.id = fields[4];
    frame.message.senderAccount = fields[5];
    frame.message.receiverAccount = fields[6];
    if (fields[7].empty() || fields[7].size() > 19 || fields[7].find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("消息时间无效。");
    frame.message.time = std::stoll(fields[7]);
    frame.message.body = payload.substr(begin);
    // 编码与解码共用必要的字段规则，避免两套协议要求不一致。
    encode(frame);
    return frame;
}

std::vector<ChatFrameLJY> ChatProtocolLJY::feed(const std::string& bytes)
{
    buffer_ += bytes;
    std::vector<ChatFrameLJY> frames;
    std::size_t used = 0;
    while (buffer_.size() - used >= 4)
    {
        unsigned int length = 0;
        for (int i = 0; i < 4; ++i)
            length = (length << 8) | static_cast<unsigned char>(buffer_[used + i]);
        if (length == 0 || length > maxFrame) throw std::invalid_argument("消息帧长度无效。");
        if (buffer_.size() - used - 4 < length) break;
        frames.push_back(decode(buffer_.substr(used + 4, length)));
        used += 4 + length;
    }
    buffer_.erase(0, used);
    return frames;
}
