#include "gui/CardSurfaceLJY.h"
#include "gui/LoginPageLJY.h"
#include "domain/PasswordCredentialLJY.h"
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <algorithm>

LoginPageLJY::LoginPageLJY(SessionManagerLJY& session, QWidget* parent)
    : QWidget(parent), session_(session)
{
    setObjectName("loginSurface");
    setAttribute(Qt::WA_StyledBackground);
    // 左侧说明、右侧表单；限宽卡片避免输入框随窗口无限拉长。
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 20, 20, 24);
    outer->setSpacing(20);
    auto* welcome = new QWidget;
    welcome->setObjectName("welcomePanel");
    welcome->setMinimumWidth(180);
    welcome->setMaximumWidth(480);
    auto* intro = new QVBoxLayout(welcome);
    intro->setContentsMargins(0, 0, 0, 0);
    intro->setSpacing(20);
    intro->addStretch();
    auto* eyebrow = new QLabel("JLU  ·  即时通信");
    eyebrow->setObjectName("welcomeEyebrow");
    intro->addWidget(eyebrow);
    auto* headline = new QLabel("三个服务，\n一个入口。");
    headline->setObjectName("welcomeHeadline");
    headline->setWordWrap(true);
    intro->addWidget(headline);
    auto* description = new QLabel("把账号、好友和群关系，\n整理在同一个空间里。");
    description->setObjectName("welcomeDescription");
    description->setWordWrap(true);
    intro->addWidget(description);
    auto* services = new QLabel("QQ   /   微信   /   微博");
    services->setObjectName("serviceBadge");
    intro->addWidget(services);
    intro->addSpacing(12);
    auto* note = new QLabel("从一个账号开始\n注册后可开通其他服务");
    note->setProperty("muted", true);
    note->setObjectName("welcomeNote");
    intro->addWidget(note);
    intro->addStretch();
    outer->addWidget(welcome, 1);

    auto* card = new QFrame;
    card->setObjectName("loginCard");
    applyCardSurfaceLJY(card);
    card->setMinimumWidth(370);
    card->setMaximumWidth(490);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(26, 22, 26, 20);
    cardLayout->setSpacing(8);
    auto* title = new QLabel("开始连接");
    title->setObjectName("pageTitle");
    cardLayout->addWidget(title);
    auto* hint = new QLabel("登录已有账号，或创建你的第一个账号。");
    hint->setProperty("muted", true);
    cardLayout->addWidget(hint);
    cardLayout->addSpacing(12);
    tabs_ = new QTabWidget;
    tabs_->setObjectName("loginTabs");
    cardLayout->addWidget(tabs_, 1);
    outer->addWidget(card, 2, Qt::AlignVCenter);
    auto* loginPage = new QWidget;
    auto* form = new QFormLayout(loginPage);
    form->setContentsMargins(0, 22, 0, 0);
    form->setVerticalSpacing(14);
    form->setFormAlignment(Qt::AlignTop);
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    loginService_ = new QComboBox;
    loginService_->setObjectName("loginService");
    loginService_->addItems({"QQ", "微信", "微博"});
    number_ = new QLineEdit;
    number_->setObjectName("loginNumber");
    number_->setPlaceholderText("输入注册时获得的账号号码");
    password_ = new QLineEdit;
    password_->setObjectName("loginPassword");
    password_->setPlaceholderText("输入账号密码");
    password_->setEchoMode(QLineEdit::Password);
    form->addRow("服务", loginService_);
    form->addRow("账号号码", number_);
    form->addRow("密码", password_);
    auto* loginButton = new QPushButton("登录");
    loginButton->setObjectName("loginButton");
    loginButton->setProperty("primary", true);
    form->addRow(loginButton);
    tabs_->addTab(loginPage, "登录");

    auto* registerPage = new QWidget;
    auto* registration = new QFormLayout(registerPage);
    registration->setContentsMargins(0, 18, 0, 0);
    registration->setVerticalSpacing(9);
    registration->setHorizontalSpacing(12);
    registration->setFormAlignment(Qt::AlignTop);
    registerService_ = new QComboBox;
    registerService_->setObjectName("registerService");
    registerService_->addItems({"QQ", "微信", "微博"});
    name_ = new QLineEdit;
    name_->setObjectName("registerName");
    birth_ = new QLineEdit;
    birth_->setObjectName("registerBirth");
    birth_->setPlaceholderText("YYYY-MM-DD，例如 2005-03-12");
    location_ = new QLineEdit;
    location_->setObjectName("registerLocation");
    nickname_ = new QLineEdit;
    nickname_->setObjectName("registerNickname");
    newPassword_ = new QLineEdit;
    newPassword_->setObjectName("registerPassword");
    newPassword_->setEchoMode(QLineEdit::Password);
    newPassword_->setPlaceholderText("8—20 字节，隐藏显示");
    confirmPassword_ = new QLineEdit;
    confirmPassword_->setObjectName("registerConfirmPassword");
    confirmPassword_->setEchoMode(QLineEdit::Password);
    confirmPassword_->setPlaceholderText("再次输入密码，隐藏显示");
    registration->addRow("首项服务", registerService_);
    registration->addRow("姓名", name_);
    registration->addRow("生日", birth_);
    registration->addRow("所在地", location_);
    registration->addRow("用户昵称", nickname_);
    registration->addRow("设置密码", newPassword_);
    auto* rules = new QLabel("密码为 8—20 字节，仅允许 ASCII 可见字符（33—126），不含空格、中文及控制字符。");
    rules->setWordWrap(true);
    rules->setProperty("muted", true);
    registration->addRow(rules);
    registration->addRow("确认密码", confirmPassword_);
    auto* registerButton = new QPushButton("注册并开通");
    registerButton->setObjectName("registerButton");
    registerButton->setProperty("primary", true);
    registration->addRow(registerButton);
    tabs_->addTab(registerPage, "注册");
    connect(loginButton, &QPushButton::clicked, this, &LoginPageLJY::login);
    connect(password_, &QLineEdit::returnPressed, this, &LoginPageLJY::login);
    connect(registerButton, &QPushButton::clicked, this, &LoginPageLJY::registerAccount);
}

void LoginPageLJY::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const double scale = std::clamp(width() / 820.0, 1.0, 1.5);
    auto* welcome = findChild<QWidget*>("welcomePanel");
    auto* intro = qobject_cast<QVBoxLayout*>(welcome->layout());
    intro->setSpacing(int(20 * scale));
    welcome->setMaximumWidth(int(320 * scale));
    auto fontSize = [this, scale](const char* name, int pixels) {
        findChild<QLabel*>(name)->setStyleSheet(QString("font-size: %1px;").arg(int(pixels * scale)));
    };
    fontSize("welcomeEyebrow",13); fontSize("welcomeHeadline",29);
    fontSize("welcomeDescription",14); fontSize("welcomeNote",12);
    findChild<QLabel*>("serviceBadge")->setStyleSheet(QString("font-size: %1px; padding: %2px %3px;")
        .arg(int(13 * scale)).arg(int(15 * scale)).arg(int(10 * scale)));
    auto* card = findChild<QFrame*>("loginCard");
    card->setMaximumWidth(int(490 * scale));
    card->setMaximumHeight(int(560 * std::min(scale, 1.25)));
    const int font = int(13 * std::min(scale, 1.3));
    card->setStyleSheet(QString("QFrame#loginCard QWidget { font-family: \"Microsoft YaHei\"; font-size: %1px; font-weight: 400; } QLabel#pageTitle { font-size: %2px; font-weight: bold; }")
        .arg(font).arg(int(24 * scale)));
    card->findChild<QLabel*>("pageTitle")->setStyleSheet(QString("font-size: %1px; font-weight: bold;").arg(int(24 * scale)));
    const int margin = int(22 * scale);
    card->layout()->setContentsMargins(margin, margin, margin, margin);
    auto* outer = qobject_cast<QHBoxLayout*>(layout());
    outer->setSpacing(int(20 * scale));
}

void LoginPageLJY::login()
{
    try
    {
        session_.login(static_cast<ServiceTypeLJY>(loginService_->currentIndex()),
            number_->text().toUtf8().toStdString(), password_->text().toUtf8().toStdString());
        password_->clear();
        emit loggedIn();
    }
    catch (const std::exception& error)
    {
        emit feedback( "登录失败：" + QString::fromUtf8(error.what()), true);
        password_->setFocus();
    }
}

void LoginPageLJY::registerAccount()
{
    try
    {
        // 单独定位生日和密码错误，保留其他已填写字段。
        try { session_.validateBirthDate(birth_->text().toUtf8().toStdString()); }
        catch (...) { birth_->setFocus(); throw; }
        try { PasswordCredentialLJY::validatePassword(newPassword_->text().toUtf8().toStdString()); }
        catch (...) { newPassword_->setFocus(); throw; }
        if (newPassword_->text() != confirmPassword_->text())
        {
            emit feedback( "两次密码不一致，请重新填写两次密码，其他资料已保留。", true);
            newPassword_->clear();
            confirmPassword_->clear();
            newPassword_->setFocus();
            return;
        }
        const auto account = session_.registerAccount(name_->text().toUtf8().toStdString(),
            birth_->text().toUtf8().toStdString(), location_->text().toUtf8().toStdString(),
            newPassword_->text().toUtf8().toStdString(),
            static_cast<ServiceTypeLJY>(registerService_->currentIndex()), nickname_->text().toUtf8().toStdString());
        loginService_->setCurrentIndex(registerService_->currentIndex());
        number_->setText(QString::fromStdString(account.externalId()));
        newPassword_->clear();
        confirmPassword_->clear();
        tabs_->setCurrentIndex(0);
        password_->setFocus();
        emit registered("注册成功，" + loginService_->currentText() + " 账号号码：" + number_->text() +
            "。账号已填入登录框，请输入密码登录。");
    }
    catch (const std::exception& error)
    {
        emit feedback( "注册失败：" + QString::fromUtf8(error.what()), true);
    }
}

void LoginPageLJY::reset()
{
    for (auto* edit : {password_, name_, birth_, location_, nickname_, newPassword_, confirmPassword_}) edit->clear();
    tabs_->setCurrentIndex(0);
}
