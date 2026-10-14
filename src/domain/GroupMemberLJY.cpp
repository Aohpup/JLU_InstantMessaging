#include "domain/GroupMemberLJY.h"
#include <stdexcept>

GroupMemberLJY::GroupMemberLJY(int accountId, bool administrator)
    : accountId_(accountId), administrator_(administrator)
{
    if (accountId <= 0) throw std::invalid_argument("群成员账号编号无效。");
}
int GroupMemberLJY::accountId() const noexcept { return accountId_; }
bool GroupMemberLJY::isAdministrator() const noexcept { return administrator_; }
