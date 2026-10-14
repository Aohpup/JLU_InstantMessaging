#pragma once

#include "domain/PersonLJY.h"
#include "domain/FriendRequestLJY.h"
#include "domain/GroupLJY.h"
#include "domain/GroupRequestLJY.h"
#include "domain/FriendRelationLJY.h"
#include "domain/PasswordCredentialLJY.h"
#include "domain/ServiceAccountLJY.h"
#include "services/SocialServiceLJY.h"

#include <map>
#include <set>
#include <memory>
#include <vector>
#include <utility>

class AccountServiceLJY;
class SessionManagerLJY;
class FriendManagerLJY;
class GroupManagerLJY;
class SqliteStorageLJY;

// 平台拥有所有数据；公开接口只读，账号、好友与群业务维护数据，会话登记成功的密码登录记录。
// 查询返回的指针由平台管理，调用方不要释放，也不要在平台销毁后使用。
class PlatformLJY
{
public:
    // 初始化 QQ、微信和微博服务，人员与账号容器初始为空。
    PlatformLJY();

    // 按内部人员编号查询；不存在时返回 nullptr。
    const PersonLJY* findPerson(int personId) const;

    // 按内部账号编号查询；不存在时返回 nullptr。
    const ServiceAccountLJY* findAccount(int accountId) const;

    // 按服务类型和对外号码查询；不存在时返回 nullptr。
    const ServiceAccountLJY* findAccount(ServiceTypeLJY type, const std::string& externalId) const;

    // 查询某人已开通的指定服务账号；未找到时返回 nullptr。
    const ServiceAccountLJY* findAccountForPerson(int personId, ServiceTypeLJY type) const;

    // 收集某人的全部账号指针；人员不存在或未开通账号时返回空列表。
    std::vector<const ServiceAccountLJY*> accountsForPerson(int personId) const;

    // 取得服务的统一接口；服务类型未知时返回 nullptr。
    const SocialServiceLJY* findService(ServiceTypeLJY type) const;

    // 按微信内部账号编号查询已绑定的 QQ；未找到绑定时返回 nullptr。
    const ServiceAccountLJY* boundQQForWeChat(int weChatAccountId) const;

    // 按任意顺序查询好友对；不存在时返回 nullptr，引用在关系删除后失效。
    const FriendRelationLJY* findFriendRelation(int firstAccountId, int secondAccountId) const;

    // 收集账号参与的全部好友关系；结果只读，删除关系后不得继续使用旧指针。
    std::vector<const FriendRelationLJY*> friendRelationsForAccount(int accountId) const;

    // 返回双向好友对数量，同一对账号只计一次。
    int friendRelationCount() const noexcept;

    // 按内部编号或“服务＋群号”查群；不存在时返回 nullptr。
    const GroupLJY* findGroup(int groupId) const;
    const GroupLJY* findGroup(ServiceTypeLJY service, int number) const;
    // 返回平台的群数量。
    int groupCount() const noexcept;

    // 返回平台保存的人员总数。
    int personCount() const noexcept;

    // 返回平台保存的服务账号总数。
    int accountCount() const noexcept;

private:
    friend class AccountServiceLJY;
    friend class SessionManagerLJY;
    friend class FriendManagerLJY;
    friend class GroupManagerLJY;
    // 存储类保存与还原完整容器，菜单和其他类仍通过原接口访问。
    friend class SqliteStorageLJY;

    // 群以内部编号保存，各服务可有相同的对外群号。
    std::map<int, GroupLJY> groups_;
    int lastGroupId_ = 0;
    // 请求由群业务维护，保留处理结果供本人或当前管理者查看。
    std::map<int, GroupRequestLJY> groupRequests_;
    int lastGroupRequestId_ = 0;

    // 人员和账号分别以内部编号为键，账号通过人员编号建立归属关系。
    std::map<int, PersonLJY> people_;
    std::map<int, ServiceAccountLJY> accounts_;

    // 以从小到大的账号编号对为键，单条记录同时保存双方关系和独立备注。
    std::map<std::pair<int, int>, FriendRelationLJY> friendRelations_;
    // 键按账号编号排序，阻止双方同时重复发送待处理申请。
    std::map<std::pair<int, int>, FriendRequestLJY> friendRequests_;

    // 每个人员编号对应一份密码凭据，由账号业务注册和验证。
    std::map<int, PasswordCredentialLJY> credentials_;

    // 通过基类指针统一保存三种服务，并自动管理其生命周期。
    std::map<ServiceTypeLJY, std::unique_ptr<SocialServiceLJY>> services_;

    // 曾用密码成功登录的账号；注销不清除，平台销毁后丢弃，不代表当前登录状态。
    std::set<int> passwordLoginHistory_;

    // 绑定表：微信内部账号编号对应 QQ 内部账号编号。
    std::map<int, int> weChatToQQ_;

    // 记录各类已分配的最后编号；由账号业务在保存成功后更新。
    int lastPersonId_ = 0;
    int lastAccountId_ = 0;
    int lastSharedNumber_ = 10000;
    int lastWeChatNumber_ = 20000;
};
