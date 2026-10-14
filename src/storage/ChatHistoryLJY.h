#pragma once
#include "domain/ChatMessageLJY.h"
#include <vector>
struct sqlite3;
struct sqlite3_stmt;

// 本地行号用于向前分页；消息编号用于去重，两者用途不同。
struct StoredChatMessageLJY
{
    long long rowId = 0;
    ChatMessageLJY message;
};

// 独立保存聊天记录，只在 GUI 主线程使用，不参加 Platform 的整库替换。
class ChatHistoryLJY
{
public:
    // 创建缺失的消息表和索引；已有账号、好友、群数据保持不变。
    explicit ChatHistoryLJY(const std::string& databasePath);
    ~ChatHistoryLJY();
    ChatHistoryLJY(const ChatHistoryLJY&) = delete;
    ChatHistoryLJY& operator=(const ChatHistoryLJY&) = delete;
    // 首次逐条插入，同一服务、账号及消息编号再次写入时只更新发送状态。
    void save(const std::string& owner, const ChatMessageLJY& message);
    // 按服务及双方账号隔离，默认最近 100 条，按本地行号正序返回；beforeId 为 0 时从最新开始。
    // 上次留下的发送中状态按结果未知显示，不恢复连接或自动重发。
    std::vector<StoredChatMessageLJY> load(const std::string& owner, const std::string& peer,
                                        long long beforeId = 0, int limit = 100,
                                        ServiceTypeLJY service = ServiceTypeLJY::QQ);
private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* statement_ = nullptr;
    void check(int result) const;
    void prepare(const char* sql);
    void bind(int index, const std::string& value);
    void finish();
};
