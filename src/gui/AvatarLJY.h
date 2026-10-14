#pragma once
#include <QLabel>

// 六组预设色供头像和选择器共用；人员对象保存索引，不允许自定义颜色。
inline const char* avatarBackgroundLJY(int color)
{
    const char* values[] = {"#d2e8d9", "#dce8f5", "#f4dfda", "#e7def2", "#f4e8c9", "#d4e9e9"};
    return values[color];
}
inline const char* avatarForegroundLJY(int color)
{
    const char* values[] = {"#226247", "#355d86", "#934c45", "#69518a", "#80642b", "#336e70"};
    return values[color];
}
// 默认按人员编号循环；传入已保存或预览的索引时使用所选颜色。
inline void applyAvatarColorLJY(QLabel* avatar, int personId, int color = -1)
{
    const int index = color >= 0 && color < 6 ? color : (personId > 0 ? (personId - 1) % 6 : 0);
    avatar->setStyleSheet(QString("background: %1; color: %2;").arg(avatarBackgroundLJY(index), avatarForegroundLJY(index)));
}
