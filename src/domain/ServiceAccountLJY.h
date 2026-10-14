#pragma once

#include "domain/ServiceTypeLJY.h"

#include <string>

// 一条已开通服务的账号记录。登录状态以后由 SessionManagerLJY 保存。
class ServiceAccountLJY
{
public:
    // accountId 为内部主键，externalId 为对外号码；二者不能混用。
    // 这里只保存传入数据；平台中的开通、号码分配和归属校验由 AccountServiceLJY 完成。
    ServiceAccountLJY(int accountId, int personId,
                      ServiceTypeLJY serviceType, std::string externalId,
                      std::string nickname, std::string appliedOn);

    // 查询内部账号编号，平台用它唯一定位一条账号记录。
    int accountId() const noexcept;

    // 查询账号所属人员的内部编号。
    int personId() const noexcept;

    // 查询账号所属的服务类型。
    ServiceTypeLJY serviceType() const noexcept;

    // 查询对外号码；定位账号时需要同时指定服务类型。
    const std::string& externalId() const noexcept;

    // 只读查询该服务账号的昵称。
    const std::string& nickname() const noexcept;

    // 只读查询申请日期，格式为 YYYY-MM-DD。
    const std::string& appliedOn() const noexcept;

private:
    int accountId_;
    int personId_;
    ServiceTypeLJY serviceType_;
    std::string externalId_;
    std::string nickname_;
    std::string appliedOn_;
};
