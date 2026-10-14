#include "domain/PasswordCredentialLJY.h"

#include <stdexcept>

PasswordCredentialLJY::PasswordCredentialLJY(const std::string& password)
{
    validatePassword(password);
    password_ = password;
}

void PasswordCredentialLJY::validatePassword(const std::string& password)
{
    if (password.size() < 8 || password.size() > 20)
        throw std::invalid_argument("密码长度须为 8 至 20 字节。");
    for (unsigned char ch : password)
        if (ch < 33 || ch > 126)
            throw std::invalid_argument("密码仅允许大小写字母、数字和符号，不能含空格、中文或控制字符。");
}

bool PasswordCredentialLJY::matches(const std::string& password) const
{
    return password_ == password;
}
