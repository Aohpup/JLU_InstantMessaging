#pragma once

#include "services/AccountServiceLJY.h"
#include "services/FriendManagerLJY.h"
#include "services/GroupManagerLJY.h"
#include "services/ServiceLoginStateLJY.h"

#include <set>

// 保存当前人员和各服务的登录状态；菜单通过本类操作本人数据。
class SessionManagerLJY
{
public:
    // 平台须比会话活得更久；参考日期留空时使用本机当天。
    explicit SessionManagerLJY(PlatformLJY& platform, const std::string& referenceDate = "");

    // 注册人员、密码和首个服务，返回账号副本供展示号码；成功后仍需登录。
    ServiceAccountLJY registerAccount(const std::string& name, const std::string& birthDate,
                        const std::string& location, const std::string& password,
                        ServiceTypeLJY type, const std::string& nickname);

    // 用服务类型、对外号码和密码登录；失败不建立会话，已登录时须先注销。
    void login(ServiceTypeLJY type, const std::string& externalId, const std::string& password);

    // 确认进入其他已开通服务，拒绝重复进入当前服务；取消返回 false 且不改变状态。
    bool enterService(ServiceTypeLJY type, bool confirmed);

    // 清空当前身份和所有服务登录状态，可重复调用。
    void logout() noexcept;

    // 判断是否已完成首次身份验证。
    bool isLoggedIn() const noexcept;

    // 查询当前人员的服务状态；未登录或服务类型无效时抛出异常。
    ServiceLoginStateLJY serviceState(ServiceTypeLJY type) const;

    // 返回本人资料副本；未登录时拒绝查询。
    PersonLJY currentPerson() const;

    // 返回当前已登录服务的账号副本。
    ServiceAccountLJY currentAccount() const;

    // 返回本人全部已开通账号的副本，不允许通过返回值修改平台。
    std::vector<ServiceAccountLJY> accounts() const;

    // 为当前人员开通其他服务；新账号等待确认，不自动切换或登录。
    int openService(ServiceTypeLJY type, const std::string& nickname);

    // 仅校验生日，不修改数据；匿名时用于注册，登录后同时检查本人的账号申请日期。
    void validateBirthDate(const std::string& birthDate) const;

    // 头像色 0—5，省略或 -1 保留原色；修改本人资料，操作人员编号始终来自已验证会话。
    void updateProfile(const std::string& name, const std::string& birthDate,
                       const std::string& location, int avatarColor = -1);

    // 只修改当前已登录服务的昵称，不接受外部传入的人员或账号编号。
    void updateNickname(const std::string& nickname);

    // 先提示已有绑定；首次绑定须两个服务当前均已登录，或均曾用密码成功登录。
    void bindWeChatToQQ();

    // 修改本人共用密码，保留当前会话；下次登录使用新密码。
    void changePassword(const std::string& oldPassword, const std::string& newPassword);
    // 解除本人的 QQ/微信关联，不能指定他人的账号。
    void unbindWeChatFromQQ();

    // 查询本人微信绑定的 QQ 对外号码；未绑定时返回空字符串。
    std::string boundQQNumber() const;

    // 查询当前服务的目标账号副本，用于添加前确认号码与昵称；必须先登录。
    ServiceAccountLJY contactAccount(const std::string& externalId) const;
    // 申请记录使用内部编号，查询仍限制为当前服务。
    ServiceAccountLJY contactAccount(int accountId) const;

    // 添加前查询并检查目标，拒绝自己和已有好友，不修改关系。
    ServiceAccountLJY friendCandidate(const std::string& externalId) const;

    // 直接建立关系的兼容入口，供原业务演示使用；用户菜单和 GUI 使用 sendFriendRequest。
    void addFriend(const std::string& externalId);
    // 用户添加走申请流程；接收者同意才成为好友，号码限定在当前服务。
    void sendFriendRequest(const std::string& externalId);
    void handleFriendRequest(const std::string& senderNumber, bool accept);
    std::vector<FriendRequestLJY> friendRequests() const;
    // 界面只负责每批展示三个，会话返回稳定顺序的同城候选。
    std::vector<ServiceAccountLJY> cityCandidates() const;

    // 删除当前服务的双向好友关系及双方备注。
    void removeFriend(const std::string& externalId);

    // 修改自己对当前服务好友的备注，空字符串表示清除。
    void updateFriendRemark(const std::string& externalId, const std::string& remark);

    // 返回当前服务的全部好友账号副本。
    std::vector<ServiceAccountLJY> friends() const;

    // 查询当前服务中已有好友的账号资料副本。
    ServiceAccountLJY friendAccount(const std::string& externalId) const;

    // 查询当前服务中已有好友的人员资料副本。
    PersonLJY friendPerson(const std::string& externalId) const;

    // 仅返回本人对该好友的备注。
    std::string friendRemark(const std::string& externalId) const;


    // 查询当前账号和指定同服务账号的共同好友；号码按当前服务解释。
    std::vector<ServiceAccountLJY> commonFriends(const std::string& externalId) const;
    // 比较本人两项服务的好友，源服务只需已开通，当前目标服务必须已登录。
    FriendQueryResultLJY crossServiceCommonFriends(ServiceTypeLJY sourceType) const;
    // 返回可推荐到当前服务的好友及未入选原因，涉及微信须有有效绑定。
    FriendQueryResultLJY friendRecommendations(ServiceTypeLJY sourceType) const;
    // 确认后只校验选中对象的源好友关系和映射，再添加；执行者取自当前会话。
    void addRecommendedFriend(ServiceTypeLJY sourceType, const std::string& externalId);

    // 返回当前服务中本人参加的群，指针由平台管理。
    std::vector<const GroupLJY*> groups() const;
    // 选群时立即验证当前服务中群号存在；非成员也可选群以申请或接受邀请。
    const GroupLJY& selectedGroup(int number) const;
    // 输入阶段检查群操作，不修改数据；目标号码为空时只检查执行者权限。
    void checkGroupOperation(int number, GroupOperationLJY operation, const std::string& targetNumber = "") const;
    // 输入请求编号后检查是否属于当前群，以及本人是否有处理权限。
    void checkGroupRequest(int number, int requestId) const;
    // 查询本人群资料，同时填入成员账号副本；非群成员拒绝查询。
    const GroupLJY& group(int number, std::vector<ServiceAccountLJY>& memberAccounts) const;

    // 当前服务申请入群或邀请他人，返回待处理请求编号。
    int applyToGroup(int number);
    int inviteToGroup(int number, const std::string& targetNumber);
    // 查询本人相关或可审批的记录；指定群号时只返回该群记录，0 表示当前服务全部。
    std::vector<GroupRequestLJY> groupRequests(int number = 0) const;
    void handleGroupRequest(int requestId, bool accept);
    // 退出本人群，或按权限移除当前服务的指定成员。
    void leaveGroup(int number);
    void kickGroupMember(int number, const std::string& targetNumber);
    // 群主任免管理员、转让群主；目标号码始终按当前服务解释。
    void setGroupAdministrator(int number, const std::string& targetNumber, bool enabled);
    void transferGroupOwner(int number, const std::string& targetNumber);

    // 创建当前群的一级子群，号码均按当前服务解析，初始成员不含自动加入的子群主。
    int createSubgroup(int parentNumber, int number, const std::string& name,
        const std::string& ownerNumber, const std::vector<std::string>& memberNumbers);
    // 查看本人父群的子群列表。
    std::vector<const GroupLJY*> subgroups(int parentNumber) const;
    // 群主切换当前群规则；已有子群及请求保留，所属服务和账号号码不变。
    void switchGroupMode(int number, GroupStyleLJY style);

    // 计算当前服务账号的注册年龄。
    int registrationAge() const;

private:
    PlatformLJY& platform_;
    AccountServiceLJY accountService_;
    FriendManagerLJY friendManager_;
    GroupManagerLJY groupManager_;
    int currentPersonId_ = 0;
    int currentAccountId_ = 0;
    std::set<int> loggedInAccounts_;

    // 所有受保护操作先检查当前会话，拒绝匿名或注销后的访问。
    void requireLogin() const;
};
