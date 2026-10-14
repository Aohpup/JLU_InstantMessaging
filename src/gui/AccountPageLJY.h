#pragma once

#include <QWidget>
#include "services/SessionManagerLJY.h"

class QLineEdit;
class QLabel;
class QComboBox;
class QTableWidget;
class QPushButton;

// 本人账号三块卡片；资料、昵称、密码均在页面内编辑，不使用输入弹窗。
class AccountPageLJY : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPageLJY(SessionManagerLJY& session, QWidget* parent = nullptr);
    // 登录、切换服务或提交后刷新真实数据，并收起编辑区。
    void refresh();
    // 离开账号页或注销时丢弃未提交输入并清空密码。
    void cancelEditing();

signals:
    // 将操作结果发送到主窗口固定提示区，不随页面滚动。
    void feedback(const QString& message, bool failed = false);
    void changed(const QString& message);

private:
    SessionManagerLJY& session_;
    QLineEdit *name_, *birth_, *location_, *nickname_, *newNickname_;
    QLineEdit *oldPassword_, *newPassword_, *confirmPassword_;
    QComboBox* newService_;
    QTableWidget* services_;
    QLabel *identity_, *avatar_, *nameText_, *birthText_, *locationText_, *nicknameText_;
    QLabel* binding_;
    QPushButton *profileButton_, *profileCancel_, *nicknameButton_, *nicknameCancel_;
    QPushButton *passwordButton_, *passwordCancel_, *bindButton_, *unbindButton_;
    QWidget *passwordEditor_, *unbindConfirmation_, *avatarColors_;
    QPushButton* colorButtons_[6];
    int selectedColor_ = 0;
    // 只预览草稿配色；保存资料时一并提交，取消还原。
    void previewAvatarColor(int color);
    bool editingProfile_ = false, editingNickname_ = false, editingPassword_ = false;

    // 每次只展开一种编辑，并隐藏另一编辑入口；取消恢复会话中的值。
    void setProfileEditing(bool editing);
    void setNicknameEditing(bool editing);
    void setPasswordEditing(bool editing);
    // 按钮首次点击展开，再次点击校验提交；失败保留表单供修改。
    void updateProfile();
    void updateNickname();
    void updatePassword();
    // QQ 行独立显示绑定操作；解绑使用卡片内的确认区。
    void openService();
    void bindAccounts();
    void unbindAccounts();
};
