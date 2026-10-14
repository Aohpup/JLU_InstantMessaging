#pragma once

#include "services/PlatformLJY.h"
#include <ostream>

// 仅在空平台加载 A—F、所需账号、好友和 18 个群；重复或非空调用明确拒绝。
void loadDemoDataLJY(PlatformLJY& platform);
// 显示虚构演示账号、密码和各服务号码，号码从平台读取，不硬编码发号顺序。
void showDemoAccountsLJY(const PlatformLJY& platform, std::ostream& output);
