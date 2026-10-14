#pragma once

// 表示群业务操作；与菜单数字分开，输入检查和实际提交共用同一套规则。
enum class GroupOperationLJY
{
    Apply, Invite, Leave, Kick, SetAdministrator, TransferOwner, CreateSubgroup, SwitchMode
};
