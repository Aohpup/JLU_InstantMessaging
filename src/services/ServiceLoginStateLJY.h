#pragma once

// 区分服务是否开通以及是否已在当前会话中确认登录。
enum class ServiceLoginStateLJY
{
    NotOpened,           // 尚未开通，不能直接登录。
    PendingConfirmation, // 已开通，等待本人确认进入。
    LoggedIn             // 已在当前会话中登录。
};
