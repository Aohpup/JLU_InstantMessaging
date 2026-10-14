#pragma once

#include "services/SocialServiceLJY.h"

// 微博服务：与本人 QQ 共享号码；默认群风格见实现中的方案约定。
class WeiboServiceLJY final : public SocialServiceLJY
{
public:
    // 返回微博服务的类型标识。
    ServiceTypeLJY type() const noexcept override;

    // 返回微博服务的显示名称。
    std::string_view name() const noexcept override;

    // 返回 QQ／微博共享号码规则，复用本人的对外号码。
    AccountIdModeLJY accountIdMode() const noexcept override;

    // 按项目方案约定，返回 QQ 群风格标识。
    GroupStyleLJY defaultGroupStyle() const noexcept override;
};
