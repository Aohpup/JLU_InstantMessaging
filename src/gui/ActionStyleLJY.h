#pragma once
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
#include <initializer_list>

// 同类操作固定等宽等高；动态文字也预留空间，避免编辑状态切换时按钮跳动。
inline void sizeActionGroupLJY(QWidget* root, std::initializer_list<const char*> names, int width, int height = 36)
{
    for (const auto* name : names)
        if (auto* button = root->findChild<QPushButton*>(name))
        {
            button->setFixedSize(width, height);
            button->setProperty("sizedAction", true);
        }
}

// 统一常规表单节奏；头像色点、导航、消息展开和资料卡保留自己的用途尺寸。
inline void applyControlRhythmLJY(QWidget* root)
{
    for (auto* button : root->findChildren<QPushButton*>())
    {
        const auto name = button->objectName();
        if (button->property("navigation").toBool() || name == "sidebarProfileButton" ||
            name.startsWith("avatarColor") || name == "chatExpand" || name == "chatFriendMore" || name == "chatLatest" || name == "groupChatLatest") continue;
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(36);
        button->setMinimumWidth(80);
        button->setProperty("sizedAction", true);
    }
    for (auto* field : root->findChildren<QLineEdit*>()) field->setFixedHeight(36);
    for (auto* field : root->findChildren<QComboBox*>())
    {
        field->setFixedHeight(36);
        field->setMinimumContentsLength(5);
        field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    }
    for (auto* field : root->findChildren<QSpinBox*>()) field->setFixedHeight(36);
    sizeActionGroupLJY(root, {"switchService0", "switchService1", "switchService2"}, 88);
    sizeActionGroupLJY(root, {"logoutButton"}, 88);
    sizeActionGroupLJY(root, {"nicknameButton", "profileButton", "passwordButton",
        "nicknameCancel", "profileCancel", "passwordCancel"}, 120);
    sizeActionGroupLJY(root, {"bindButton", "unbindButton"}, 80, 32);
    sizeActionGroupLJY(root, {"confirmUnbind", "cancelUnbind", "saveRemarkButton", "cancelRemarkButton",
        "acceptFriendRequest", "rejectFriendRequest", "acceptGroupRequest", "rejectGroupRequest",
        "saveSubgroupButton", "cancelSubgroupButton"}, 112);
    sizeActionGroupLJY(root, {"friendMessageButton", "friendRemarkButton", "removeFriendButton",
        "applyGroupButton", "leaveGroupButton", "groupMessageButton", "createSubgroupButton",
        "openSubgroupButton", "parentGroupButton"}, 88);
    sizeActionGroupLJY(root, {"findGroupButton", "refreshGroupsButton", "addFriendButton",
        "openServiceButton", "switchGroupModeButton", "inviteGroupButton",
        "createGroupButton", "confirmCreateGroup", "cancelCreateGroup"}, 112);
    sizeActionGroupLJY(root, {"groupMemberAction"}, 128);
    sizeActionGroupLJY(root, {"chatSettings", "groupChatSettings", "groupChatMembersButton", "chatSend", "groupChatSend",
        "chatResetSettings", "chatDisconnect", "chatCancelSettings", "chatSaveSettings",
        "groupChatCancelSettings", "groupChatSaveSettings", "groupChatCopyIp", "chatCopyIp"}, 96);
    for (const auto* name : {"loginButton", "registerButton"})
        if (auto* button = root->findChild<QPushButton*>(name)) button->setFixedHeight(44);
}
