#include "storage/SqliteStorageLJY.h"
#include "services/PlatformLJY.h"
#include "SqliteSchemaLJY.h"
#include <sqlite3.h>
#include <filesystem>
#include <charconv>
#include <limits>
#include <stdexcept>

SqliteStorageLJY::SqliteStorageLJY(const std::string& databasePath)
{
    if (databasePath.empty()) throw std::invalid_argument("数据库路径不能为空。");
    const auto directory = std::filesystem::u8path(databasePath).parent_path();
    if (!directory.empty()) std::filesystem::create_directories(directory);
    try
    {
        check(sqlite3_open(databasePath.c_str(), &db_), "打开数据库失败");
        execute("PRAGMA foreign_keys=ON;");
        execute("BEGIN;");
        execute(sqliteSchemaLJY);
        // 旧库只补头像字段；-1 沿用原来的注册顺序配色，不改变已有资料。
        bool hasAvatarColor = false;
        prepare("PRAGMA table_info(persons);");
        while (readRow()) if (readText(1) == "avatar_color") hasAvatarColor = true;
        finishStatement();
        if (!hasAvatarColor) execute("ALTER TABLE persons ADD COLUMN avatar_color INTEGER NOT NULL DEFAULT -1 CHECK (avatar_color BETWEEN -1 AND 5);");
        execute("COMMIT;");
    }
    catch (...)
    {
        if (db_)
        {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw;
    }
}
SqliteStorageLJY::~SqliteStorageLJY()
{
    finishStatement();
    if (db_) sqlite3_close(db_);
}
void SqliteStorageLJY::check(int result, const char* action) const
{
    if (result != SQLITE_OK)
        throw std::runtime_error(std::string(action) + "：" + (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(result)));
}
void SqliteStorageLJY::execute(const char* sql)
{
    check(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), "执行数据库语句失败");
}
void SqliteStorageLJY::finishStatement() noexcept
{
    if (statement_) sqlite3_finalize(statement_);
    statement_ = nullptr;
}
void SqliteStorageLJY::prepare(const char* sql)
{
    finishStatement();
    check(sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr), "准备数据库语句失败");
}
void SqliteStorageLJY::bind(int index, int value)
{
    check(sqlite3_bind_int(statement_, index, value), "绑定整数失败");
}
void SqliteStorageLJY::bind(int index, const std::string& value)
{
    check(sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT),
        "绑定文本失败");
}
void SqliteStorageLJY::writeRow()
{
    const int result = sqlite3_step(statement_);
    if (result != SQLITE_DONE) check(result, "写入数据库失败");
    check(sqlite3_reset(statement_), "重置保存语句失败");
    check(sqlite3_clear_bindings(statement_), "清理保存参数失败");
}
void SqliteStorageLJY::save(const PlatformLJY& platform)
{
    execute("BEGIN IMMEDIATE;");
    try
    {
        // 外键保持开启：先删除依赖记录，群先子后父，最后清理人员。
        execute("DELETE FROM friend_requests; DELETE FROM group_requests; DELETE FROM group_members;"
                "DELETE FROM chat_groups WHERE parent_group_id IS NOT NULL; DELETE FROM chat_groups;"
                "DELETE FROM friendships; DELETE FROM account_bindings; DELETE FROM credentials;"
                "DELETE FROM service_accounts; DELETE FROM persons; DELETE FROM id_state;");

        prepare("INSERT INTO persons(person_id,name,birth_date,location,avatar_color) VALUES(?,?,?,?,?);");
        for (const auto& entry : platform.people_)
        {
            const auto& person = entry.second;
            bind(1, person.personId()); bind(2, person.name());
            bind(3, person.birthDate()); bind(4, person.location()); bind(5, person.avatarColor()); writeRow();
        }
        prepare("INSERT INTO service_accounts(account_id,person_id,service,external_id,nickname,applied_on) VALUES(?,?,?,?,?,?);");
        for (const auto& entry : platform.accounts_)
        {
            const auto& account = entry.second;
            bind(1, account.accountId()); bind(2, account.personId());
            bind(3, static_cast<int>(account.serviceType())); bind(4, account.externalId());
            bind(5, account.nickname()); bind(6, account.appliedOn()); writeRow();
        }
        prepare("INSERT INTO credentials(person_id,password) VALUES(?,?);");
        for (const auto& entry : platform.credentials_)
        {
            bind(1, entry.first); bind(2, entry.second.password_); writeRow();
        }
        prepare("INSERT INTO account_bindings(wechat_account_id,qq_account_id) VALUES(?,?);");
        for (const auto& entry : platform.weChatToQQ_)
        {
            bind(1, entry.first); bind(2, entry.second); writeRow();
        }
        prepare("INSERT INTO friendships(first_account_id,second_account_id,first_remark,second_remark) VALUES(?,?,?,?);");
        for (const auto& entry : platform.friendRelations_)
        {
            const auto& relation = entry.second;
            bind(1, relation.firstAccountId()); bind(2, relation.secondAccountId());
            bind(3, relation.remarkFor(relation.firstAccountId()));
            bind(4, relation.remarkFor(relation.secondAccountId())); writeRow();
        }
        prepare("INSERT INTO friend_requests(first_account_id,second_account_id,sender_account_id,recipient_account_id,state) VALUES(?,?,?,?,?);");
        for (const auto& entry : platform.friendRequests_)
        {
            bind(1,entry.first.first); bind(2,entry.first.second);
            bind(3,entry.second.senderId()); bind(4,entry.second.recipientId()); bind(5,entry.second.state()); writeRow();
        }
        prepare("INSERT INTO chat_groups(group_id,service,number,name,owner_account_id,style,parent_group_id) VALUES(?,?,?,?,?,?,?);");
        // 当前只支持一级子群，两遍分别保存顶级群和子群。
        for (int child = 0; child <= 1; ++child)
            for (const auto& entry : platform.groups_)
            {
                const auto& group = entry.second;
                if ((group.parentGroupId() != 0) != (child != 0)) continue;
                bind(1, group.groupId()); bind(2, static_cast<int>(group.serviceType()));
                bind(3, group.number()); bind(4, group.name()); bind(5, group.ownerAccountId());
                bind(6, static_cast<int>(group.style()));
                if (child) bind(7, group.parentGroupId());
                else check(sqlite3_bind_null(statement_, 7), "绑定父群空值失败");
                writeRow();
            }
        prepare("INSERT INTO group_members(group_id,account_id,is_administrator,member_order) VALUES(?,?,?,?);");
        for (const auto& entry : platform.groups_)
        {
            int order = 0;
            for (const auto& member : entry.second.members())
            {
                bind(1, entry.first); bind(2, member.accountId());
                bind(3, member.isAdministrator() ? 1 : 0); bind(4, order++); writeRow();
            }
        }
        prepare("INSERT INTO group_requests(request_id,service,group_number,type,sender_number,target_number,state) VALUES(?,?,?,?,?,?,?);");
        for (const auto& entry : platform.groupRequests_)
        {
            const auto& request = entry.second;
            bind(1, request.id()); bind(2, static_cast<int>(request.serviceType()));
            bind(3, request.groupNumber()); bind(4, static_cast<int>(request.type()));
            bind(5, request.senderNumber()); bind(6, request.targetNumber());
            bind(7, static_cast<int>(request.state())); writeRow();
        }
        prepare("INSERT INTO id_state(singleton,last_person_id,last_account_id,last_shared_number,last_wechat_number,last_group_id,last_group_request_id) VALUES(1,?,?,?,?,?,?);");
        bind(1, platform.lastPersonId_); bind(2, platform.lastAccountId_);
        bind(3, platform.lastSharedNumber_); bind(4, platform.lastWeChatNumber_);
        bind(5, platform.lastGroupId_); bind(6, platform.lastGroupRequestId_); writeRow();
        finishStatement();
        execute("COMMIT;");
    }
    catch (...)
    {
        finishStatement();
        // 保留原异常信息；回滚数据库，不回滚调用方仍可重试保存的内存数据。
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}

bool SqliteStorageLJY::readRow()
{
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) return true;
    if (result != SQLITE_DONE) check(result, "读取数据库失败");
    return false;
}
int SqliteStorageLJY::readInt(int column) const
{
    const auto value = sqlite3_column_int64(statement_, column);
    if (sqlite3_column_type(statement_, column) != SQLITE_INTEGER ||
        value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::runtime_error("数据库整数字段无效或超出 int 范围。");
    return static_cast<int>(value);
}
std::string SqliteStorageLJY::readText(int column) const
{
    if (sqlite3_column_type(statement_, column) != SQLITE_TEXT)
        throw std::runtime_error("数据库文本字段无效。");
    const auto* value = sqlite3_column_text(statement_, column);
    if (!value) throw std::runtime_error("读取数据库文本失败。");
    return std::string(reinterpret_cast<const char*>(value), sqlite3_column_bytes(statement_, column));
}
void SqliteStorageLJY::load(PlatformLJY& platform)
{
    PlatformLJY restored;
    execute("BEGIN;");
    try
    {
        // 在同一次读取事务内检查外键，后续可按依赖直接取出关联对象。
        prepare("PRAGMA foreign_key_check;");
        if (readRow()) throw std::runtime_error("数据库存在无效引用，请检查表之间的关联。");

        prepare("SELECT person_id,name,birth_date,location,avatar_color FROM persons ORDER BY person_id;");
        while (readRow())
        {
            const int id = readInt(0);
            if (id <= 0) throw std::runtime_error("数据库人员编号必须为正数。");
            restored.people_.emplace(id, PersonLJY(id, readText(1), readText(2), readText(3), readInt(4)));
        }
        prepare("SELECT account_id,person_id,service,external_id,nickname,applied_on FROM service_accounts ORDER BY account_id;");
        while (readRow())
        {
            const int id = readInt(0), service = readInt(2);
            if (id <= 0 || service < 0 || service > 2)
                throw std::runtime_error("数据库账号编号或服务类型无效。");
            restored.accounts_.emplace(id, ServiceAccountLJY(id, readInt(1),
                static_cast<ServiceTypeLJY>(service), readText(3), readText(4), readText(5)));
        }
        prepare("SELECT person_id,password FROM credentials;");
        while (readRow()) restored.credentials_.emplace(readInt(0), PasswordCredentialLJY(readText(1)));
        prepare("SELECT wechat_account_id,qq_account_id FROM account_bindings;");
        while (readRow()) restored.weChatToQQ_.emplace(readInt(0), readInt(1));
        prepare("SELECT first_account_id,second_account_id,first_remark,second_remark FROM friendships;");
        while (readRow())
        {
            const int first = readInt(0), second = readInt(1);
            FriendRelationLJY relation(first, second);
            relation.setRemark(first, readText(2)); relation.setRemark(second, readText(3));
            restored.friendRelations_.emplace(std::make_pair(first, second), std::move(relation));
        }
        prepare("SELECT sender_account_id,recipient_account_id,state FROM friend_requests;");
        while (readRow())
        {
            const int sender=readInt(0), recipient=readInt(1);
            restored.friendRequests_.emplace(std::make_pair(std::min(sender,recipient),std::max(sender,recipient)), FriendRequestLJY(sender,recipient,readInt(2)));
        }
        prepare("SELECT group_id,service,number,name,owner_account_id,style,parent_group_id FROM chat_groups ORDER BY group_id;");
        while (readRow())
        {
            const int id = readInt(0);
            GroupLJY group(id, static_cast<ServiceTypeLJY>(readInt(1)), readInt(2), readText(3),
                readInt(4), static_cast<GroupStyleLJY>(readInt(5)));
            group.parentGroupId_ = sqlite3_column_type(statement_, 6) == SQLITE_NULL ? 0 : readInt(6);
            // 构造函数已加入群主；读取成员表前清空，防止重复并保留原顺序。
            group.members_.clear();
            restored.groups_.emplace(id, std::move(group));
        }
        prepare("SELECT group_id,account_id,is_administrator,member_order FROM group_members ORDER BY group_id,member_order;");
        while (readRow())
        {
            auto& group = restored.groups_.at(readInt(0));
            const int administrator = readInt(2);
            if ((administrator != 0 && administrator != 1) || readInt(3) != static_cast<int>(group.members_.size()))
                throw std::runtime_error("数据库群成员顺序或管理员标记无效。");
            group.members_.emplace_back(readInt(1), administrator != 0);
        }
        prepare("SELECT request_id,service,group_number,type,sender_number,target_number,state FROM group_requests ORDER BY request_id;");
        while (readRow())
        {
            const int id = readInt(0), state = readInt(6);
            if (state < 0 || state > 3) throw std::runtime_error("数据库群请求状态无效。");
            GroupRequestLJY request(id, static_cast<ServiceTypeLJY>(readInt(1)), readInt(2),
                static_cast<GroupRequestTypeLJY>(readInt(3)), readText(4), readText(5));
            request.state_ = static_cast<GroupRequestStateLJY>(state);
            restored.groupRequests_.emplace(id, std::move(request));
        }
        prepare("SELECT last_person_id,last_account_id,last_shared_number,last_wechat_number,last_group_id,last_group_request_id FROM id_state WHERE singleton=1;");
        if (!readRow()) throw std::runtime_error("数据库缺少编号状态。");
        restored.lastPersonId_ = readInt(0); restored.lastAccountId_ = readInt(1);
        restored.lastSharedNumber_ = readInt(2); restored.lastWeChatNumber_ = readInt(3);
        restored.lastGroupId_ = readInt(4); restored.lastGroupRequestId_ = readInt(5);
        finishStatement();
        validateLoaded(restored);
        execute("COMMIT;");
        // 临时平台没有登录历史；全部成功后才替换，原平台的查询指针随之失效。
        platform = std::move(restored);
    }
    catch (...)
    {
        finishStatement();
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}
void SqliteStorageLJY::validateLoaded(const PlatformLJY& platform) const
{
    // 外键检查引用是否存在，这里只补充跨表的业务归属和发号检查。
    for (const auto& entry : platform.weChatToQQ_)
    {
        const auto& wx = platform.accounts_.at(entry.first);
        const auto& qq = platform.accounts_.at(entry.second);
        if (wx.serviceType() != ServiceTypeLJY::WeChat || qq.serviceType() != ServiceTypeLJY::QQ || wx.personId() != qq.personId())
            throw std::runtime_error("数据库绑定必须属于同一人的微信和 QQ 账号。");
    }
    for (const auto& entry : platform.friendRelations_)
        if (platform.accounts_.at(entry.first.first).serviceType() != platform.accounts_.at(entry.first.second).serviceType())
            throw std::runtime_error("数据库好友双方必须属于同一服务。");
    for (const auto& entry : platform.friendRequests_)
        if (platform.accounts_.at(entry.second.senderId()).serviceType() != platform.accounts_.at(entry.second.recipientId()).serviceType())
            throw std::runtime_error("好友申请双方必须属于同一服务。");
    for (const auto& entry : platform.groups_)
    {
        const auto& group = entry.second;
        if (!group.findMember(group.ownerAccountId())) throw std::runtime_error("数据库群主不在本群成员中。");
        const auto* parent = group.parentGroupId() == 0 ? nullptr : platform.findGroup(group.parentGroupId());
        if (parent && (parent->parentGroupId() != 0 || parent->serviceType() != group.serviceType()))
            throw std::runtime_error("数据库子群的父群或所属服务关联无效。");
        for (const auto& member : group.members())
        {
            if (platform.accounts_.at(member.accountId()).serviceType() != group.serviceType())
                throw std::runtime_error("数据库群成员不属于群所在服务。");
            if (parent && !parent->findMember(member.accountId()))
                throw std::runtime_error("数据库子群成员不在父群中。");
        }
    }
    const int maxPerson = platform.people_.empty() ? 0 : platform.people_.rbegin()->first;
    const int maxAccount = platform.accounts_.empty() ? 0 : platform.accounts_.rbegin()->first;
    const int maxGroup = platform.groups_.empty() ? 0 : platform.groups_.rbegin()->first;
    const int maxRequest = platform.groupRequests_.empty() ? 0 : platform.groupRequests_.rbegin()->first;
    if (platform.lastPersonId_ < maxPerson || platform.lastAccountId_ < maxAccount ||
        platform.lastGroupId_ < maxGroup || platform.lastGroupRequestId_ < maxRequest ||
        platform.lastSharedNumber_ < 10000 || platform.lastWeChatNumber_ < 20000)
        throw std::runtime_error("数据库编号状态落后于已有数据。");
    for (const auto& entry : platform.accounts_)
    {
        const auto& account = entry.second;
        const bool weChat = account.serviceType() == ServiceTypeLJY::WeChat;
        const auto& number = account.externalId();
        const std::string prefix = weChat ? "wx_" : "";
        int value = 0;
        if (number.size() <= prefix.size() || number.compare(0, prefix.size(), prefix) != 0)
            throw std::runtime_error("数据库账号号码格式无效。");
        const auto parsed = std::from_chars(number.data() + prefix.size(), number.data() + number.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
            value <= (weChat ? 20000 : 10000) || number != prefix + std::to_string(value) ||
            value > (weChat ? platform.lastWeChatNumber_ : platform.lastSharedNumber_))
            throw std::runtime_error("数据库账号号码或发号状态无效。");
        if (!weChat)
        {
            const auto otherType = account.serviceType() == ServiceTypeLJY::QQ ? ServiceTypeLJY::Weibo : ServiceTypeLJY::QQ;
            const auto* other = platform.findAccount(otherType, number);
            const auto* ownOther = platform.findAccountForPerson(account.personId(), otherType);
            if ((other && other->personId() != account.personId()) || (ownOther && ownOther->externalId() != number))
                throw std::runtime_error("数据库 QQ 与微博共享号码不一致。");
        }
    }
}
