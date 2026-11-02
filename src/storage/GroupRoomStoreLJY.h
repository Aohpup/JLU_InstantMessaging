#pragma once
#include <QJsonObject>
#include <QVector>
#include <QString>
struct sqlite3;
// 服务端群日志：先落库再确认，编号与房间标识在进程重启后保持不变。
class GroupRoomStoreLJY
{
public:
    explicit GroupRoomStoreLJY(const QString &path, const QString &room);
    ~GroupRoomStoreLJY();
    GroupRoomStoreLJY(const GroupRoomStoreLJY &) = delete;
    GroupRoomStoreLJY &operator=(const GroupRoomStoreLJY &) = delete;
    QString identity() const { return identity_; }
    long long last() const;
    // 同一发送者的同一提交编号只记录一次。
    QJsonObject append(QJsonObject frame);
    // 固定上界分页，避免同步期间的新消息混入历史快照。
    QVector<QJsonObject> page(long long after, long long through) const;
private:
    sqlite3 *db_ = nullptr;
    QString identity_;
};
