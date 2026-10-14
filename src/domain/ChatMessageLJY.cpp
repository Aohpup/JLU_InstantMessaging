#include "domain/ChatMessageLJY.h"
#include <chrono>

const char* chatDeliveryTextLJY(ChatDeliveryLJY state)
{
    switch (state)
    {
    case ChatDeliveryLJY::Sending: return "发送中";
    case ChatDeliveryLJY::Received: return "对端已接收";
    case ChatDeliveryLJY::Failed: return "发送失败";
    case ChatDeliveryLJY::Unknown: return "结果未知";
    }
    return "结果未知";
}
long long chatTimeLJY()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
