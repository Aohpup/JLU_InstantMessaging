#include "gui/CardSurfaceLJY.h"
#include "gui/AccountPageLJY.h"
#include "gui/AvatarLJY.h"
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QIcon>
#include <QTableWidget>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>

AccountPageLJY::AccountPageLJY(SessionManagerLJY& session, QWidget* parent)
    : QWidget(parent), session_(session)
{
    setObjectName("accountPage");
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QVBoxLayout(this);
    // 页面边缘给阴影留白，避免滚动区域裁掉卡片投影。
    layout->setContentsMargins(12, 4, 12, 16);
    layout->setSpacing(16);
    auto* title = new QLabel("我的账号");
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    identity_ = new QLabel;
    identity_->setObjectName("accountIdentity");
    layout->addWidget(identity_);

    // 三块卡片使用同一标题样式，只复用布局，不增加控件类层次。
    auto card = [layout](const QString& title, const char* objectName) {
        auto* frame = new QFrame;
        frame->setObjectName(objectName);
        frame->setProperty("accountCard", true);
        applyCardSurfaceLJY(frame);
        auto* box = new QVBoxLayout(frame);
        box->setContentsMargins(16, 12, 16, 14);
        box->setSpacing(12);
        auto* heading = new QLabel(title);
        heading->setProperty("cardHeading", true);
        box->addWidget(heading);
        layout->addWidget(frame);
        return box;
    };
    auto* basic = card("基本信息", "basicCard");
    auto* details = new QHBoxLayout;
    details->setSpacing(20);
    avatar_ = new QLabel;
    avatar_->setObjectName("accountAvatar");
    avatar_->setFixedSize(76, 76);
    avatar_->setAlignment(Qt::AlignCenter);
    avatar_->setTextFormat(Qt::PlainText);
    auto* avatarBox = new QVBoxLayout;
    avatarBox->setSpacing(0);
    avatarBox->addWidget(avatar_, 0, Qt::AlignHCenter);
    avatarColors_ = new QWidget;
    avatarColors_->setObjectName("avatarColors");
    avatarColors_->setFixedSize(144, 52);
    const QStringList colorNames{"苔绿", "雾蓝", "珊瑚", "浅紫", "麦金", "湖青"};
    // 六个色点沿浅弧排列；勾选和描边共同指示当前颜色。
    const int heights[] = {4, 15, 21, 21, 15, 4};
    for (int i = 0; i < 6; ++i) {
        auto* button = new QPushButton(avatarColors_);
        colorButtons_[i] = button;
        button->setObjectName("avatarColor" + QString::number(i));
        button->setGeometry(i * 24, heights[i], 24, 24);
        button->setToolTip(colorNames[i]); button->setAccessibleName(colorNames[i] + "头像");
        button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i] { previewAvatarColor(i); });
    }
    avatarBox->addWidget(avatarColors_, 0, Qt::AlignHCenter);
    details->addLayout(avatarBox);
    auto* fields = new QGridLayout;
    fields->setHorizontalSpacing(12);
    fields->setVerticalSpacing(12);
    fields->setColumnStretch(1, 1);
    fields->setColumnStretch(3, 1);
    // 同一个网格位置放文字和输入框，通过显隐切换保持位置稳定。
    auto field = [fields](const QString& caption, int row, int column,
                         const char* objectName, QLabel*& value, QLineEdit*& edit) {
        fields->addWidget(new QLabel(caption), row, column);
        value = new QLabel;
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setObjectName(QString(objectName) + "Text");
        edit = new QLineEdit;
        edit->setObjectName(objectName);
        fields->addWidget(value, row, column + 1);
        fields->addWidget(edit, row, column + 1);
        edit->hide();
    };
    field("姓名", 0, 0, "profileName", nameText_, name_);
    field("生日", 1, 0, "profileBirth", birthText_, birth_);
    field("用户昵称", 0, 2, "profileNickname", nicknameText_, nickname_);
    field("所在地", 1, 2, "profileLocation", locationText_, location_);
    birth_->setPlaceholderText("YYYY-MM-DD");
    details->addLayout(fields, 1);
    basic->addLayout(details);
    auto* basicActions = new QHBoxLayout;
    basicActions->addStretch();
    nicknameButton_ = new QPushButton("修改当前昵称");
    nicknameButton_->setObjectName("nicknameButton");
    nicknameCancel_ = new QPushButton("取消");
    nicknameCancel_->setObjectName("nicknameCancel");
    profileButton_ = new QPushButton("修改个人资料");
    profileButton_->setObjectName("profileButton");
    profileButton_->setProperty("coral", true);
    profileCancel_ = new QPushButton("取消");
    profileCancel_->setObjectName("profileCancel");
    for (auto* button : {nicknameButton_, nicknameCancel_, profileButton_, profileCancel_})
        basicActions->addWidget(button);
    basic->addLayout(basicActions);

    auto* security = card("安全设置", "securityCard");
    auto* securityRow = new QHBoxLayout;
    auto* explanation = new QLabel("账号密码\n本人各服务共用密码；修改后下次登录使用新密码。");
    explanation->setWordWrap(true);
    securityRow->addWidget(explanation, 1);
    passwordButton_ = new QPushButton("修改密码");
    passwordButton_->setObjectName("passwordButton");
    passwordButton_->setProperty("coral", true);
    passwordCancel_ = new QPushButton("取消");
    passwordCancel_->setObjectName("passwordCancel");
    securityRow->addWidget(passwordButton_);
    securityRow->addWidget(passwordCancel_);
    security->addLayout(securityRow);
    passwordEditor_ = new QWidget;
    passwordEditor_->setObjectName("passwordEditor");
    auto* passwordForm = new QFormLayout(passwordEditor_);
    passwordForm->setContentsMargins(0, 0, 0, 0);
    oldPassword_ = new QLineEdit;
    oldPassword_->setObjectName("oldPassword");
    newPassword_ = new QLineEdit;
    newPassword_->setObjectName("newPassword");
    confirmPassword_ = new QLineEdit;
    confirmPassword_->setObjectName("confirmPassword");
    for (auto* edit : {oldPassword_, newPassword_, confirmPassword_}) edit->setEchoMode(QLineEdit::Password);
    passwordForm->addRow("原密码", oldPassword_);
    passwordForm->addRow("新密码", newPassword_);
    passwordForm->addRow("确认新密码", confirmPassword_);
    auto* rules = new QLabel("8—20 字节，仅允许 ASCII 可见字符（33—126），不含空格、中文及控制字符。");
    rules->setWordWrap(true);
    rules->setProperty("muted", true);
    passwordForm->addRow(rules);
    security->addWidget(passwordEditor_);

    auto* association = card("账号关联", "associationCard");
    services_ = new QTableWidget(3, 6);
    services_->setObjectName("servicesTable");
    services_->setHorizontalHeaderLabels({"服务", "账号号码", "用户昵称", "申请日期", "会话状态", "绑定操作"});
    services_->horizontalHeader()->setMinimumSectionSize(70);
    services_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    services_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    services_->setColumnWidth(5, 180);
    services_->verticalHeader()->hide();
    services_->verticalHeader()->setDefaultSectionSize(44);
    services_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    services_->setSelectionMode(QAbstractItemView::NoSelection);
    services_->setShowGrid(false);
    services_->setFixedHeight(174);
    auto* bindingActions = new QWidget;
    auto* bindingRow = new QHBoxLayout(bindingActions);
    bindingRow->setContentsMargins(4, 4, 4, 4);
    bindingRow->setSpacing(6);
    bindButton_ = new QPushButton("绑定微信");
    bindButton_->setObjectName("bindButton");
    unbindButton_ = new QPushButton("解绑");
    unbindButton_->setObjectName("unbindButton");
    for (auto* button : {bindButton_, unbindButton_}) {
        button->setProperty("compact", true);
        bindingRow->addWidget(button);
    }
    services_->setCellWidget(0, 5, bindingActions);
    services_->setItem(1, 5, new QTableWidgetItem("—"));
    services_->setItem(2, 5, new QTableWidgetItem("—"));
    association->addWidget(services_);
    binding_ = new QLabel;
    binding_->setObjectName("bindingState");
    binding_->setWordWrap(true);
    binding_->setProperty("muted", true);
    association->addWidget(binding_);
    unbindConfirmation_ = new QWidget;
    unbindConfirmation_->setObjectName("unbindConfirmation");
    auto* confirmation = new QHBoxLayout(unbindConfirmation_);
    confirmation->setContentsMargins(0, 0, 0, 0);
    auto* warning = new QLabel("确认解除 QQ 与微信的关联？好友和群保留，微信跨服务映射将失效。");
    warning->setWordWrap(true);
    confirmation->addWidget(warning, 1);
    auto* confirmUnbind = new QPushButton("确认解绑");
    confirmUnbind->setObjectName("confirmUnbind");
    confirmUnbind->setProperty("danger", true);
    auto* cancelUnbind = new QPushButton("取消");
    cancelUnbind->setObjectName("cancelUnbind");
    confirmation->addWidget(confirmUnbind);
    confirmation->addWidget(cancelUnbind);
    association->addWidget(unbindConfirmation_);
    auto* openPanel = new QWidget;
    openPanel->setObjectName("openServicePanel");
    auto* openRow = new QHBoxLayout(openPanel);
    openRow->setContentsMargins(0, 0, 0, 0);
    openRow->addWidget(new QLabel("待开通服务"));
    newService_ = new QComboBox;
    newService_->setObjectName("openServiceType");
    newService_->setPlaceholderText("暂无可开通服务");
    openRow->addWidget(newService_, 1);
    openRow->addWidget(new QLabel("用户昵称"));
    newNickname_ = new QLineEdit;
    newNickname_->setObjectName("openServiceNickname");
    openRow->addWidget(newNickname_, 1);
    auto* openButton = new QPushButton("开通服务");
    openButton->setObjectName("openServiceButton");
    openRow->addWidget(openButton);
    association->addWidget(openPanel);
    auto* hint = new QLabel("开通后，点击顶部服务按钮确认进入；首次绑定需确认 QQ 和微信，或曾分别用密码登录。");
    hint->setObjectName("openServiceHint");
    hint->setWordWrap(true);
    hint->setProperty("muted", true);
    association->addWidget(hint);
    layout->addStretch();
    connect(profileButton_, &QPushButton::clicked, this, &AccountPageLJY::updateProfile);
    connect(nicknameButton_, &QPushButton::clicked, this, &AccountPageLJY::updateNickname);
    connect(passwordButton_, &QPushButton::clicked, this, &AccountPageLJY::updatePassword);
    for (auto* button : {profileCancel_, nicknameCancel_, passwordCancel_})
        connect(button, &QPushButton::clicked, this, [this] {
            cancelEditing();
            emit feedback("已取消修改，资料保持不变。");
        });
    connect(openButton, &QPushButton::clicked, this, &AccountPageLJY::openService);
    connect(bindButton_, &QPushButton::clicked, this, &AccountPageLJY::bindAccounts);
    connect(unbindButton_, &QPushButton::clicked, this, [this] { cancelEditing(); unbindConfirmation_->show(); });
    connect(confirmUnbind, &QPushButton::clicked, this, &AccountPageLJY::unbindAccounts);
    connect(cancelUnbind, &QPushButton::clicked, this, [this] { unbindConfirmation_->hide(); });
    cancelEditing();
}

void AccountPageLJY::setProfileEditing(bool editing)
{
    editingProfile_ = editing;
    avatarColors_->setVisible(editing);
    nicknameButton_->setVisible(!editing);
    for (auto* label : {nameText_, birthText_, locationText_}) label->setVisible(!editing);
    for (auto* edit : {name_, birth_, location_}) edit->setVisible(editing);
    profileButton_->setText(editing ? "保存资料" : "修改个人资料");
    profileCancel_->setVisible(editing);
}

void AccountPageLJY::setNicknameEditing(bool editing)
{
    editingNickname_ = editing;
    profileButton_->setVisible(!editing);
    nicknameText_->setVisible(!editing);
    nickname_->setVisible(editing);
    nicknameButton_->setText(editing ? "保存昵称" : "修改当前昵称");
    nicknameCancel_->setVisible(editing);
}

void AccountPageLJY::setPasswordEditing(bool editing)
{
    editingPassword_ = editing;
    passwordEditor_->setVisible(editing);
    passwordCancel_->setVisible(editing);
    passwordButton_->setText(editing ? "保存密码" : "修改密码");
    for (auto* edit : {oldPassword_, newPassword_, confirmPassword_}) edit->clear();
}

void AccountPageLJY::cancelEditing()
{
    if (session_.isLoggedIn()) previewAvatarColor(session_.currentPerson().avatarColor());
    name_->setText(nameText_->text());
    birth_->setText(birthText_->text());
    location_->setText(locationText_->text());
    nickname_->setText(nicknameText_->text());
    setProfileEditing(false);
    setNicknameEditing(false);
    setPasswordEditing(false);
    unbindConfirmation_->hide();
}

void AccountPageLJY::refresh()
{
    const auto person = session_.currentPerson();
    const auto current = session_.currentAccount();
    applyAvatarColorLJY(avatar_, person.personId(), person.avatarColor());
    nameText_->setText(QString::fromStdString(person.name()));
    birthText_->setText(QString::fromStdString(person.birthDate()));
    locationText_->setText(QString::fromStdString(person.location()));
    nicknameText_->setText(QString::fromStdString(current.nickname()));
    // 按可见字符边界取昵称首字，避免把中文或表情截成半个字符。
    const auto nickname = nicknameText_->text().trimmed();
    QTextBoundaryFinder first(QTextBoundaryFinder::Grapheme, nickname);
    avatar_->setText(nickname.left(first.toNextBoundary()));
    identity_->setText("当前账号：" + QString::fromStdString(current.externalId()) +
        "  |  注册年龄：" + QString::number(session_.registrationAge()) + " 年");
    if (shownAccount_ != current.accountId() || (!editingProfile_ && !editingNickname_ && !editingPassword_)) cancelEditing();
    else if (editingProfile_) previewAvatarColor(selectedColor_);
    shownAccount_ = current.accountId();
    const QStringList names{"QQ", "微信", "微博"};
    newService_->clear();
    const auto accounts = session_.accounts();
    for (int i = 0; i < 3; ++i)
    {
        const auto type = static_cast<ServiceTypeLJY>(i);
        QStringList row{names[i], "—", "—", "—", "未开通"};
        for (const auto& account : accounts)
        {
            if (account.serviceType() != type) continue;
            row[1] = QString::fromStdString(account.externalId());
            row[2] = QString::fromStdString(account.nickname());
            row[3] = QString::fromStdString(account.appliedOn());
            row[4] = session_.serviceState(type) == ServiceLoginStateLJY::LoggedIn ? "已确认" : "待确认";
            if (current.serviceType() == type) row[4] = "当前服务";
        }
        if (session_.serviceState(type) == ServiceLoginStateLJY::NotOpened) newService_->addItem(names[i], i);
        for (int column = 0; column < 5; ++column) {
            auto* item = new QTableWidgetItem(row[column]);
            item->setToolTip(row[column]);
            services_->setItem(i, column, item);
        }
    }
    const bool canOpen = newService_->count() > 0;
    findChild<QWidget*>("openServicePanel")->setVisible(canOpen);
    findChild<QLabel*>("openServiceHint")->setVisible(canOpen);
    newService_->setCurrentIndex(canOpen ? 0 : -1);
    findChild<QPushButton*>("openServiceButton")->setEnabled(canOpen);
    newService_->setEnabled(canOpen);
    newNickname_->setEnabled(canOpen);
    const auto bound = session_.boundQQNumber();
    const bool bothOpened = session_.serviceState(ServiceTypeLJY::QQ) != ServiceLoginStateLJY::NotOpened &&
        session_.serviceState(ServiceTypeLJY::WeChat) != ServiceLoginStateLJY::NotOpened;
    bindButton_->setEnabled(bothOpened && bound.empty());
    bindButton_->setText(bound.empty() ? "绑定微信" : "已绑定");
    bindButton_->setToolTip(bothOpened ? "绑定本人 QQ 与微信" : "请先开通 QQ 和微信");
    unbindButton_->setEnabled(!bound.empty());
    binding_->setText(bound.empty() ? "微信与 QQ 尚未绑定" : "微信已绑定 QQ：" + QString::fromStdString(bound));
}

void AccountPageLJY::previewAvatarColor(int color)
{
    selectedColor_ = color;
    applyAvatarColorLJY(avatar_, 0, color);
    for (int i = 0; i < 6; ++i) {
        colorButtons_[i]->setChecked(i == color);
        // 用线条绘制勾号，避免部分字体缺字显示方框。
        QPixmap mark(14, 14); mark.fill(Qt::transparent);
        if (i == color) {
            QPainter painter(&mark); painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(QColor(avatarForegroundLJY(i)), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(3, 7, 6, 10); painter.drawLine(6, 10, 11, 4);
        }
        colorButtons_[i]->setIcon(QIcon(mark)); colorButtons_[i]->setIconSize(QSize(14, 14));
        colorButtons_[i]->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: 2px solid %3; border-radius: 12px; padding: 0; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { border-color: %2; } QPushButton:focus { border-color: %2; }")
            .arg(avatarBackgroundLJY(i), avatarForegroundLJY(i), i == color ? avatarForegroundLJY(i) : "white"));
    }
}

void AccountPageLJY::updateProfile()
{
    if (!editingProfile_) { cancelEditing(); emit feedback(""); setProfileEditing(true); name_->setFocus(); return; }
    try {
        session_.updateProfile(name_->text().toUtf8().toStdString(), birth_->text().toUtf8().toStdString(),
            location_->text().toUtf8().toStdString(), selectedColor_);
        cancelEditing();
        emit changed("个人资料已修改，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("修改失败：" + QString::fromUtf8(error.what()), true); }
}

void AccountPageLJY::updateNickname()
{
    if (!editingNickname_) { cancelEditing(); emit feedback(""); setNicknameEditing(true); nickname_->setFocus(); return; }
    try {
        session_.updateNickname(nickname_->text().toUtf8().toStdString());
        cancelEditing();
        emit changed("当前服务昵称已修改，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("修改失败：" + QString::fromUtf8(error.what()), true); }
}

void AccountPageLJY::updatePassword()
{
    if (!editingPassword_) { cancelEditing(); emit feedback(""); setPasswordEditing(true); oldPassword_->setFocus(); return; }
    if (newPassword_->text() != confirmPassword_->text()) {
        emit feedback( "两次新密码不一致，请检查后重新提交。", true);
        confirmPassword_->setFocus();
        return;
    }
    try {
        session_.changePassword(oldPassword_->text().toUtf8().toStdString(), newPassword_->text().toUtf8().toStdString());
        cancelEditing();
        emit changed("密码已修改，各服务下次登录使用新密码；尚未保存到数据库。");
    }
    catch (const std::exception& error) {
        emit feedback( "修改失败：" + QString::fromUtf8(error.what()), true);
    }
}

void AccountPageLJY::openService()
{
    if (newService_->currentIndex() < 0) return;
    try {
        session_.openService(static_cast<ServiceTypeLJY>(newService_->currentData().toInt()),
            newNickname_->text().toUtf8().toStdString());
        newNickname_->clear();
        emit changed("服务已开通。点击顶部按钮确认进入；修改尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("开通失败：" + QString::fromUtf8(error.what()), true); }
}

void AccountPageLJY::bindAccounts()
{
    try { session_.bindWeChatToQQ(); emit changed("绑定成功，尚未保存到数据库。"); }
    catch (const std::exception& error) { emit feedback("操作失败：" + QString::fromUtf8(error.what()), true); }
}

void AccountPageLJY::unbindAccounts()
{
    try { session_.unbindWeChatFromQQ(); emit changed("已解除 QQ 与微信的关联，尚未保存到数据库。"); }
    catch (const std::exception& error) { emit feedback("操作失败：" + QString::fromUtf8(error.what()), true); }
}
