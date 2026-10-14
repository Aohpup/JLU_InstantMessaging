#include "domain/FriendRelationLJY.h"

#include <algorithm>
#include <stdexcept>

FriendRelationLJY::FriendRelationLJY(int firstAccountId, int secondAccountId)
    : firstAccountId_(std::min(firstAccountId, secondAccountId)),
      secondAccountId_(std::max(firstAccountId, secondAccountId))
{
    if (firstAccountId_ <= 0 || firstAccountId_ == secondAccountId_)
        throw std::invalid_argument("好友双方必须是不同的有效账号。");
}

int FriendRelationLJY::firstAccountId() const noexcept { return firstAccountId_; }
int FriendRelationLJY::secondAccountId() const noexcept { return secondAccountId_; }

int FriendRelationLJY::otherAccountId(int ownerAccountId) const
{
    if (ownerAccountId == firstAccountId_) return secondAccountId_;
    if (ownerAccountId == secondAccountId_) return firstAccountId_;
    throw std::invalid_argument("账号不属于这条好友关系。");
}

const std::string& FriendRelationLJY::remarkFor(int ownerAccountId) const
{
    if (ownerAccountId == firstAccountId_) return firstRemark_;
    if (ownerAccountId == secondAccountId_) return secondRemark_;
    throw std::invalid_argument("账号不属于这条好友关系。");
}

void FriendRelationLJY::setRemark(int ownerAccountId, const std::string& remark)
{
    if (ownerAccountId == firstAccountId_) firstRemark_ = remark;
    else if (ownerAccountId == secondAccountId_) secondRemark_ = remark;
    else throw std::invalid_argument("账号不属于这条好友关系。");
}
