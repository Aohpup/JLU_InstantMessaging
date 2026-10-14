#include "domain/GroupRequestLJY.h"
#include <stdexcept>

GroupRequestLJY::GroupRequestLJY(int id, ServiceTypeLJY service, int groupNumber,
    GroupRequestTypeLJY type, const std::string& sender, const std::string& target)
    : id_(id), service_(service), groupNumber_(groupNumber), type_(type), sender_(sender), target_(target)
{
    if (id <= 0 || groupNumber <= 0 || sender.empty() || target.empty())
        throw std::invalid_argument("申请或邀请记录不完整。");
    if (service != ServiceTypeLJY::QQ && service != ServiceTypeLJY::WeChat && service != ServiceTypeLJY::Weibo)
        throw std::invalid_argument("请求服务类型无效。");
    if (type != GroupRequestTypeLJY::Application && type != GroupRequestTypeLJY::Invitation)
        throw std::invalid_argument("请求种类无效。");
    if ((type == GroupRequestTypeLJY::Application) != (sender == target))
        throw std::invalid_argument("申请须由本人发起，邀请须发给其他账号。");
}
int GroupRequestLJY::id() const noexcept { return id_; }
ServiceTypeLJY GroupRequestLJY::serviceType() const noexcept { return service_; }
int GroupRequestLJY::groupNumber() const noexcept { return groupNumber_; }
GroupRequestTypeLJY GroupRequestLJY::type() const noexcept { return type_; }
const std::string& GroupRequestLJY::senderNumber() const noexcept { return sender_; }
const std::string& GroupRequestLJY::targetNumber() const noexcept { return target_; }
GroupRequestStateLJY GroupRequestLJY::state() const noexcept { return state_; }
