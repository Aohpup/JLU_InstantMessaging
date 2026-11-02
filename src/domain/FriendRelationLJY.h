#pragma once

#include <string>

class FriendManagerLJY;

// 一条记录表示双方互为好友，两个备注分别属于各自账号。
class FriendRelationLJY
{
public:
    // 账号编号必须为不同的正数，内部按从小到大保存。
    FriendRelationLJY(int firstAccountId, int secondAccountId);
    // 返回关系中较小的账号内部编号。
    int firstAccountId() const noexcept;
    // 返回关系中较大的账号内部编号。
    int secondAccountId() const noexcept;
    // 返回另一方编号；传入非本关系的账号时拒绝。
    int otherAccountId(int ownerAccountId) const;
    // 返回指定一方自己设置的备注；不能借此修改关系。
    const std::string& remarkFor(int ownerAccountId) const;

private:
    // 存储类直接还原已保存状态，不开放额外修改接口。
    friend class SqliteStorageLJY;
    friend class FriendManagerLJY;
    int firstAccountId_;
    int secondAccountId_;
    std::string firstRemark_;
    std::string secondRemark_;
    // 仅由好友业务在检查权限和文本后修改本人的备注。
    void setRemark(int ownerAccountId, const std::string& remark); //双方关系对象.setRemark(账号编号(发出者), 备注文本(给对方的备注))
};
