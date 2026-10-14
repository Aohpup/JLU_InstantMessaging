#include "network/GroupChatProtocolLJY.h"
#include <QJsonDocument>
#include <stdexcept>
QByteArray GroupChatProtocolLJY::encode(const QJsonObject &object)
{
    const auto body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (body.size() > 65536)
        throw std::runtime_error("群消息帧过长。");
    QByteArray result(4, '\0');
    const unsigned int size = static_cast<unsigned int>(body.size());
    for (int i = 0; i < 4; ++i)
        result[i] = static_cast<char>((size >> (24 - i * 8)) & 255);
    return result + body;
}
QVector<QJsonObject> GroupChatProtocolLJY::feed(QByteArray &buffer, const QByteArray &bytes)
{
    buffer += bytes;
    QVector<QJsonObject> result;
    while (buffer.size() >= 4)
    {
        unsigned int size = 0;
        for (int i = 0; i < 4; ++i)
            size = (size << 8) | static_cast<unsigned char>(buffer[i]);
        if (size == 0 || size > 65536)
            throw std::runtime_error("无效的群消息帧长度。");
        if (buffer.size() < static_cast<int>(size) + 4)
            break;
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(buffer.mid(4, size), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            throw std::runtime_error("群消息格式错误。");
        result.push_back(doc.object());
        buffer.remove(0, size + 4);
    }
    return result;
}
