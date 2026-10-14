#pragma once

// 每对账号保留最近一次申请；拒绝后可重发，处理状态随平台保存。
class FriendRequestLJY
{
public:
    FriendRequestLJY(int sender, int recipient, int state = 0);
    // 状态：0 待处理，1 已同意，2 已拒绝；只有收件人可处理。
    int senderId() const noexcept;
    int recipientId() const noexcept;
    int state() const noexcept;
private:
    friend class FriendManagerLJY;
    int sender_, recipient_, state_;
};
