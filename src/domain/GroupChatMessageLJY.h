#pragma once
#include <QString>
#include <QJsonObject>
// 群聊天值只在可选图形/网络模块使用；rowId 用于本地历史分页。
struct GroupChatMessageLJY
{
    long long rowId = 0, time = 0;
    QString id, sender, nickname, body;
    int color = 0;
    static GroupChatMessageLJY fromFrame(const QJsonObject &frame)
    {
        GroupChatMessageLJY message;
        message.id = frame.value("id").toString();
        message.sender = frame.value("sender").toString();
        message.nickname = frame.value("nickname").toString();
        message.body = frame.value("body").toString();
        message.time = frame.value("time").toString().toLongLong();
        message.color = frame.value("color").toInt();
        return message;
    }
};
