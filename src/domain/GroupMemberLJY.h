#pragma once

// 群内身份只保存服务账号编号和管理员标记，群主由群对象单独记录。
class GroupMemberLJY
{
public:
    // 账号编号须为正数；管理员标记是否允许由群业务按模式检查。
    explicit GroupMemberLJY(int accountId, bool administrator = false);
    // 返回成员的内部服务账号编号。
    int accountId() const noexcept;
    // 返回保存的管理员标记；是否有管理权限由当前策略决定。
    bool isAdministrator() const noexcept;

private:
    friend class GroupManagerLJY;
    int accountId_;
    bool administrator_;
};
