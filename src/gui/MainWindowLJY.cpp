#include "gui/MessagesPageLJY.h"
#include "gui/GroupMessagesPageLJY.h"
#include <QTabWidget>
#include "gui/UiFeedbackLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/LoginPageLJY.h"
#include "gui/AccountPageLJY.h"
#include "gui/FriendsPageLJY.h"
#include "gui/GroupsPageLJY.h"
#include <QButtonGroup>
#include <QFile>
#include <QCloseEvent>
#include <QResource>
#include <QHBoxLayout>
#include <QLabel>
#include "gui/InlineDialogLJY.h"
#include "gui/AvatarLJY.h"
#include "gui/CardSurfaceLJY.h"
#include "gui/SidebarIconLJY.h"
#include <QTextBoundaryFinder>
#include <QIcon>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindowLJY::MainWindowLJY(SessionManagerLJY& session, std::function<void()> saveAction,
    const QString& databasePath, QWidget* parent)
    : QMainWindow(parent), session_(session), saveAction_(std::move(saveAction))
{
    // 页面静态库中的图标资源需要显式初始化。
    Q_INIT_RESOURCE(gui_assets);
    setWindowIcon(QIcon(":/gui/app.png"));
    setWindowTitle("JLU 即时通信 · 图形界面");
    resize(1100, 780);
    setMinimumSize(900, 600);
    // 根窗口同时指定字体，局部样式只改字号时也不会退回其他中文字形。
    QFont baseFont("Microsoft YaHei"); baseFont.setPixelSize(13); baseFont.setWeight(QFont::Normal);
    setFont(baseFont);
    // 外观集中到资源样式表，页面仍使用 Qt 原生控件和布局。
    QFile theme(":/gui/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()));
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto* sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(208);
    auto* nav = new QVBoxLayout(sidebar);
    nav->setContentsMargins(16, 24, 16, 20);
    nav->setSpacing(8);
    auto* brand = new QLabel("JLU  /  IM");
    brand->setObjectName("brand");
    nav->addWidget(brand);
    auto* subtitle = new QLabel("即时通信平台");
    subtitle->setProperty("muted", true);
    nav->addWidget(subtitle);
    nav->addSpacing(34);
    auto* section = new QLabel("工作空间");
    section->setProperty("muted", true);
    nav->addWidget(section);

    auto* right = new QWidget;
    auto* outer = new QVBoxLayout(right);
    outer->setContentsMargins(24, 16, 24, 12);
    outer->setSpacing(12);
    auto* top = new QHBoxLayout;
    auto* caption = new QLabel("我的空间");
    caption->setObjectName("workspaceTitle");
    top->addWidget(caption);
    top->addStretch();
    const QStringList names{"QQ", "微信", "微博"};
    for (int i = 0; i < 3; ++i)
    {
        serviceButtons_[i] = new QPushButton(names[i]);
        serviceButtons_[i]->setObjectName("switchService" + QString::number(i));
        top->addWidget(serviceButtons_[i]);
        connect(serviceButtons_[i], &QPushButton::clicked, this,
            [this, i] { switchService(static_cast<ServiceTypeLJY>(i)); });
    }
    auto* saveButton = new QPushButton("保存数据");
    saveButton->setObjectName("saveButton");
    saveButton->setEnabled(static_cast<bool>(saveAction_));
    top->addWidget(saveButton);
    logoutButton_ = new QPushButton("注销");
    logoutButton_->setObjectName("logoutButton");
    top->addWidget(logoutButton_);
    outer->addLayout(top);
    pages_ = new QStackedWidget;
    login_ = new LoginPageLJY(session_);
    account_ = new AccountPageLJY(session_);
    auto* loginScroll = new QScrollArea;
    loginScroll->setWidgetResizable(true);
    loginScroll->setFrameShape(QFrame::NoFrame);
    login_->setMinimumSize(628, 550);
    loginScroll->setWidget(login_);
    pages_->addWidget(loginScroll);
    auto* workspace = new QWidget;
    auto* body = new QHBoxLayout(workspace);
    body->setContentsMargins(0, 0, 0, 0);
    messagesNav_ = new QPushButton("消息");
    messagesNav_->setObjectName("messagesNav");
    accountNav_ = new QPushButton("我的账号");
    accountNav_->setObjectName("accountNav");
    friendsNav_ = new QPushButton("好友列表");
    friendsNav_->setObjectName("friendsNav");
    groupsNav_ = new QPushButton("群聊");
    groupsNav_->setObjectName("groupsNav");
    auto* navigation = new QButtonGroup(this);
    int iconIndex = 0;
    // 图标在左、文字在右；微软雅黑常规字重，选中只改变底色和左侧标记。
    for (auto* button : {messagesNav_, groupsNav_, friendsNav_, accountNav_})
    {
        button->setProperty("navigation", true);
        button->setIcon(sidebarIconLJY(button == friendsNav_ ? 3 : (button == accountNav_ ? 2 : iconIndex))); button->setIconSize(QSize(24,24));
        QFont font("Microsoft YaHei"); font.setPixelSize(16); font.setWeight(QFont::Normal);
        button->setFont(font); button->setMinimumHeight(48);
        button->setCursor(Qt::PointingHandCursor);
        button->setCheckable(true);
        ++iconIndex;
        navigation->addButton(button);
        nav->addWidget(button);
    }
    accountNav_->setChecked(true);
    nav->addStretch();
    identityCard_ = new QPushButton;
    identityCard_->setObjectName("sidebarProfileButton");
    applyCardSurfaceLJY(identityCard_);
    identityCard_->setCursor(Qt::PointingHandCursor);
    identityCard_->setFixedHeight(160);
    auto* profileBox = new QVBoxLayout(identityCard_);
    profileBox->setContentsMargins(8,12,8,12); profileBox->setSpacing(6);
    identityAvatar_=new QLabel; identityAvatar_->setObjectName("sidebarAvatar");
    identityAvatar_->setFixedSize(56,56); identityAvatar_->setAlignment(Qt::AlignCenter);
    identity_=new QLabel; identity_->setObjectName("sidebarNickname");
    identityNumber_=new QLabel; identityNumber_->setObjectName("sidebarNumber");
    for (auto* label : {identityAvatar_,identity_,identityNumber_})
    {
        label->setTextFormat(Qt::PlainText); label->setAlignment(Qt::AlignCenter);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        profileBox->addWidget(label,0,Qt::AlignHCenter);
    }
    identity_->setFixedSize(152,26);
    QFont identityFont=identity_->font(); identityFont.setPixelSize(17); identityFont.setBold(true); identity_->setFont(identityFont);
    nav->addWidget(identityCard_);
    connect(identityCard_,&QPushButton::clicked,this,[this]{accountNav_->click();});
    shell->addWidget(sidebar);
    shell->addWidget(right, 1);
    auto* accountScroll = new QScrollArea;
    accountScroll->setWidgetResizable(true);
    accountScroll->setFrameShape(QFrame::NoFrame);
    account_->setMinimumSize(620, 540);
    accountScroll->setWidget(account_);
    content_ = new QStackedWidget;
    content_->addWidget(accountScroll);
    auto* friendsScroll = new QScrollArea;
    friendsScroll->setWidgetResizable(true);
    friendsScroll->setFrameShape(QFrame::NoFrame);
    friends_ = new FriendsPageLJY(session_);
    friends_->setMinimumSize(620, 530);
    friendsScroll->setWidget(friends_);
    content_->addWidget(friendsScroll);
    auto* groupsScroll = new QScrollArea;
    groupsScroll->setWidgetResizable(true);
    groupsScroll->setFrameShape(QFrame::NoFrame);
    groups_ = new GroupsPageLJY(session_);
    groups_->setMinimumSize(620, 550);
    groupsScroll->setWidget(groups_);
    content_->addWidget(groupsScroll);
    messages_ = new MessagesPageLJY(session_, databasePath);
    groupMessages_ = new GroupMessagesPageLJY(session_, databasePath);
    messageTabs_ = new QTabWidget; messageTabs_->setObjectName("messageTabs");
    messageTabs_->addTab(messages_, "好友"); messageTabs_->addTab(groupMessages_, "群聊");
    content_->addWidget(messageTabs_);
    body->addWidget(content_, 1);
    connect(messagesNav_, &QPushButton::clicked, this, [this] { account_->cancelEditing(); messages_->refresh(); groupMessages_->refresh(); content_->setCurrentIndex(3); });
    connect(accountNav_, &QPushButton::clicked, this, [this] { content_->setCurrentIndex(0); });
    connect(friendsNav_, &QPushButton::clicked, this, [this] { account_->cancelEditing(); content_->setCurrentIndex(1); });
    connect(friends_, &FriendsPageLJY::changed, this, &MainWindowLJY::markChanged);
    connect(friends_, &FriendsPageLJY::messageRequested, this, [this](const QString& number) {
        messagesNav_->click();
        messageTabs_->setCurrentIndex(0); messages_->openConversation(number);
    });
    connect(messages_, &MessagesPageLJY::friendRequested, this, [this](const QString& number) {
        friendsNav_->click();
        friends_->openFriend(number);
    });
    connect(groupsNav_, &QPushButton::clicked, this, [this] { account_->cancelEditing(); content_->setCurrentIndex(2); });
    connect(groups_, &GroupsPageLJY::changed, this, &MainWindowLJY::markChanged);
    connect(groups_, &GroupsPageLJY::messageRequested, this, [this](int number) {
        messagesNav_->click(); messageTabs_->setCurrentIndex(1); groupMessages_->openGroup(number);
    });
    pages_->addWidget(workspace);
    outer->addWidget(pages_, 1);
    status_ = new QLabel;
    status_->setObjectName("saveStatus");
    status_->setWordWrap(true);
    status_->setMinimumHeight(42);
    status_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // 固定放在滚动区域外、数据库路径上方，小窗口也始终可见。
    outer->addWidget(status_);
    auto* pathLabel = new QLabel(saveAction_ ? "本地数据库  ·  " + databasePath : "演示空间  ·  内存模式，关闭后不保留");
    pathLabel->setObjectName("databasePath");
    pathLabel->setProperty("muted", true);
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setToolTip(databasePath);
    pathLabel->setWordWrap(true);
    outer->addWidget(pathLabel);
    connect(login_, &LoginPageLJY::loggedIn, this, [this] { refresh(); setFeedbackLJY(status_, "登录成功。"); });
    connect(login_, &LoginPageLJY::registered, this, [this](const QString& message) {
        refresh();
        setFeedbackLJY(status_, message + (saveAction_ ? " 尚未保存到数据库。" : " 关闭窗口后不保留。"));
    });
    connect(account_, &AccountPageLJY::changed, this, &MainWindowLJY::markChanged);
    auto report = [this](const QString& message, bool failed) { setFeedbackLJY(status_, message, failed); };
    connect(login_, &LoginPageLJY::feedback, this, report);
    connect(account_, &AccountPageLJY::feedback, this, report);
    connect(friends_, &FriendsPageLJY::feedback, this, report);
    connect(groups_, &GroupsPageLJY::feedback, this, report);
    connect(messages_, &MessagesPageLJY::feedback, this, report);
    connect(groupMessages_, &GroupMessagesPageLJY::feedback, this, report);
    connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    connect(logoutButton_, &QPushButton::clicked, this, [this] {
        account_->cancelEditing();
        messages_->stopConnection(); groupMessages_->stop();
        session_.logout();
        login_->reset();
        refresh();
        setFeedbackLJY(status_, "已注销。内存中的修改仍保留，点击保存或关闭窗口可保存。");
        if (!saveAction_) setFeedbackLJY(status_, "已注销。内存数据暂时保留，关闭窗口后清空。");
    });
    refresh();
    setFeedbackLJY(status_, saveAction_ ? "数据已加载，请登录。修改后可手动保存，关闭窗口也会保存。" : "内存练习模式，不读写数据库。");
}

void MainWindowLJY::refresh()
{
    const bool loggedIn = session_.isLoggedIn();
    pages_->setCurrentIndex(loggedIn ? 1 : 0);
    logoutButton_->setEnabled(loggedIn);
    messagesNav_->setEnabled(loggedIn);
    accountNav_->setEnabled(loggedIn);
    friendsNav_->setEnabled(loggedIn);
    groupsNav_->setEnabled(loggedIn);
    identityCard_->setEnabled(loggedIn);
    const auto nickname=loggedIn ? QString::fromStdString(session_.currentAccount().nickname()) : QString("尚未登录");
    identity_->setText(identity_->fontMetrics().elidedText(nickname,Qt::ElideRight,152));
    identity_->setToolTip(nickname);
    identityNumber_->setText(loggedIn ? QString::fromStdString(session_.currentAccount().externalId()) : "登录后进入我的账号");
    QTextBoundaryFinder first(QTextBoundaryFinder::Grapheme,nickname);
    identityAvatar_->setText(loggedIn ? nickname.left(first.toNextBoundary()) : "J");
    applyAvatarColorLJY(identityAvatar_,loggedIn ? session_.currentPerson().personId() : 0,
        loggedIn ? session_.currentPerson().avatarColor() : 0);
    const QStringList names{"QQ", "微信", "微博"};
    for (int i = 0; i < 3; ++i)
    {
        const auto type = static_cast<ServiceTypeLJY>(i);
        const bool current = loggedIn && session_.currentAccount().serviceType() == type;
        const bool opened = loggedIn && session_.serviceState(type) != ServiceLoginStateLJY::NotOpened;
        serviceButtons_[i]->setText(names[i] + (current ? " · 当前" : ""));
        serviceButtons_[i]->setEnabled(opened && !current);
        serviceButtons_[i]->setToolTip(opened ? "切换到此服务" : "请先登录并在账号页开通此服务");
    }
    if (loggedIn) account_->refresh();
    else { content_->setCurrentIndex(0); accountNav_->setChecked(true); }
    friends_->refresh();
    groups_->refresh();
    messages_->refresh(); groupMessages_->refresh();
}

void MainWindowLJY::switchService(ServiceTypeLJY type)
{
    if (InlineDialogLJY::ask(this, "切换服务", "确认进入此服务？未提交的表单输入将重新加载。") != InlineDialogLJY::Confirm) return;
    try
    {
        messages_->stopConnection(); groupMessages_->stop();
        session_.enterService(type, true);
        refresh();
        setFeedbackLJY(status_, "已切换服务。");
    }
    catch (const std::exception& error) { setFeedbackLJY(status_, "切换失败：" + QString::fromUtf8(error.what()), true); }
}

void MainWindowLJY::markChanged(const QString& message)
{
    refresh();
    setFeedbackLJY(status_, saveAction_ ? message : "修改已应用到内存，关闭窗口后不保留。");
}

bool MainWindowLJY::save()
{
    if (!saveAction_) return true;
    if (!messages_->retryHistory(false) || !groupMessages_->retryHistory(false)) return false;
    try
    {
        saveAction_();
        setFeedbackLJY(status_, "已保存到数据库。");
        return true;
    }
    catch (const std::exception& error)
    {
        setFeedbackLJY(status_, "保存失败：" + QString::fromUtf8(error.what()) + "。当前内存修改仍保留。", true);
        return false;
    }
}

void MainWindowLJY::closeEvent(QCloseEvent* event)
{
    if (!groupMessages_->prepareToClose()) { event->ignore(); return; }
    if (!messages_->prepareToClose()) { event->ignore(); return; }
    while (!save())
    {
        const auto answer=InlineDialogLJY::ask(this, "保存失败", status_->text()+"\n可重试保存、取消关闭或放弃本次未保存修改。",
            {InlineDialogLJY::Retry, InlineDialogLJY::Cancel, InlineDialogLJY::Discard});
        if (answer==InlineDialogLJY::Cancel) { event->ignore(); return; }
        if (answer==InlineDialogLJY::Discard) break;
    }
    messages_->stopConnection();
    event->accept();
}
