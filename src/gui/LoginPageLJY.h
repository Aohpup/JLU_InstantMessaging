#pragma once

#include <QWidget>
#include "services/SessionManagerLJY.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QTabWidget;

// 登录和注册卡片：左侧说明、右侧限宽表单；业务校验交给会话，错误时保留输入。
class LoginPageLJY : public QWidget
{
    Q_OBJECT
public:
    explicit LoginPageLJY(SessionManagerLJY& session, QWidget* parent = nullptr);
    // 注销后清空密码和注册资料，回到登录页。
    void reset();

signals:
    // 将操作结果发送到主窗口固定提示区，不随页面滚动。
    void feedback(const QString& message, bool failed = false);
    // 验证成功后由主窗口切换页面。
    void loggedIn();
    // 注册只创建账号，仍需登录；通知主窗口有未保存修改。
    void registered(const QString& message);

protected:
    // 根据可用宽度适度放大文案、间距和表单，最大约 1.5 倍。
    void resizeEvent(QResizeEvent* event) override;

private:
    SessionManagerLJY& session_;
    QTabWidget* tabs_;
    QComboBox *loginService_, *registerService_;
    QLineEdit *number_, *password_, *name_, *birth_, *location_, *nickname_, *newPassword_, *confirmPassword_;
    // 从表单读取 UTF-8 文本，调用原账号接口并发送底部提示。
    void login();
    // 两次密码一致才注册；成功后在固定提示区告知服务与号码，并自动填入登录框。
    void registerAccount();
};
