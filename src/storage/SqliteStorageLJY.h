#pragma once
#include <string>

struct sqlite3;
struct sqlite3_stmt;
class PlatformLJY;

// 拥有一个数据库连接，负责建表、完整保存和读取；业务规则仍由原管理类处理。
class SqliteStorageLJY
{
public:
    // 路径使用 UTF-8；创建父目录和缺失的表，已有数据不清空，失败抛出异常。
    explicit SqliteStorageLJY(const std::string& databasePath);

    // 只释放语句和连接，不隐式保存平台数据。
    ~SqliteStorageLJY();

    // 连接只有一个所有者，禁止复制，避免重复关闭。
    SqliteStorageLJY(const SqliteStorageLJY&) = delete;
    SqliteStorageLJY& operator=(const SqliteStorageLJY&) = delete;

    // 用一次事务替换本项目十张业务表的数据；失败回滚，平台内存保持原样。
    // 只传入本次准备保存的完整平台，不把未加载的空平台当作已有数据保存。
    void save(const PlatformLJY& platform);

    // 成功替换完整平台并使旧查询指针失效，失败保留原平台；默认不保留本次运行的登录历史。
    void load(PlatformLJY& platform, bool preserveLoginHistory = false);

    // 其他窗口提交后重读业务数据，保留当前运行的密码登录历史；只在界面空闲时调用。
    // 会话保存的人员和账号编号不变；返回 true 时界面须重新查询列表，不能继续使用旧指针。
    bool refreshIfChanged(PlatformLJY& platform);

private:
    // 上次读取或提交的业务版本；聊天记录写入不改变它。
    int revision_ = 0;
    int readRevision();
    sqlite3* db_ = nullptr;// SQLite 数据库连接
    
    sqlite3_stmt* statement_ = nullptr;

    // 将 SQLite 返回码转成带操作说明的异常。
    void check(int result, const char* action) const;

    // 执行建表、事务和删除等固定 SQL，不拼接用户输入。
    void execute(const char* sql);

    // 准备下一种读写语句，先释放上一条语句。
    void prepare(const char* sql);

    // 将整数或文本绑定到从 1 开始的参数位置。
    void bind(int index, int value);
    void bind(int index, const std::string& value);

    // 写入一行并重置参数，供下一行复用相同语句。
    void writeRow();

    // 读取下一行，结果结束返回 false，数据库错误抛出异常。
    bool readRow();

    // 按从 0 开始的列号读取整数或文本，拒绝无法还原的字段类型。
    int readInt(int column) const;
    std::string readText(int column) const;

    // 加载完成后一次核对业务归属与编号，避免继续操作时出现重号。
    void validateLoaded(const PlatformLJY& platform) const;

    // 释放当前语句；正常提交和异常回滚前均调用。
    void finishStatement() noexcept;
};
