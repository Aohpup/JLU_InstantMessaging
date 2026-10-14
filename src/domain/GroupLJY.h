#pragma once

#include "domain/GroupMemberLJY.h"
#include "domain/ServiceTypeLJY.h"
#include "services/GroupStyleLJY.h"
#include <string>
#include <vector>

// 群拥有成员数据；管理策略只读这些数据，不另存成员名单。
class GroupLJY
{
public:
    // 新群自动加入群主；平台账号归属及同服务限制由 GroupManager 检查。
    GroupLJY(int groupId, ServiceTypeLJY service, int number, const std::string& name,
             int ownerAccountId, GroupStyleLJY style);
    // 内部编号在平台唯一，对外群号仅在所属服务内唯一。
    int groupId() const noexcept;
    int number() const noexcept;
    // 查询所属服务、群名和群主账号编号。
    ServiceTypeLJY serviceType() const noexcept;
    const std::string& name() const noexcept;
    int ownerAccountId() const noexcept;
    // 当前模式决定管理规则；父群编号 0 表示顶级群，非 0 指向所属父群。
    GroupStyleLJY style() const noexcept;
    int parentGroupId() const noexcept;
    // 只读成员列表；按账号查找时，不在群内返回 nullptr。
    const std::vector<GroupMemberLJY>& members() const noexcept;
    const GroupMemberLJY* findMember(int accountId) const;

private:
    // 存储类直接还原已保存状态，不开放额外修改接口。
    friend class SqliteStorageLJY;
    friend class GroupManagerLJY;
    int groupId_;
    ServiceTypeLJY service_;
    int number_;
    std::string name_;
    int ownerAccountId_;
    GroupStyleLJY style_;
    int parentGroupId_ = 0;
    std::vector<GroupMemberLJY> members_;
};
