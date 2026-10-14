#pragma once

#include "services/SocialServiceLJY.h"

// 微信服务：独立分配号码，默认使用微信群风格。
class WeChatServiceLJY final : public SocialServiceLJY
{
public:
    // 返回微信服务的类型标识。
    ServiceTypeLJY type() const noexcept override;

    // 返回微信服务的显示名称。
    std::string_view name() const noexcept override;

    // 返回独立号码规则，由微信服务单独分配对外号码。
    AccountIdModeLJY accountIdMode() const noexcept override;

    // 返回微信群风格标识。
    GroupStyleLJY defaultGroupStyle() const noexcept override;
};
