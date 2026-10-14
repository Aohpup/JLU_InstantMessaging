#include "domain/DateLJY.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

DateLJY::DateLJY(const std::string& text)
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        throw std::invalid_argument("日期须使用 YYYY-MM-DD 格式。");
    for (int i = 0; i < 10; ++i)
    {
        if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9'))
            throw std::invalid_argument("日期中的年月日须使用数字。");
    }
    year_ = std::stoi(text.substr(0, 4));
    month_ = std::stoi(text.substr(5, 2));
    day_ = std::stoi(text.substr(8, 2));
    if (year_ < 1 || month_ < 1 || month_ > 12)
        throw std::invalid_argument("年份或月份无效。");
    int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year_ % 400 == 0 || (year_ % 4 == 0 && year_ % 100 != 0);
    if (leap) monthDays[1] = 29;       //leap：闰年
    if (day_ < 1 || day_ > monthDays[month_ - 1])
        throw std::invalid_argument("该月份不存在此日期。");
}

DateLJY DateLJY::today()
{
    const auto now = std::time(nullptr);
    const std::tm* local = std::localtime(&now);
    if (!local) throw std::runtime_error("无法读取本机日期。");
    char buffer[11];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", local) == 0)
        throw std::runtime_error("无法转换本机日期格式。");
    return DateLJY(buffer);
}

std::string DateLJY::toString() const
{
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << year_ << '-'
        << std::setw(2) << month_ << '-' << std::setw(2) << day_;
    return out.str();
}

bool DateLJY::operator<(const DateLJY& other) const noexcept
{
    if (year_ != other.year_) return year_ < other.year_;
    if (month_ != other.month_) return month_ < other.month_;
    return day_ < other.day_;
}

int DateLJY::fullYearsUntil(const DateLJY& end) const
{
    if (end < *this) throw std::invalid_argument("结束日期不能早于开始日期。");
    int years = end.year_ - year_;
    if (end.month_ < month_ || (end.month_ == month_ && end.day_ < day_)) --years;
    return years;
}
