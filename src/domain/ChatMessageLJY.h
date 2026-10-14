#pragma once
#include <string>
#include "domain/ServiceTypeLJY.h"

class SessionManagerLJY;

// 由已登录会话核对好友后创建，只把号码副本交给网络线程。
class ChatIdentityLJY
{
public:
    const std::string& ownAccount() const { return ownAccount_; }
    const std::string& peerAccount() const { return peerAccount_; }
    ServiceTypeLJY service() const { return service_; }
private:
    friend ChatIdentityLJY makeChatIdentityLJY(const SessionManagerLJY&, const std::string&);
    ChatIdentityLJY(std::string own, std::string peer, ServiceTypeLJY service)
        : ownAccount_(own), peerAccount_(peer), service_(service) {}
    std::string ownAccount_;
    std::string peerAccount_;
    ServiceTypeLJY service_;
};

// 发送状态只表示本次通信结果；对端接收不等于已读或已保存。
enum class ChatDeliveryLJY { Sending, Received, Failed, Unknown };
const char* chatDeliveryTextLJY(ChatDeliveryLJY state);

// 一条文字消息的值对象；时间使用 Unix 毫秒，服务与对外账号共同标识身份。
struct ChatMessageLJY
{
    std::string id;
    std::string senderAccount;
    std::string receiverAccount;
    std::string body;
    long long time = 0;
    ChatDeliveryLJY delivery = ChatDeliveryLJY::Sending;
    ServiceTypeLJY service = ServiceTypeLJY::QQ;
};

// 当前时间仅供显示和消息编号使用，超时另用单调时钟计算。
long long chatTimeLJY();
