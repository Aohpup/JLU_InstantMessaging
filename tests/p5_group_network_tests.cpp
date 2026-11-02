#include "network/GroupChatClientLJY.h"
#include "network/GroupChatProtocolLJY.h"
#include "storage/GroupChatHistoryLJY.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>
#include <sqlite3.h>
#include <functional>
#include <iostream>
#include <stdexcept>
void check(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}
void waitFor(const std::function<bool()> &condition, const char *why, int ms = 9000)
{
    QElapsedTimer t;
    t.start();
    while (!condition() && t.elapsed() < ms)
        QTest::qWait(10);
    check(condition(), why);
}
int freePort()
{
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost, 0), "free port");
    return server.serverPort();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try
    {
        QJsonObject object{{"kind", "text"}, {"body", "中文\n消息"}};
        auto encoded = GroupChatProtocolLJY::encode(object);
        QByteArray buffer;
        check(GroupChatProtocolLJY::feed(buffer, encoded.left(3)).isEmpty(), "partial header");
        check(GroupChatProtocolLJY::feed(buffer, encoded.mid(3) + encoded).size() == 2,
              "split and coalesced frames");
        bool invalid = false;
        try
        {
            QByteArray b;
            GroupChatProtocolLJY::feed(b, QByteArray(4, '\xff'));
        }
        catch (...)
        {
            invalid = true;
        }
        check(invalid, "oversized frame");
        GroupChatMembersLJY members{{"10001", {"群主", 0}}, {"10002", {"成员B", 1}}, {"10003", {"成员C", 2}}};
        GroupChatClientLJY a, b, c;
        int na = 0, nb = 0, nc = 0, online = 0;
        QString id, prior;
        QObject::connect(&a, &GroupChatClientLJY::message, [&](const QJsonObject &m) {
            ++na;
            id = m.value("id").toString();
            check(m.value("sender") == "10002", "host changed sender");
        });
        QObject::connect(&b, &GroupChatClientLJY::message, [&](const QJsonObject &) { ++nb; });
        QObject::connect(&c, &GroupChatClientLJY::message, [&](const QJsonObject &) { ++nc; });
        QObject::connect(&a, &GroupChatClientLJY::online, [&](int n) { online = n; });
        const int port = freePort();
        c.enter("0:1001", "10003", members, port);
        waitFor([&] { return c.ready(); }, "ordinary first member host");
        check(c.hosting(), "ordinary not host");
        a.enter("0:1001", "10001", members, port);
        b.enter("0:1001", "10002", members, port);
        waitFor([&] { return a.ready() && b.ready() && online == 3; }, "three members auto join");
        b.sendText("三人群消息\n中文🙂");
        waitFor([&] { return na == 1 && nb == 1 && nc == 1; }, "fanout including sender");
        prior = id;
        GroupChatClientLJY wrong;
        bool rejected = false;
        QObject::connect(&wrong, &GroupChatClientLJY::status,
                         [&](const QString &, bool failed) { rejected |= failed; });
        wrong.enter("1:1001", "10001", members, port);
        waitFor([&] { return rejected; }, "wrong service accepted");
        check(!wrong.ready(), "wrong room ready");
        wrong.leave();
        rejected = false;
        wrong.enter("0:1001", "99999", members, port);
        waitFor([&] { return rejected; }, "nonmember accepted");
        wrong.leave();
        rejected = false;
        wrong.enter("0:1001", "10001", members, port);
        waitFor([&] { return rejected; }, "duplicate account accepted");
        wrong.leave();
        auto reduced = members;
        reduced.remove("10002");
        c.updateMembers(reduced);
        waitFor([&] { return !b.ready() && online == 2; }, "removed member still connected");
        c.updateMembers(members);
        b.enter("0:1001", "10002", members, port);
        waitFor([&] { return b.ready(); }, "restored member cannot join");
        c.leave();
        waitFor([&] { return a.ready() && b.ready() && (a.hosting() != b.hosting()); }, "host takeover");
        b.sendText("主持端退出后继续发送");
        waitFor([&] { return na == 2 && nb == 2; }, "takeover fanout");
        check(id != prior, "room restart reused message ID");
        a.leave();
        b.leave();
        QTemporaryDir folder;
        const auto path = folder.path() + "/history.db";
        sqlite3 *create = nullptr;
        check(sqlite3_open(path.toUtf8().constData(), &create) == SQLITE_OK, "create DB");
        sqlite3_close(create);
        GroupChatHistoryLJY history(path);
        for (int i = 0; i < 105; ++i)
        {
            GroupChatMessageLJY m;
            m.id = QString::number(i);
            m.sender = "10002";
            m.nickname = "测试";
            m.body = "历史";
            m.time = i;
            history.save(0, "10001", 1001, m);
            history.save(0, "10001", 1001, m);
        }
        auto recent = history.load(0, "10001", 1001);
        check(recent.size() == 100, "history dedup/page");
        check(history.load(0, "10001", 1001, recent.front().rowId).size() == 5, "older page");
        check(history.load(1, "10001", 1001).empty() && history.load(0, "10002", 1001).empty() &&
                  history.load(0, "10001", 2001).empty(),
              "history identity isolation");
        sqlite3 *lock = nullptr;
        sqlite3_open(path.toUtf8().constData(), &lock);
        sqlite3_exec(lock, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        bool busy = false;
        try
        {
            history.save(0, "10001", 1001, recent.back());
        }
        catch (...)
        {
            busy = true;
        }
        check(busy, "write lock not surfaced");
        sqlite3_exec(lock, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(lock);
        std::cout
            << "PASS frames, first ordinary member, three-way auto chat, service/member/duplicate checks, "
               "removal, takeover, history paging/dedup/isolation/lock\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
