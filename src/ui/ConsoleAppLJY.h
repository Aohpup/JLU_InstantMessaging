#pragma once

#include "services/SessionManagerLJY.h"

#include <functional>
#include <istream>
#include <ostream>

// 负责数字菜单和输入输出；所有账号操作交给会话，不直接修改平台容器。
class ConsoleAppLJY
{
public:
    // 会话与流须比菜单活得更久；保存动作为空时是内存测试模式。
    // 保存动作由主程序提供，只在用户保存或退出时调用，不在菜单中执行 SQL。
    ConsoleAppLJY(SessionManagerLJY& session, std::istream& input, std::ostream& output,
                  std::function<void()> saveAction = {}, const std::string& databasePath = "");

    // 运行菜单；退出或输入结束时保存并注销；正常结束返回 0，输入结束且保存失败返回 1。
    int run();

private:
    SessionManagerLJY& session_;
    std::istream& input_;
    std::ostream& output_;
    bool stopped_ = false;
    std::function<void()> saveAction_;
    std::string databasePath_;
    // 调用保存动作并提示位置，失败返回 false，菜单可继续操作。
    bool saveData();
    // 保存后退出；失败可重试、返回菜单或明确放弃。返回 -1 表示继续菜单。
    int finish();

    // 操作结束后等待任意功能键或字符键，不必回车；脚本输入及退出时跳过，忽略 Ctrl+C，Ctrl+Z 退出。
    void pressAnyKeyToContinue();

    // 普通控制台输入采用系统行编辑；Ctrl+C 中断后继续读取，脚本仍使用 getline。
    bool readInputLine(std::string& text);

    // 整行读取；输入 0 取消当前操作，输入结束时请求退出，密码在控制台中用星号占位。
    bool readLine(const char* prompt, std::string& text, bool secret = false);

    // 循环读取并立即校验生日；错误时只重输生日，输入 0 返回，输入结束时退出。
    bool readBirthDate(std::string& text);

    // 逐键读取控制台密码并输出星号；支持退格，忽略 Ctrl+C，Ctrl+Z 结束输入。
    bool readMaskedPassword(std::string& text);

    // 读取 0 至最大值之间的整数；拒绝溢出、小数和尾随字符并重新提示。
    int readChoice(const char* prompt, int maximum);

    // 选择 QQ、微信或微博；选择 0 返回 false。
    bool chooseService(ServiceTypeLJY& type);

    // 返回面向用户的中文服务名称。
    static const char* serviceName(ServiceTypeLJY type);

    // 显示当前身份对应的菜单。
    void showMenu();

    // 收集注册信息；用 while 循环重输无效密码，保留已填资料，确认后才提交。
    void registerAccount();

    // 收集服务、对外号码和密码，完成首次登录。
    void login();

    // 显示本人资料、当前账号和注册年龄。
    void showProfile();

    // 显示三种服务的开通、确认状态及微信绑定信息。
    void showServices();

    // 为本人开通其他服务，开通后展示号码和待确认状态。
    void openService();

    // 未登录的已开通服务先确认；取消不改变原有状态。
    void switchService();

    // 收集并提交本人资料修改，输入中途返回不会保存。
    void updateProfile();

    // 当前服务的好友子菜单；每项结束后暂停，返回时直接回到主菜单。
    void friendMenu();

    // 列出当前服务好友及本人备注。
    void listFriends();

    // 按当前服务号码查询已添加好友的资料。
    void showFriend();

    // 展示目标账号并要求确认后添加好友。
    void addFriend();
    // 查看本人收发的好友申请，收件人可同意或拒绝。
    void friendRequests();

    // 输入并修改本人备注，留空清除。
    void updateFriendRemark();

    // 确认后删除双向关系，取消不修改数据。
    void removeFriend();


    // 输入同服务账号号码，展示双方共同好友的号码与用户昵称。
    void showCommonFriends();
    // 选择本人的源服务，展示与当前服务的共同好友及未入选原因。
    void showCrossServiceCommonFriends();
    // 展示跨服务推荐，按序号选择并二次确认后添加；取消或输入结束不提交。
    void recommendFriends();
    // 统一展示跨服务查询的账号序号和未入选说明，不展示他人的备注。
    void showFriendQueryResult(const FriendQueryResultLJY& result);

    // 先选有效群，再展示该群操作菜单；退群成功后重新选群。
    void groupMenu();
    // 展示当前服务本人群号、群名和管理模式。
    void listGroups();
    // 展示已选群的群主、成员角色及账号号码，供查询和选取目标。
    void showGroup(int number);

    // 展示申请、邀请的编号、来源、目标及当前处理状态。
    void showGroupRequests(int number);
    // 操作已选群，权限和目标在确认前检查；成功退群返回 true 以重新选群。
    bool groupAction(int choice, int number);

    // 先展示父群成员，再选择子群群主和成员，确认后一次创建。
    void createSubgroup(int parentNumber);
    // 展示当前仍存在的子群号码和名称。
    void showSubgroups(int parentNumber);
    // 选择并确认新管理模式，保留已有数据。
    void switchGroupMode(int number);

    // 修改当前服务的用户昵称。
    void updateNickname();
};
