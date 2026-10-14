#pragma once

#include "domain/DateLJY.h"
#include "services/PlatformLJY.h"

// 账号业务入口。平台必须比本对象活得更久，不在此类中复制平台数据。
// 输入或账号归属校验失败时抛出 std::invalid_argument。
class AccountServiceLJY
{
public:
    // 构造时固定业务参考日期；留空取本机当天，测试可传入指定日期。
    explicit AccountServiceLJY(PlatformLJY& platform, const std::string& referenceDate = "");

    // 供底层测试创建无密码人员；正式菜单使用 registerAccount 注册，返回内部人员编号。
    int createPerson(const std::string& name, const std::string& birthDate,
                     const std::string& location);
    // 同时注册人员、密码和首个服务；失败回滚，返回内部账号编号。
    int registerAccount(const std::string& name, const std::string& birthDate,
                        const std::string& location, const std::string& password,
                        ServiceTypeLJY type, const std::string& nickname);

    // 验证服务号码和人员密码，成功返回人员编号；失败统一抛出验证错误。
    int authenticate(ServiceTypeLJY type, const std::string& externalId,
                     const std::string& password) const;

    // 检查生日格式、真实日期和上限；人员编号为 0 表示注册，否则同时检查账号申请日期。
    void validateBirthDate(const std::string& birthDate, int personId = 0) const;

    // 头像色 0—5，省略或 -1 保留原色；修改本人资料；新生日不得晚于已开通账号的申请日期。
    void updateProfile(int actorPersonId, const std::string& name,
                       const std::string& birthDate, const std::string& location, int avatarColor = -1);
    // 开通服务并返回内部账号编号；QQ／微博共享本人号码，微信独立发号。
    // 申请日期留空取业务参考日期；拒绝重复开通、未来日期及早于生日的日期。
    int openService(int actorPersonId, ServiceTypeLJY type, const std::string& nickname,
                    const std::string& appliedOn = "");
    // 修改本人账号的昵称；拒绝空白昵称和操作他人账号。
    void updateNickname(int actorPersonId, int accountId, const std::string& nickname);
    // 按微信、QQ 的顺序传入内部账号编号；仅允许本人账号一对一绑定。
    void bindWeChatToQQ(int actorPersonId, int weChatAccountId, int qqAccountId);
    // 验证本人原密码并替换凭据；各服务共用，校验失败不改变原密码。
    void changePassword(int actorPersonId, const std::string& oldPassword, const std::string& newPassword);
    // 仅解除本人微信与 QQ 的映射，好友、群及服务账号保持不变。
    void unbindWeChatFromQQ(int actorPersonId, int weChatAccountId);
    // 按申请日期到业务参考日期的完整周年计算注册年龄。
    int registrationAge(int accountId) const;

    // actorPersonId 是可信调用方上下文，并非密码或登录凭据。
    // 菜单统一通过 SessionManagerLJY 调用本层，由会话提供身份，不能采信用户输入的编号。

private:
    PlatformLJY& platform_;
    DateLJY referenceDate_;
    // 拒绝空字符串和全空白文本，field 用于标明出错字段。
    static void requireText(const std::string& text, const std::string& field);
    // 生成下一个编号；达到 int 上限时抛出 std::overflow_error。
    static int nextNumber(int lastNumber);
    // 检查姓名、所在地非空白，且生日合法并不晚于业务参考日期。
    void validateProfile(const std::string& name, const std::string& birthDate,
                         const std::string& location) const;
    // 取得指定人员；人员不存在时抛出异常。
    const PersonLJY& requirePerson(int personId) const;
    // 取得本人账号；人员或账号不存在、账号不属于本人时抛出异常。
    const ServiceAccountLJY& requireOwnedAccount(int personId, int accountId) const;
};
