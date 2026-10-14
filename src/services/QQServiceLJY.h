#pragma once

#include "services/SocialServiceLJY.h"

// QQ服务：与本人微博共享号码，默认使用 QQ 群风格。
class QQServiceLJY final : public SocialServiceLJY
{
public:
    // 返回QQ服务的类型标识。
    ServiceTypeLJY type() const noexcept override;

    // 返回QQ服务的显示名称。
    std::string_view name() const noexcept override;

    // 返回 QQ／微博共享号码规则，复用本人的对外号码。
    AccountIdModeLJY accountIdMode() const noexcept override;

    // 返回 QQ 群风格标识。
    GroupStyleLJY defaultGroupStyle() const noexcept override;
};
