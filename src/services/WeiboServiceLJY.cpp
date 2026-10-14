#include "services/WeiboServiceLJY.h"

ServiceTypeLJY WeiboServiceLJY::type() const noexcept { return ServiceTypeLJY::Weibo; }
std::string_view WeiboServiceLJY::name() const noexcept { return "Weibo"; }
AccountIdModeLJY WeiboServiceLJY::accountIdMode() const noexcept
{
    return AccountIdModeLJY::SharedQQWeibo;
}
GroupStyleLJY WeiboServiceLJY::defaultGroupStyle() const noexcept
{
    return GroupStyleLJY::QQ;
}
