#include "domain/FriendRequestLJY.h"
#include <stdexcept>
FriendRequestLJY::FriendRequestLJY(int sender, int recipient, int state)
    : sender_(sender), recipient_(recipient), state_(state)
{
    if (sender <= 0 || recipient <= 0 || sender == recipient || state < 0 || state > 2)
        throw std::invalid_argument("好友申请数据无效。");
}
int FriendRequestLJY::senderId() const noexcept { return sender_; }
int FriendRequestLJY::recipientId() const noexcept { return recipient_; }
int FriendRequestLJY::state() const noexcept { return state_; }
