#pragma once

#include "domain/ServiceAccountLJY.h"

#include <vector>

// 保存跨服务查询结果及未入选原因，菜单只读取结果，不自行判断映射规则。
class FriendQueryResultLJY
{
public:
    // 接收目标服务账号副本和说明；查询本身不改变任何好友关系。
    FriendQueryResultLJY(const std::vector<ServiceAccountLJY>& accounts,
                        const std::vector<std::string>& skippedReasons);
    // 返回已按人员身份去重的目标账号，引用随结果对象销毁而失效。
    const std::vector<ServiceAccountLJY>& accounts() const noexcept;
    // 返回包含源账号号码和未入选原因的说明，不包含密码或个人备注。
    const std::vector<std::string>& skippedReasons() const noexcept;

private:
    std::vector<ServiceAccountLJY> accounts_;
    std::vector<std::string> skippedReasons_;
};
