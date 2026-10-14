#include "domain/PersonLJY.h"

#include <utility>
#include <stdexcept>

PersonLJY::PersonLJY(int personId, std::string name,
                     std::string birthDate, std::string location, int avatarColor)
    : personId_(personId), avatarColor_(avatarColor == -1 ? (personId > 0 ? (personId - 1) % 6 : 0) : avatarColor), name_(std::move(name)),
      birthDate_(std::move(birthDate)), location_(std::move(location))
{
    if (avatarColor < -1 || avatarColor > 5) throw std::invalid_argument("请选择六种预设头像颜色之一。");
}

int PersonLJY::avatarColor() const noexcept { return avatarColor_; }

int PersonLJY::personId() const noexcept { return personId_; }
const std::string& PersonLJY::name() const noexcept { return name_; }
const std::string& PersonLJY::birthDate() const noexcept { return birthDate_; }
const std::string& PersonLJY::location() const noexcept { return location_; }
