#pragma once

#include <string>

// 课设演示凭据：在内存中保存测试密码，以字符串比较完成登录验证。
class PasswordCredentialLJY
{
public:
    // 检查密码后保存；长度为 8 至 20 字节，只允许 ASCII 可见字符（33 至 126）。
    explicit PasswordCredentialLJY(const std::string& password);

    // 不合格时抛出异常，供注册菜单提示并重输密码。
    static void validatePassword(const std::string& password);

    // 比较输入与保存的密码，不向调用方提供密码内容。
    bool matches(const std::string& password) const;

private:
    // 仅存储类可保存课堂账号凭据，不增加对外读取密码的接口。
    friend class SqliteStorageLJY;
    std::string password_;
};
