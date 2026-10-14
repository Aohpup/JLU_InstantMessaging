#pragma once

#include <string>

// 日期只保留年月日，统一使用 YYYY-MM-DD；编号和年月日均使用 int。
class DateLJY
{
public:
    // 解析并校验日期；格式错误或日期不存在时抛出 std::invalid_argument。
    explicit DateLJY(const std::string& text);
    // 读取本机当前日期；读取失败时抛出 std::runtime_error。
    static DateLJY today();
    // 按 YYYY-MM-DD 格式输出日期，月和日不足两位时补零。
    std::string toString() const;
    // 按年月日判断当前日期是否早于另一个日期。
    bool operator<(const DateLJY& other) const noexcept;
    // 计算完整周年。2 月 29 日在非闰年到 3 月 1 日才计满一周年。
    // 结束日期早于当前日期时抛出 std::invalid_argument。
    int fullYearsUntil(const DateLJY& end) const;

private:
    int year_;
    int month_;
    int day_;
};
