#include "domain/FriendQueryResultLJY.h"

FriendQueryResultLJY::FriendQueryResultLJY(const std::vector<ServiceAccountLJY>& accounts,
    const std::vector<std::string>& skippedReasons)
    : accounts_(accounts), skippedReasons_(skippedReasons) {}

const std::vector<ServiceAccountLJY>& FriendQueryResultLJY::accounts() const noexcept
{
    return accounts_;
}

const std::vector<std::string>& FriendQueryResultLJY::skippedReasons() const noexcept
{
    return skippedReasons_;
}
