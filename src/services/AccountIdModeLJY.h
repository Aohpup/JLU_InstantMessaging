#pragma once

// 描述服务的号码规则；AccountServiceLJY 根据该规则分配或复用号码。
enum class AccountIdModeLJY
{
    SharedQQWeibo, // 本人的 QQ 和微博复用同一对外号码。
    Independent // 使用该服务独立分配的对外号码。
};
