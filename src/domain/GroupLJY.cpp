#include "domain/GroupLJY.h"
#include <stdexcept>
#include <cctype>

GroupLJY::GroupLJY(int groupId, ServiceTypeLJY service, int number, const std::string& name,
    int ownerAccountId, GroupStyleLJY style)
    : groupId_(groupId), service_(service), number_(number), name_(name),
      ownerAccountId_(ownerAccountId), style_(style)
{
    if (groupId <= 0 || number <= 0) throw std::invalid_argument("群编号和群号须为正整数。");
    if (service != ServiceTypeLJY::QQ && service != ServiceTypeLJY::WeChat && service != ServiceTypeLJY::Weibo)
        throw std::invalid_argument("未知的群所属服务。");
    if (style != GroupStyleLJY::QQ && style != GroupStyleLJY::WeChat)
        throw std::invalid_argument("未知的群管理模式。");
    bool hasText = false;
    for (unsigned char ch : name) if (!std::isspace(ch)) hasText = true;
    if (!hasText) throw std::invalid_argument("群名不能为空或全为空白。");
    members_.emplace_back(ownerAccountId);
}
int GroupLJY::groupId() const noexcept { return groupId_; }
int GroupLJY::number() const noexcept { return number_; }
ServiceTypeLJY GroupLJY::serviceType() const noexcept { return service_; }
const std::string& GroupLJY::name() const noexcept { return name_; }
int GroupLJY::ownerAccountId() const noexcept { return ownerAccountId_; }
GroupStyleLJY GroupLJY::style() const noexcept { return style_; }
int GroupLJY::parentGroupId() const noexcept { return parentGroupId_; }
const std::vector<GroupMemberLJY>& GroupLJY::members() const noexcept { return members_; }
const GroupMemberLJY* GroupLJY::findMember(int accountId) const
{
    for (const auto& member : members_) if (member.accountId() == accountId) return &member;
    return nullptr;
}
