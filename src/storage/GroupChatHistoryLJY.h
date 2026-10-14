#pragma once
#include "domain/GroupChatMessageLJY.h"
#include <vector>
struct sqlite3;
// 群消息独立建表，不随平台账号/群的整体保存删除。
class GroupChatHistoryLJY
{
  public:
    explicit GroupChatHistoryLJY(const QString &path);
    ~GroupChatHistoryLJY();
    GroupChatHistoryLJY(const GroupChatHistoryLJY &) = delete;
    GroupChatHistoryLJY &operator=(const GroupChatHistoryLJY &) = delete;
    void save(int service, const QString &owner, int group, const GroupChatMessageLJY &message);
    std::vector<GroupChatMessageLJY> load(int service, const QString &owner, int group, long long before = 0,
                                          int limit = 100);

  private:
    sqlite3 *db_ = nullptr;
    void check(int result);
};
