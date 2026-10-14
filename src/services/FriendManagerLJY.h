#pragma once

#include "services/PlatformLJY.h"
#include "domain/FriendQueryResultLJY.h"

// 好友关系只连接同服务账号；跨服务查询先验证映射，执行者身份由可信会话传入。
class FriendManagerLJY
{
public:
    // 平台拥有关系数据，生命周期须长于本对象。
    explicit FriendManagerLJY(PlatformLJY& platform);
    // 确认后添加双向好友；拒绝自身、跨服务、不存在及重复关系。
    void addFriend(int actorPersonId, int actorAccountId, int targetAccountId);
    // 删除一条关系，同时清除双方好友资格及各自备注。
    void removeFriend(int actorPersonId, int actorAccountId, int targetAccountId);
    // 只修改本人的备注；空字符串清除，非空备注最多 120 字节且不能全空白或含控制字符。
    void updateRemark(int actorPersonId, int actorAccountId, int targetAccountId,
                      const std::string& remark);
    // 查询本账号的好友账号副本，避免向菜单暴露可修改容器。
    std::vector<ServiceAccountLJY> friends(int actorPersonId, int actorAccountId) const;
    // 检查双方账号及好友关系，返回只读关系；删除关系后引用失效。
    const FriendRelationLJY& friendRelation(int personId, int accountId, int targetId) const;

    // 查询本人和同服务另一账号的共同好友；双方无需事先互为好友。
    std::vector<ServiceAccountLJY> commonFriends(int personId, int accountId, int otherAccountId) const;
    // 比较本人源服务与当前目标服务的好友，按有效映射取交集并去重。
    FriendQueryResultLJY crossServiceCommonFriends(int personId, int targetAccountId,
                                                  ServiceTypeLJY sourceType) const;
    // 从本人的另一服务推荐好友，排除未开通、未绑定、本人和已有好友。
    FriendQueryResultLJY friendRecommendations(int personId, int targetAccountId,
                                              ServiceTypeLJY sourceType) const;
    // 只检查选中对象的源好友关系和映射，再建立双向关系，不重新生成推荐列表。
    void addRecommendedFriend(int personId, int targetAccountId, ServiceTypeLJY sourceType,
                              int recommendedAccountId);

    // 发送申请不建立好友关系；只有收件人同意后才建立双向关系。
    void sendRequest(int personId, int accountId, int targetId);
    void handleRequest(int personId, int accountId, int senderId, bool accept);
    // 只返回本人参与的申请，状态用于界面显示收到和发出的记录。
    std::vector<FriendRequestLJY> requests(int personId, int accountId) const;
    // 同服务、所在地相同；排除本人、已有好友和双向待处理申请，按账号顺序返回。
    std::vector<ServiceAccountLJY> cityCandidates(int personId, int accountId) const;
private:

    PlatformLJY& platform_;
    // 集中解析跨服务映射；QQ/微博核对共享号码，涉及微信时核对绑定及账号归属。
    const ServiceAccountLJY* mappedAccount(const ServiceAccountLJY& source,
                                         ServiceTypeLJY targetType, std::string& reason) const;
    // 两类跨服务查询共用映射和过滤；commonOnly 为真取交集，否则返回可推荐对象。
    FriendQueryResultLJY queryAcrossServices(int personId, int targetAccountId,
                                            ServiceTypeLJY sourceType, bool commonOnly) const;
    // 验证执行者存在且拥有所操作的服务账号。
    const ServiceAccountLJY& requireActor(int personId, int accountId) const;
    // 验证目标账号存在、与执行者不同且服务相同。
    const ServiceAccountLJY& requireTarget(int personId, int accountId, int targetId) const;
    // 双方已校验后保存一条关系；普通添加与推荐添加共用，重复关系在此拒绝。
    void createRelation(int accountId, int targetId);
    // 检查本人源服务已开通且能与目标账号关联，供跨服务查询和添加共用。
    const ServiceAccountLJY& requireSource(const ServiceAccountLJY& target, ServiceTypeLJY sourceType) const;
};
