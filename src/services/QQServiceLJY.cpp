#include "services/QQServiceLJY.h"

ServiceTypeLJY QQServiceLJY::type() const noexcept { return ServiceTypeLJY::QQ; }
std::string_view QQServiceLJY::name() const noexcept { return "QQ"; }
AccountIdModeLJY QQServiceLJY::accountIdMode() const noexcept
{
    return AccountIdModeLJY::SharedQQWeibo;
}
GroupStyleLJY QQServiceLJY::defaultGroupStyle() const noexcept
{
    return GroupStyleLJY::QQ;
}
