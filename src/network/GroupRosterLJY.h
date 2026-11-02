#pragma once
#include "network/GroupChatHostLJY.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <stdexcept>
// 同机群服务共用一份名单；锁保护读取、合并、原子提交这三个步骤。
inline void saveGroupRosterLJY(const QString& directory, const QString& room,
    const GroupChatMembersLJY& before, const GroupChatMembersLJY& after, bool initialize=false)
{
    QDir().mkpath(directory);
    auto key=room; key.replace(":","_");
    const auto path=directory+"/"+key+".json";
    QLockFile lock(path+".lock");
    if (!lock.tryLock(1000)) throw std::runtime_error("群名单正在更新，请稍后重试。");
    QFile input(path);
    if (initialize && input.exists()) return;
    GroupChatMembersLJY merged;
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("无法读取群服务名单。");
        QJsonParseError error;
        const auto object=QJsonDocument::fromJson(input.readAll(),&error).object();
        if (error.error!=QJsonParseError::NoError || object.value("room").toString()!=room)
            throw std::runtime_error("群服务名单格式不正确，未覆盖原文件。");
        for (const auto& row:object.value("members").toArray()) {
            const auto m=row.toObject();
            merged[m.value("account").toString()]={m.value("nickname").toString(),m.value("color").toInt()};
        }
    } else merged=after;
    input.close();
    for (auto it=before.begin();it!=before.end();++it)
        if (!after.contains(it.key())) merged.remove(it.key());
    for (auto it=after.begin();it!=after.end();++it)
        if (!before.contains(it.key())) merged[it.key()]=it.value();
        else if (merged.contains(it.key()) && it.value()!=before.value(it.key())) merged[it.key()]=it.value();
    QJsonArray rows;
    for (auto it=merged.begin();it!=merged.end();++it)
        rows.append(QJsonObject{{"account",it.key()},{"nickname",it.value().first},{"color",it.value().second}});
    QSaveFile output(path);
    const auto bytes=QJsonDocument(QJsonObject{{"room",room},{"members",rows}}).toJson();
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())
        throw std::runtime_error("群服务名单保存失败。");
}
