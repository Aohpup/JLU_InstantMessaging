#include "network/GroupChatHostLJY.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QDir>
#include <QLockFile>
#include <iostream>

// 一个进程保管一个群房间，退出图形页面不会关闭它；日志与客户端数据库分开。
int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    QCommandLineParser args;
    args.setApplicationDescription("JLU 持久群房间服务；数据库与成员文件必须与群号对应。");
    args.addHelpOption();
    args.addOptions({{"room","服务序号:群号","room"},{"port","监听端口","port"},
        {"database","服务端日志路径","database"},{"members","成员 JSON 文件","members"},
        {"idle-seconds","无人连接后退出秒数；0 为一直运行","seconds","0"}});
    args.process(app);
    try
    {
        const auto path=args.value("database"), room=args.value("room");
        int port=args.value("port").toInt();
        if(path.isEmpty() || room.isEmpty() || port<1 || port>65535 || args.value("members").isEmpty())
            throw std::runtime_error("需要 --room、--port、--database 和 --members。");
        QLockFile lock(path+".lock");
        if(!lock.tryLock(0)) return 0;
        auto readMembers=[&]() {
            QFile f(args.value("members"));
            if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error("无法读取群成员文件。");
            auto doc=QJsonDocument::fromJson(f.readAll());
            if(!doc.isObject() || doc.object().value("room").toString()!=room)
                throw std::runtime_error("群成员文件与房间不匹配。");
            GroupChatMembersLJY members;
            for(auto value:doc.object().value("members").toArray())
            { auto m=value.toObject(); members.insert(m.value("account").toString(),{m.value("nickname").toString(),m.value("color").toInt()}); }
            return members;
        };
        GroupChatHostLJY host;
        host.setDatabase(path);
        if(!host.start(room,port,"persistent",readMembers()) || host.port()!=port)
            throw std::runtime_error("端口不可用，请检查是否已运行其他群房间。");
        QTimer timer;
        int idle=0;
        QObject::connect(&timer,&QTimer::timeout,[&] {
            try { host.updateMembers(readMembers()); }
            catch(const std::exception &e) { std::cerr<<e.what()<<std::endl; host.updateMembers({}); app.quit(); }
            idle=host.connectedCount()==0 ? idle+1 : 0;
            if(args.value("idle-seconds").toInt()>0 && idle>=args.value("idle-seconds").toInt()) app.quit();
        });
        timer.start(1000);
        std::cout<<"READY "<<room.toStdString()<<" "<<host.port()<<std::endl;
        return app.exec();
    }
    catch(const std::exception &e) { std::cerr<<e.what()<<std::endl; return 1; }
}
