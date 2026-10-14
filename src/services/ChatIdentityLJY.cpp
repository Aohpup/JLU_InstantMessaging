#include "services/ChatIdentityLJY.h"
#include "services/SessionManagerLJY.h"
#include <stdexcept>

ChatIdentityLJY makeChatIdentityLJY(const SessionManagerLJY& session, const std::string& peerAccount)
{
    const auto own = session.currentAccount();
    if (own.externalId() == peerAccount)
        throw std::invalid_argument("不能与自己建立聊天连接。");
    const auto peer = session.friendAccount(peerAccount);
    return ChatIdentityLJY(own.externalId(), peer.externalId(), own.serviceType());
}
