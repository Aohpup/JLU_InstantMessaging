#include "storage/GroupRoomStoreLJY.h"
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>
#include <sqlite3.h>
#include <stdexcept>
namespace
{
struct RoomStatementLJY
{
    sqlite3_stmt *p = nullptr;
    ~RoomStatementLJY() { sqlite3_finalize(p); }
};
void check(sqlite3 *db, int result)
{
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW)
        throw std::runtime_error(sqlite3_errmsg(db));
}
void sql(sqlite3 *db, const char *text) { check(db, sqlite3_exec(db, text, nullptr, nullptr, nullptr)); }
void bind(sqlite3 *db, sqlite3_stmt *s, int i, const QString &text)
{
    auto b = text.toUtf8();
    check(db, sqlite3_bind_text(s, i, b.constData(), b.size(), SQLITE_TRANSIENT));
}
QString text(sqlite3_stmt *s, int i) { return QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(s,i))); }
}
GroupRoomStoreLJY::GroupRoomStoreLJY(const QString &path, const QString &room)
{
    try
    {
        check(db_, sqlite3_open(path.toUtf8().constData(), &db_));
        sqlite3_busy_timeout(db_, 1000);
        sql(db_, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; "
                 "CREATE TABLE IF NOT EXISTS room_identity(room TEXT PRIMARY KEY, identity TEXT NOT NULL);"
                 "CREATE TABLE IF NOT EXISTS room_messages(seq INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "sender TEXT NOT NULL, request TEXT NOT NULL, frame TEXT NOT NULL, UNIQUE(sender,request));");
        RoomStatementLJY q;
        check(db_, sqlite3_prepare_v2(db_, "SELECT room,identity FROM room_identity", -1, &q.p, nullptr));
        int r = sqlite3_step(q.p); check(db_, r);
        if (r == SQLITE_ROW)
        {
            if (text(q.p,0) != room) throw std::runtime_error("该日志属于其他群，不能混用。");
            identity_ = text(q.p,1);
        }
        else
        {
            identity_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
            RoomStatementLJY insert;
            check(db_, sqlite3_prepare_v2(db_, "INSERT INTO room_identity VALUES(?,?)", -1, &insert.p,nullptr));
            bind(db_,insert.p,1,room); bind(db_,insert.p,2,identity_); check(db_,sqlite3_step(insert.p));
        }
    }
    catch (...) { sqlite3_close(db_); db_ = nullptr; throw; }
}
GroupRoomStoreLJY::~GroupRoomStoreLJY() { sqlite3_close(db_); }
long long GroupRoomStoreLJY::last() const
{
    RoomStatementLJY q;
    check(db_,sqlite3_prepare_v2(db_,"SELECT COALESCE(MAX(seq),0) FROM room_messages",-1,&q.p,nullptr));
    check(db_,sqlite3_step(q.p)); return sqlite3_column_int64(q.p,0);
}
QJsonObject GroupRoomStoreLJY::append(QJsonObject frame)
{
    sql(db_,"BEGIN IMMEDIATE");
    try
    {
        RoomStatementLJY existing;
        check(db_,sqlite3_prepare_v2(db_,"SELECT frame FROM room_messages WHERE sender=? AND request=?",-1,&existing.p,nullptr));
        bind(db_,existing.p,1,frame.value("sender").toString()); bind(db_,existing.p,2,frame.value("request").toString());
        int r=sqlite3_step(existing.p); check(db_,r);
        if(r==SQLITE_ROW) { auto result=QJsonDocument::fromJson(text(existing.p,0).toUtf8()).object(); sql(db_,"COMMIT");return result; }
        // 时间也保持递增，使补回的旧记录可以按时间排入客户端原有记录。
        long long time=QDateTime::currentMSecsSinceEpoch();
        RoomStatementLJY previous;
        check(db_,sqlite3_prepare_v2(db_,"SELECT frame FROM room_messages ORDER BY seq DESC LIMIT 1",-1,&previous.p,nullptr));
        r=sqlite3_step(previous.p);check(db_,r);
        if(r==SQLITE_ROW) time=std::max(time,QJsonDocument::fromJson(text(previous.p,0).toUtf8()).object().value("time").toString().toLongLong()+1);
        RoomStatementLJY insert;
        check(db_,sqlite3_prepare_v2(db_,"INSERT INTO room_messages(sender,request,frame) VALUES(?,?, '')",-1,&insert.p,nullptr));
        bind(db_,insert.p,1,frame.value("sender").toString());bind(db_,insert.p,2,frame.value("request").toString());check(db_,sqlite3_step(insert.p));
        auto seq=sqlite3_last_insert_rowid(db_);
        frame["id"]=identity_+":"+QString::number(seq); frame["seq"]=QString::number(seq); frame["time"]=QString::number(time);
        RoomStatementLJY update;
        check(db_,sqlite3_prepare_v2(db_,"UPDATE room_messages SET frame=? WHERE seq=?",-1,&update.p,nullptr));
        bind(db_,update.p,1,QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact)));
        sqlite3_bind_int64(update.p,2,seq);check(db_,sqlite3_step(update.p));sql(db_,"COMMIT");return frame;
    }
    catch(...) { sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw; }
}
QVector<QJsonObject> GroupRoomStoreLJY::page(long long after, long long through) const
{
    RoomStatementLJY q;
    check(db_,sqlite3_prepare_v2(db_,"SELECT frame FROM room_messages WHERE seq>? AND seq<=? ORDER BY seq LIMIT 50",-1,&q.p,nullptr));
    sqlite3_bind_int64(q.p,1,after);sqlite3_bind_int64(q.p,2,through);
    QVector<QJsonObject> rows;int bytes=0,r;
    while((r=sqlite3_step(q.p))==SQLITE_ROW)
    {
        auto b=text(q.p,0).toUtf8();
        if(!rows.isEmpty() && bytes+b.size()>128*1024) break;
        bytes+=b.size();rows.append(QJsonDocument::fromJson(b).object());
    }
    check(db_,r);return rows;
}
