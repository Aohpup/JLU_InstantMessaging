#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QVector>

// 群协议与私聊独立：四字节大端长度 + UTF-8 JSON，一条连接保存一份缓冲。
class GroupChatProtocolLJY
{
  public:
    static QByteArray encode(const QJsonObject &object);
    static QVector<QJsonObject> feed(QByteArray &buffer, const QByteArray &bytes);
};
