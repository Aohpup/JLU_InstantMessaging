#pragma once
#include "domain/ServiceTypeLJY.h"
#include "domain/GroupRequestTypeLJY.h"
#include "domain/GroupRequestStateLJY.h"
#include <string>

// 按“服务＋群号/账号号码”关联请求；这些号码在当前平台中不会变化。
class GroupRequestLJY
{
public:
    // 申请的发起者与目标相同，邀请的发起者与目标不同；账号和群有效性由群业务检查。
    GroupRequestLJY(int id, ServiceTypeLJY service, int groupNumber, GroupRequestTypeLJY type,
                    const std::string& sender, const std::string& target);
    // 查询请求编号。
    int id() const noexcept;
    // 查询请求所属服务。
    ServiceTypeLJY serviceType() const noexcept;
    // 查询目标群的可见群号。
    int groupNumber() const noexcept;
    // 查询请求是申请还是邀请。
    GroupRequestTypeLJY type() const noexcept;
    // 查询发起者在当前服务的号码。
    const std::string& senderNumber() const noexcept;
    // 查询待入群账号的号码。
    const std::string& targetNumber() const noexcept;
    // 查询待处理、同意、拒绝或失效状态。
    GroupRequestStateLJY state() const noexcept;
private:
    // 存储类直接还原已保存状态，不开放额外修改接口。
    friend class SqliteStorageLJY;
    friend class GroupManagerLJY;
    int id_;
    ServiceTypeLJY service_;
    int groupNumber_;
    GroupRequestTypeLJY type_;
    std::string sender_;
    std::string target_;
    GroupRequestStateLJY state_ = GroupRequestStateLJY::Pending;
};
