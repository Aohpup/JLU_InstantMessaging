#pragma once

#include <string>

// 表示一个人，而不是某项服务账号。账号通过 personId 关联到本人。
class PersonLJY
{
public:
    // 日期使用 YYYY-MM-DD 文本；存入平台前由 AccountServiceLJY 校验。
    PersonLJY(int personId, std::string name,
              std::string birthDate, std::string location, int avatarColor = -1);

    // 查询内部人员编号，用于关联该人员的服务账号。
    int personId() const noexcept;

    // 只读查询姓名，返回成员字符串的引用。
    const std::string& name() const noexcept;

    // 只读查询生日，格式为 YYYY-MM-DD。
    const std::string& birthDate() const noexcept;

    // 只读查询所在地，返回成员字符串的引用。
    const std::string& location() const noexcept;

// 头像预设色为 0—5，构造时 -1 表示按人员编号循环分配。
    int avatarColor() const noexcept;

private:
    int personId_;
    int avatarColor_;
    std::string name_;
    std::string birthDate_;
    std::string location_;
};
