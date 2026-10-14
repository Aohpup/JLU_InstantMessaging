#pragma once

#include "domain/ServiceTypeLJY.h"
#include "services/AccountIdModeLJY.h"
#include "services/GroupStyleLJY.h"

#include <string_view>

// 抽象接口：调用方只依赖共同能力，具体服务通过虚函数提供不同信息。
class SocialServiceLJY
{
public:
    // 支持通过基类指针安全释放派生类对象。
    virtual ~SocialServiceLJY();

    // 返回服务类型，用于区分账号和查找对应服务。
    virtual ServiceTypeLJY type() const noexcept = 0;

    // 返回服务名称；派生类应保证名称文本在使用期间有效。
    virtual std::string_view name() const noexcept = 0;

    // 返回号码分配规则，供账号业务决定共享号码或独立发号。
    virtual AccountIdModeLJY accountIdMode() const noexcept = 0;

    // 返回新群采用的默认管理模式，由群业务选择对应策略。
    virtual GroupStyleLJY defaultGroupStyle() const noexcept = 0;
};
