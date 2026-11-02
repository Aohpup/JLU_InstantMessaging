#pragma once
#include <QWidget>
#include "services/SessionManagerLJY.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QTableWidget;
class QTabWidget;
class QPushButton;
class QComboBox;

// 群页面先选群再操作，只保存群号、账号号码和请求编号，复用原群业务。
class GroupsPageLJY : public QWidget
{
    Q_OBJECT
public:
    explicit GroupsPageLJY(SessionManagerLJY& session, QWidget* parent = nullptr);
    // 提交后保留当前群；注销或切换服务时清空群及目标选择。
    // 后台同步时保留同一群的子群创建草稿；提交时仍检查最新成员和权限。
    void refresh(bool preserveDraft = false);
signals:
    // 由主窗口打开所选群的实时聊天。
    void messageRequested(int number);
    // 操作结果统一交给主窗口固定提示区。
    void feedback(const QString& message, bool failed = false);
    void changed(const QString& message);
private:
    SessionManagerLJY& session_;
    int accountId_ = 0, selectedNumber_ = 0;
    // 选中子群时记住父群号，子群释放后可回到有效父群。
    int parentNumber_ = 0;
    QListWidget* groups_;
    QLineEdit *number_, *inviteNumber_;
    QLabel *identity_, *summary_, *groupHint_, *memberHint_, *requestHint_, *inviteHint_;
    QTableWidget *members_, *requests_;
    QTabWidget* tabs_;
    QWidget* inviteEditor_;
    QComboBox* memberOperation_;
    QPushButton *apply_, *leave_, *invite_, *memberAction_, *accept_, *reject_;
    QListWidget *subgroups_, *subgroupMembers_;
    QLineEdit *subgroupNumber_, *subgroupName_;
    QComboBox *subgroupOwner_, *mode_;
    QPushButton *createSubgroup_, *openSubgroup_, *parentGroup_, *switchMode_;
    QLabel *subgroupHint_;
    QWidget *managementView_, *subgroupEditor_;
    // 刷新子群列表及模式；创建表单只使用当前父群成员，不引入业务副本。
    void showManagement();
    void beginSubgroup();
    void updateSubgroupOwner();
    void saveSubgroup();
    void switchMode();
    void openSubgroup();
    // 查找失败立即清空旧群，不能在无效群上继续选人操作。
    void findGroup();
    void selectGroup(int number);
    void showGroup(bool preserveDraft = false);
    GroupOperationLJY memberOperation() const;
    QString selectedMember() const;
    // 换选目标只清除旧反馈；点击操作后检查权限，内部刷新保留成功提示。
    void updateMemberAction(bool clearFeedback = false);
    void updateRequestActions(bool clearFeedback = false);
    // 修改前确认；提交仍由会话取得本人身份并执行原业务检查。
    void changeMembership(bool apply);
    void inviteMember();
    void operateMember();
    void handleRequest(bool accept);
    bool confirm(const QString& message, const QString& title = "确认群操作");
};
