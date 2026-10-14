#include "services/WeChatServiceLJY.h"

ServiceTypeLJY WeChatServiceLJY::type() const noexcept { return ServiceTypeLJY::WeChat; }
GroupStyleLJY WeChatServiceLJY::defaultGroupStyle() const noexcept
{
    return GroupStyleLJY::WeChat;
}

std::string_view WeChatServiceLJY::name() const noexcept { return "WeChat"; }
AccountIdModeLJY WeChatServiceLJY::accountIdMode() const noexcept
{
    return AccountIdModeLJY::Independent;
}
