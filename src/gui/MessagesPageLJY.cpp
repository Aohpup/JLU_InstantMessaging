#include "gui/ListPresentationLJY.h"
#include "gui/MessagesPageLJY.h"
#include "gui/AvatarLJY.h"
#include "gui/CardSurfaceLJY.h"
#include "gui/ChatBubbleLJY.h"
#include "gui/InlineDialogLJY.h"
#include "services/ChatIdentityLJY.h"
#include <QDateTime>
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

MessagesPageLJY::MessagesPageLJY(SessionManagerLJY& session, const QString& databasePath, QWidget* parent)
    : QWidget(parent), session_(session), databasePath_(databasePath)
{
    setObjectName("messagesPage");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 12); root->setSpacing(10);

    identity_ = new QLabel; identity_->setObjectName("chatIdentity");
    identity_->setTextFormat(Qt::PlainText); identity_->setWordWrap(true);
    root->addWidget(identity_); identity_->hide();
    auto* columns = new QHBoxLayout; columns->setSpacing(16);
    auto* listCard = new QFrame; applyCardSurfaceLJY(listCard);
    listCard->setMinimumWidth(170); listCard->setMaximumWidth(235);
    auto* listBox = new QVBoxLayout(listCard); listBox->setContentsMargins(10, 14, 10, 12);
    listTitle_ = new QLabel("好友"); listTitle_->setProperty("cardHeading", true);
    listBox->addWidget(listTitle_);
    friends_ = new QListWidget; friends_->setObjectName("chatFriends");
    // 列表变窄时重新布局整行，避免资料菜单停留在旧宽度之外。
    friends_->setResizeMode(QListView::Adjust);
    friends_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    friends_->setMinimumWidth(0); friends_->setTextElideMode(Qt::ElideRight);
    addListFilterLJY(listBox, friends_, "搜索备注、昵称或号码");
    listBox->addWidget(friends_, 1);
    earlier_ = new QPushButton("加载更早消息"); earlier_->setObjectName("chatLoadEarlier");
    earlier_->setProperty("compact", true); listBox->addWidget(earlier_);
    retry_ = new QPushButton("重试保存记录"); retry_->setObjectName("chatRetryHistory");
    retry_->setProperty("compact", true); listBox->addWidget(retry_);
    historyHint_ = new QLabel; historyHint_->setObjectName("chatHistoryHint");
    historyHint_->setWordWrap(true); historyHint_->setProperty("muted", true);
    listBox->addWidget(historyHint_);
    connect(earlier_, &QPushButton::clicked, this, [this] { loadHistory(true); });
    connect(retry_, &QPushButton::clicked, this, [this] { retryHistory(); });
    columns->addWidget(listCard, 1);

    auto* chatCard = new QFrame; applyCardSurfaceLJY(chatCard);
    chatCard->setObjectName("chatCard");
    auto* box = new QVBoxLayout(chatCard); box->setContentsMargins(14, 12, 14, 12); box->setSpacing(8);
    auto* top = new QHBoxLayout;
    avatar_ = new QLabel("友"); avatar_->setObjectName("chatAvatar"); avatar_->setFixedSize(40, 40); avatar_->setAlignment(Qt::AlignCenter);
    avatar_->setTextFormat(Qt::PlainText); applyAvatarColorLJY(avatar_, 0);
    top->addWidget(avatar_);
    auto* headingBox = new QVBoxLayout; headingBox->setSpacing(2);
    title_ = new QLabel("选择一位好友"); title_->setObjectName("chatPeerTitle");
    title_->setProperty("cardHeading", true); title_->setTextFormat(Qt::PlainText);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    state_ = new QLabel("尚未连接"); state_->setObjectName("chatState"); state_->setProperty("muted", true);
    state_->setTextFormat(Qt::PlainText); state_->setWordWrap(true);
    headingBox->addWidget(title_); headingBox->addWidget(state_); top->addLayout(headingBox, 1);
    settings_ = new QPushButton("更多设置"); settings_->setObjectName("chatSettings"); settings_->setCheckable(true);
    top->addWidget(settings_); box->addLayout(top);
    connectionBar_ = new QWidget; connectionBar_->setObjectName("chatConnectionBar");
    auto* connectionBox = new QVBoxLayout(connectionBar_); connectionBox->setContentsMargins(0, 0, 0, 0); connectionBox->setSpacing(4);
    auto* quick = new QHBoxLayout;
    quick->addWidget(new QLabel("本机聊天")); quick->addStretch();
    start_ = new QPushButton("开始聊天"); start_->setObjectName("chatStart"); start_->setProperty("primary", true); start_->setMinimumHeight(36);
    quick->addWidget(start_); connectionBox->addLayout(quick);
    connectionHint_ = new QLabel; connectionHint_->setWordWrap(true); connectionHint_->setProperty("muted", true);
    connectionBox->addWidget(connectionHint_); box->addWidget(connectionBar_);

    panel_ = new QWidget; panel_->setObjectName("chatConnectionPanel");
    auto* grid = new QGridLayout(panel_); grid->setContentsMargins(0, 2, 0, 2); grid->setSpacing(8);
    port_ = new QSpinBox; port_->setObjectName("chatPort"); port_->setRange(1, 65535); port_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    grid->addWidget(new QLabel("连接地址"), 0, 0); grid->addWidget(new QLabel("127.0.0.1（本机）"), 0, 1);
    grid->addWidget(new QLabel("端口"), 0, 2); grid->addWidget(port_, 0, 3); grid->setColumnStretch(1, 1);
    role_ = new QLabel; role_->setWordWrap(true); role_->setProperty("muted", true); role_->setObjectName("chatRole");
    grid->addWidget(role_, 2, 0, 1, 4);
    auto* actions = new QGridLayout; actions->setSpacing(8);
    auto* reset = new QPushButton("恢复默认"); reset->setObjectName("chatResetSettings");
    auto* cancel = new QPushButton("取消"); cancel->setObjectName("chatCancelSettings");
    auto* save = new QPushButton("保存设置"); save->setObjectName("chatSaveSettings"); save->setProperty("primary", true);
    stop_ = new QPushButton("断开连接"); stop_->setObjectName("chatDisconnect");
    actions->addWidget(reset, 0, 0, Qt::AlignLeft); actions->addWidget(stop_, 0, 1, Qt::AlignRight);
    actions->addWidget(cancel, 1, 0, Qt::AlignLeft); actions->addWidget(save, 1, 1, Qt::AlignRight);
    stop_->setProperty("danger", true);
    grid->addLayout(actions, 3, 0, 1, 4); grid->setRowStretch(4, 1); box->addWidget(panel_, 1); panel_->hide();
    settingsPath_ = databasePath_.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/connection.ini" : databasePath_ + ".connection.ini";
    QSettings config(settingsPath_, QSettings::IniFormat);
    localPort_ = std::clamp(config.value("local/port", 27015).toInt(), 1, 65535);
    // 旧版双机设置不再使用，本机端口继续保留。
    config.remove("mode"); config.remove("lan");
    loadConnectionFields();
    connect(reset, &QPushButton::clicked, this, [this] { port_->setValue(27015); });
    connect(cancel, &QPushButton::clicked, this, [this] { loadConnectionFields(); settings_->setChecked(false); });
    connect(save, &QPushButton::clicked, this, &MessagesPageLJY::saveConnectionSettings);
    scroll_ = new QScrollArea; scroll_->setObjectName("chatScroll");
    scroll_->setWidgetResizable(true); scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll_->setMinimumHeight(65);
    timeline_ = new QWidget; timeline_->setObjectName("chatTimeline");
    bubbles_ = new QVBoxLayout(timeline_); bubbles_->setContentsMargins(4, 8, 4, 8);
    bubbles_->setSpacing(12); bubbles_->setAlignment(Qt::AlignTop);
    bubbles_->setSizeConstraint(QLayout::SetMinimumSize);
    empty_ = new QLabel("从左侧选择好友，点击开始聊天。");
    empty_->setObjectName("chatEmpty"); empty_->setWordWrap(true); empty_->setAlignment(Qt::AlignCenter); empty_->setProperty("muted", true);
    bubbles_->addWidget(empty_); scroll_->setWidget(timeline_);
    scroll_->viewport()->installEventFilter(this); box->addWidget(scroll_, 1);
    auto* bar = scroll_->verticalScrollBar();
    connect(bar, &QScrollBar::rangeChanged, this, [this, bar] {
        if (followLatest_) bar->setValue(bar->maximum());
    });
    connect(bar, &QScrollBar::valueChanged, this, [this, bar] {
        followLatest_ = bar->value() >= bar->maximum() - 24;
        if (latest_) updateReading();
    });
    latest_ = new QPushButton; latest_->setObjectName("chatLatest");
    styleUnreadBannerLJY(latest_); box->addWidget(latest_); 
    connect(latest_, &QPushButton::clicked, this, [this] {
        jumpToUnread();
    });
    composer_ = new QWidget;
    auto* composerBox = new QVBoxLayout(composer_); composerBox->setContentsMargins(0, 0, 0, 0); composerBox->setSpacing(8);
    editor_ = new QPlainTextEdit; editor_->setObjectName("chatEditor"); editor_->setFixedHeight(64);
    editor_->setPlaceholderText("输入消息，Enter 发送，Shift/Ctrl+Enter 换行"); editor_->installEventFilter(this);
    composerBox->addWidget(editor_);
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    send_ = new QPushButton("发送"); send_->setObjectName("chatSend"); send_->setProperty("primary", true);
    footer->addWidget(send_); composerBox->addLayout(footer); box->addWidget(composer_);
    columns->addWidget(chatCard, 3); root->addLayout(columns, 1);
    connect(settings_, &QPushButton::toggled, this, [this](bool shown) { panel_->setVisible(shown); if (shown) loadConnectionFields(); updateControls(); });
    connect(friends_, &QListWidget::currentRowChanged, this, [this] { selectFriend(); });
    connect(start_, &QPushButton::clicked, this, [this] {
        if (autoWaiting_ && selected_ == connectedPeer_) { stopConnection(); emit feedback("已取消连接，草稿保留。"); }
        else beginConnection();
    });
    retryTimer_ = new QTimer(this); retryTimer_->setSingleShot(true); retryTimer_->setInterval(2500);
    connect(retryTimer_, &QTimer::timeout, this, [this] { if (autoWaiting_) startConnection(); });
    connect(stop_, &QPushButton::clicked, this, [this] { stopConnection(); emit feedback("连接已停止，可重新连接。"); });
    connect(send_, &QPushButton::clicked, this, &MessagesPageLJY::sendText);
    timer_ = new QTimer(this); timer_->setInterval(50);
    connect(timer_, &QTimer::timeout, this, &MessagesPageLJY::pollEvents); timer_->start();
    updateControls();
}

MessagesPageLJY::~MessagesPageLJY() { timer_->stop(); peer_.stopChat(); }

void MessagesPageLJY::refresh()
{
    const bool loggedIn = session_.isLoggedIn();
    const auto nextService = loggedIn ? session_.currentAccount().serviceType() : service_;
    const std::string next = loggedIn ? session_.currentAccount().externalId() : "";
    if (next != owner_ || nextService != service_)
    {
        stopConnection(); owner_ = next; service_ = nextService; selected_.clear(); connectedPeer_.clear(); connectionState_ = "尚未连接"; attempted_ = false;
        previews_.clear(); findChild<QLineEdit*>("chatFriendsSearch")->clear();
        unread_.clear(); conversations_.clear(); drafts_.clear(); editor_->clear(); oldest_.clear(); historyEnded_.clear(); expanded_.clear();
    }
    supported_ = loggedIn;
    const QString name = QStringList{"QQ", "微信", "微博"}.at(static_cast<int>(service_));
    identity_->setText(loggedIn ? "我的" + name + "：" + QString::fromStdString(owner_) : "请先登录后选择好友聊天。");
    listTitle_->setText(loggedIn ? name + "好友" : "好友");
    if (loggedIn)
    {
        ownNickname_ = QString::fromStdString(session_.currentAccount().nickname());
        ownPerson_ = session_.currentPerson().personId(); ownColor_ = session_.currentPerson().avatarColor();
    }
    QSignalBlocker block(friends_);
    friends_->clear();
    if (loggedIn)
        for (const auto& account : session_.friends())
        {
            const auto number = QString::fromStdString(account.externalId());
            if (!databasePath_.isEmpty()) {
                QSettings read(databasePath_+".unread.ini",QSettings::IniFormat);
                unread_[number]=read.value(QString::number(static_cast<int>(service_))+"/"+QString::fromStdString(owner_)+"/"+number,0).toInt();
            }
            const auto remark = QString::fromStdString(session_.friendRemark(account.externalId()));
            const auto nickname = QString::fromStdString(account.nickname());
            const auto display = remark.isEmpty() ? nickname : remark;
            const auto person = session_.friendPerson(account.externalId());
            auto* item = new ConversationItemLJY(friends_);
            item->setData(Qt::UserRole+10, remark+" "+nickname+" "+number);
            item->setData(Qt::UserRole, number); item->setToolTip(display + " · " + number); item->setSizeHint(QSize(120, 84));
            auto* row = new QWidget; row->setObjectName("chatFriendRow");
            auto* layout = new QHBoxLayout(row); layout->setContentsMargins(6, 6, 4, 6); layout->setSpacing(6);
            QTextBoundaryFinder first(QTextBoundaryFinder::Grapheme, nickname);
            auto* icon = new QLabel(nickname.isEmpty() ? "友" : nickname.left(first.toNextBoundary()));
            icon->setObjectName("chatFriendAvatar"); icon->setFixedSize(32, 32); icon->setAlignment(Qt::AlignCenter);
            icon->setTextFormat(Qt::PlainText); icon->setAttribute(Qt::WA_TransparentForMouseEvents);
            applyAvatarColorLJY(icon, person.personId(), person.avatarColor()); layout->addWidget(icon);
            auto* text = new QVBoxLayout; text->setSpacing(2);
            for (const auto& value : {display, number})
            {
                auto* label = new QLabel(value); label->setObjectName("chatFriendText");
                label->setProperty("fullText", value); label->setTextFormat(Qt::PlainText);
                label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                label->setAttribute(Qt::WA_TransparentForMouseEvents);
                label->setProperty("muted", value == number); label->installEventFilter(this); text->addWidget(label);
            }
            auto* preview=new QLabel; preview->setObjectName("chatPreview");
            preview->setProperty("account",number); preview->setProperty("muted",true);
            preview->setTextFormat(Qt::PlainText); preview->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
            preview->setAttribute(Qt::WA_TransparentForMouseEvents); preview->installEventFilter(this);
            text->addWidget(preview); layout->addLayout(text, 1);
            auto* unread=new QLabel; unread->setObjectName("chatUnread"); unread->setProperty("account",number);
            styleUnreadBadgeLJY(unread);
            unread->hide(); layout->addWidget(unread);
            auto* more = new QPushButton; more->setObjectName("chatFriendMore");
            // 直接绘制三个圆点，避免字体缺少省略号字形时显示方框。
            QPixmap dots(36, 36); dots.setDevicePixelRatio(2); dots.fill(Qt::transparent);
            QPainter painter(&dots); painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen); painter.setBrush(QColor("#537565"));
            for (int x : {4, 9, 14}) painter.drawEllipse(QPointF(x, 9), 1.5, 1.5);
            painter.end(); more->setIcon(QIcon(dots)); more->setIconSize(QSize(18, 18)); more->setFixedSize(26, 30);
            more->setProperty("account", number); more->setCursor(Qt::PointingHandCursor);
            more->setToolTip("查看好友资料"); more->setAccessibleName("查看 " + display + " 的好友资料");
            connect(more, &QPushButton::clicked, this, [this, number] { emit friendRequested(number); });
            layout->addWidget(more); friends_->setItemWidget(item, row);
            if (account.externalId() == selected_) friends_->setCurrentItem(item);
            if (!previews_.contains(number) && !databasePath_.isEmpty()) {
                try {
                    const auto rows=history().load(owner_,account.externalId(),0,1,service_);
                    if (!rows.empty()) updatePreview(number,rows.back().message);
                } catch (const std::exception&) { preview->setText("摘要暂不可用"); }
            }
            const auto recent=previews_.value(number);
            item->setData(Qt::UserRole+11,recent.first);
            const auto summary=recent.first ? messageDateLJY(recent.first)+" "+QDateTime::fromMSecsSinceEpoch(recent.first).toString("HH:mm")+" · "+recent.second : "暂无消息";
            preview->setText(summary); preview->setProperty("fullText",summary); preview->setToolTip(summary);
        }
    for (const auto& number:unread_.keys()) {
        bool present=false;
        for (int i=0;i<friends_->count();++i) present |= friends_->item(i)->data(Qt::UserRole).toString()==number;
        if (!present) unread_.remove(number);
    }
    if (!connectedPeer_.empty() && (active_ || autoWaiting_)) {
        bool exists = false;
        for (int i=0; i<friends_->count(); ++i)
            exists |= friends_->item(i)->data(Qt::UserRole).toString().toStdString() == connectedPeer_;
        if (!exists) stopConnection();
    }
    if (!friends_->currentItem() && !selected_.empty())
    {
        if (selected_ == connectedPeer_) stopConnection();
        conversations_.erase(selected_); drafts_.remove(QString::fromStdString(selected_));
        oldest_.erase(selected_); historyEnded_.erase(selected_);
        selected_.clear(); editor_->clear();
    }
    if (!selected_.empty())
    {
        const auto account = session_.friendAccount(selected_);
        const auto person = session_.friendPerson(selected_);
        const auto nickname = QString::fromStdString(account.nickname());
        peerNickname_ = nickname; peerPerson_ = person.personId(); peerColor_ = person.avatarColor();
        const auto remark = QString::fromStdString(session_.friendRemark(selected_));
        title_->setText((remark.isEmpty() ? nickname : remark) + " · " + QString::fromStdString(selected_));
        title_->setToolTip(nickname + " · " + QString::fromStdString(selected_));
        QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, nickname);
        avatar_->setText(nickname.isEmpty() ? "友" : nickname.left(boundary.toNextBoundary()));
        applyAvatarColorLJY(avatar_, person.personId(), person.avatarColor());
    }
    else { title_->setText("选择一位好友"); avatar_->setText("友"); applyAvatarColorLJY(avatar_, 0); }
    friends_->sortItems(); filterListLJY(friends_);
    loadHistory(); renderConversation(); updateControls();
}

void MessagesPageLJY::openConversation(const QString& number)
{
    findChild<QLineEdit*>("chatFriendsSearch")->clear();
    refresh();
    if (!supported_) { emit feedback("请先登录后选择当前服务的好友。", true); return; }
    for (int i = 0; i < friends_->count(); ++i)
        if (friends_->item(i)->data(Qt::UserRole).toString() == number)
        { friends_->setCurrentRow(i); editor_->setFocus(); return; }
    emit feedback("该账号不在当前服务的好友列表中。", true);
}

void MessagesPageLJY::selectFriend()
{
    const auto next = friends_->currentItem() ? friends_->currentItem()->data(Qt::UserRole).toString().toStdString() : "";
    if (next == selected_) return;
    drafts_[QString::fromStdString(selected_)] = editor_->toPlainText();
    selected_ = next; followLatest_=unread_.value(QString::fromStdString(next)) == 0;
    editor_->setPlainText(drafts_.value(QString::fromStdString(selected_)));
    refresh();

}

bool MessagesPageLJY::shouldListen() const
{
    bool ownNumeric = false, peerNumeric = false;
    const auto own = QString::fromStdString(owner_).toULongLong(&ownNumeric);
    const auto other = QString::fromStdString(selected_).toULongLong(&peerNumeric);
    return ownNumeric && peerNumeric && own != other ? own < other : owner_ < selected_;
}

void MessagesPageLJY::loadConnectionFields()
{
    port_->setValue(localPort_);
}

void MessagesPageLJY::saveConnectionSettings()
{
    QSettings config(settingsPath_, QSettings::IniFormat);
    config.setValue("local/port", port_->value());
    config.sync();
    if (config.status() != QSettings::NoError) { emit feedback("连接设置保存失败，请检查配置文件的写入权限。", true); return; }
    localPort_ = port_->value();
    settings_->setChecked(false);
    emit feedback(active_ || autoWaiting_ ? "设置已保存，下次连接生效；当前连接不变。" : "连接设置已保存。");
}

void MessagesPageLJY::beginConnection()
{
    if ((active_ || autoWaiting_) && selected_ == connectedPeer_) return;
    try { makeChatIdentityLJY(session_, selected_); }
    catch (const std::exception& error) { emit feedback(QString::fromUtf8(error.what()), true); return; }
    listening_ = shouldListen();
    attemptAddress_ = "127.0.0.1";
    attemptPort_ = localPort_;
    // 用户明确连接新好友时，才停止原好友的收发。
    stopConnection(); connectedPeer_ = selected_;
    attempted_ = true; autoWaiting_ = true; waitStarted_ = QDateTime::currentMSecsSinceEpoch();
    settings_->setChecked(false); startConnection();
}

void MessagesPageLJY::startConnection()
{
    connectionFailed_ = false; handshaking_ = false;
    try
    {
        const auto identity = makeChatIdentityLJY(session_, connectedPeer_);
        connection_ = peer_.startChat(listening_, attemptAddress_.toStdString(), attemptPort_, identity);
        active_ = true; ready_ = false;
        connectionState_ = listening_ ? "等待连接 · 请对方点击开始聊天" : "正在连接对方…";
    }
    catch (const std::exception& error)
    { autoWaiting_ = false; active_ = false; connectionFailed_ = true; emit feedback("连接失败：" + QString::fromUtf8(error.what()), true); }
    updateControls();
}

void MessagesPageLJY::sendText()
{
    try
    {
        if (selected_ != connectedPeer_ || !ready_)
        { emit feedback("请先连接当前好友，输入的草稿会保留。", true); return; }
        // 使用现有会话再核对当前对象，编辑或切服务后不会向旧好友误发。
        makeChatIdentityLJY(session_, selected_);
        peer_.sendMessage(editor_->toPlainText().toUtf8().toStdString());
        editor_->clear(); drafts_.remove(QString::fromStdString(selected_));
        emit feedback("消息已提交，接收状态见气泡下方。");
        pollEvents();
    }
    catch (const std::exception& error) { emit feedback("发送失败：" + QString::fromUtf8(error.what()), true); }
}

void MessagesPageLJY::stopConnection()
{
    autoWaiting_ = false; retryTimer_->stop();
    const bool hadConnection = !connection_.empty();
    peer_.stopChat();
    pollEvents();
    connection_.clear(); active_ = false; ready_ = false;
    connectionState_ = hadConnection ? "已断开 · 可手动重新连接" : "尚未连接"; updateControls();
}

void MessagesPageLJY::pollEvents()
{
    for (const auto& event : peer_.takeEvents())
    {
        if (event.connection != connection_) continue;
        switch (event.type)
        {
        case ChatEventTypeLJY::Listening:
            connectionState_ = "等待连接 · 端口 " + QString::number(event.port); break;
        case ChatEventTypeLJY::Connecting: connectionState_ = "正在连接对方…"; break;
        case ChatEventTypeLJY::Handshaking: handshaking_ = true; connectionState_ = "已连通，正在核对好友身份…"; break;
        case ChatEventTypeLJY::Ready:
            autoWaiting_ = false; retryTimer_->stop(); ready_ = true; connectionState_ = "已连接 · 可双向聊天";
            if (selected_ == connectedPeer_) settings_->setChecked(false);
            break;
        case ChatEventTypeLJY::Incoming:
            if (selected_ != connectedPeer_ || !isVisible() || !followLatest_ || settings_->isChecked()) {
                if (selected_ == connectedPeer_) followLatest_ = false;
                ++unread_[QString::fromStdString(connectedPeer_)];
                if (!databasePath_.isEmpty()) {
                    QSettings read(databasePath_+".unread.ini",QSettings::IniFormat);
                    read.setValue(QString::number(static_cast<int>(service_))+"/"+QString::fromStdString(owner_)+"/"+QString::fromStdString(connectedPeer_),unread_.value(QString::fromStdString(connectedPeer_)));
                }
            }
            conversations_[connectedPeer_].push_back(event.message);
            if (selected_ == connectedPeer_) appendBubble(event.message);
            storeMessage(event.message);
            updatePreview(QString::fromStdString(connectedPeer_), event.message);
            break;
        case ChatEventTypeLJY::Delivery:
        {
            auto& messages = conversations_[connectedPeer_];
            auto found = std::find_if(messages.begin(), messages.end(), [&](const auto& m) { return m.id == event.message.id; });
            if (found == messages.end()) { messages.push_back(event.message); if (selected_ == connectedPeer_) appendBubble(event.message); }
            else
            {
                *found = event.message;
                auto* label = selected_ == connectedPeer_ ? deliveryLabels_.value(QString::fromStdString(found->id), nullptr) : nullptr;
                if (label) { updateDeliveryLabel(label, *found); fitBubbles(); }
            }
            storeMessage(event.message);
            updatePreview(QString::fromStdString(connectedPeer_), event.message);
            break;
        }
        case ChatEventTypeLJY::Error:
        {
            const auto detail = QString::fromStdString(event.detail);
            // 只重试尚未连通时的拒绝或超时；握手、端口和收发错误直接报告。
            const bool unavailable = detail.contains("10061") || detail.contains("10060") || detail.contains("连接超时");
            if (autoWaiting_ && !listening_ && !handshaking_ && unavailable)
            { connectionState_ = "正在等待对方开始聊天…（每 2.5 秒重试）"; break; }
            autoWaiting_ = false; connectionFailed_ = true; ready_ = false;
            connectionState_ = "连接失败 · 可重新连接"; emit feedback(detail, true); break;
        }
        case ChatEventTypeLJY::Closed:
            active_ = false; ready_ = false;
            if (autoWaiting_ && !listening_)
                retryTimer_->start();
            else if (!connectionFailed_)
            { connectionState_ = "已断开 · 点击重新连接"; emit feedback(QString::fromStdString(event.detail)); }
            break;
        }
    }
    if (autoWaiting_ && !listening_ && QDateTime::currentMSecsSinceEpoch() - waitStarted_ >= 60000)
    {
        autoWaiting_ = false; retryTimer_->stop(); peer_.stopChat(); peer_.takeEvents();
        connection_.clear(); active_ = false; ready_ = false; connectionFailed_ = true;
        connectionState_ = "等待超时 · 可重新连接";
        emit feedback("等待已超过 60 秒，请确认对方已点击开始聊天，并检查 IP、端口和网络。", true);
    }
    updateControls();
}

void MessagesPageLJY::updateControls()
{
    const bool selected = supported_ && !selected_.empty();
    settings_->setEnabled(selected); start_->setEnabled(selected);
    const bool current = selected_ == connectedPeer_;
    const bool ready = current && ready_;
    start_->setText(current && autoWaiting_ ? "取消等待" : (current && attempted_ ? "重新连接" : "开始聊天"));
    state_->setText(current ? connectionState_ :
        (active_ || autoWaiting_ ? "与 " + QString::fromStdString(connectedPeer_) + " 的连接仍保留；点击开始聊天切换到当前好友。" : "尚未连接"));
    stop_->setEnabled(active_ || autoWaiting_);
    port_->setEnabled(true);
    role_->setText(!selected ? "选择好友后自动分配连接角色。" :
        (shouldListen() ? "本窗口自动等待另一方连接。" : "本窗口自动连接另一方。"));
    connectionHint_->setText("在本机打开两个窗口，双方选择彼此并点击开始聊天，端口保持一致。");
    connectionBar_->setVisible(!ready || settings_->isChecked());
    start_->setVisible(!ready);
    // 小窗口编辑设置时使用聊天内容区，避免把输入框和表单压成细条。
    scroll_->setVisible(!settings_->isChecked()); composer_->setVisible(!settings_->isChecked());
    editor_->setEnabled(selected); send_->setEnabled(selected && ready);
    send_->setToolTip(ready ? "Enter 发送" : "先开始聊天，输入的草稿会保留");
    state_->setProperty("connected", ready);
    if (!selected) settings_->setChecked(false);
    updateHistoryControls(); updateReading();
}

void MessagesPageLJY::renderConversation()
{
    const int revision=++scrollRevision_;
    const bool follow=followLatest_;
    const int position=scroll_->verticalScrollBar()->value();
    const QSignalBlocker block(scroll_->verticalScrollBar());
    const bool wasLoading=loadingHistory_; loadingHistory_=true; lastDate_=QDate();
    deliveryLabels_.clear();
    while (auto* item = bubbles_->takeAt(0))
    {
        if (item->widget() != empty_) delete item->widget();
        delete item;
    }
    bubbles_->addWidget(empty_);
    const auto found = conversations_.find(selected_);
    empty_->setText(selected_.empty() ? "从左侧选择当前服务的一位好友。" : "还没有消息。点击“开始聊天”，开启第一段对话。");
    empty_->setVisible(found == conversations_.end() || found->second.empty());
    if (found != conversations_.end()) for (const auto& message : found->second) appendBubble(message);
    loadingHistory_=wasLoading; followLatest_=follow;
    QTimer::singleShot(0,this,[this,follow,position,revision,peer=selected_] {
        if (peer!=selected_ || revision!=scrollRevision_) return;
        const QSignalBlocker block(scroll_->verticalScrollBar());
        scroll_->verticalScrollBar()->setValue(follow ? scroll_->verticalScrollBar()->maximum() : position);
        followLatest_=follow; updateReading();
    });
}

void MessagesPageLJY::appendBubble(const ChatMessageLJY& message)
{
    const bool outgoing = message.senderAccount == owner_;
    if (outgoing && !loadingHistory_) followLatest_ = true;
    empty_->hide();
    const auto date=QDateTime::fromMSecsSinceEpoch(message.time).date();
    if (date!=lastDate_) { bubbles_->addWidget(dateSeparatorLJY(message.time)); lastDate_=date; }
    auto* row = new QWidget; auto* line = new QHBoxLayout(row); line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(8);
    // 头像使用本地账号的昵称首字和已保存颜色，不增加网络协议字段。
    const auto nickname = outgoing ? ownNickname_ : peerNickname_;
    QTextBoundaryFinder first(QTextBoundaryFinder::Grapheme, nickname);
    auto* senderAvatar = new QLabel(nickname.isEmpty() ? "友" : nickname.left(first.toNextBoundary()));
    senderAvatar->setObjectName("chatSenderAvatar");
    senderAvatar->setFixedSize(36, 36); senderAvatar->setAlignment(Qt::AlignCenter);
    senderAvatar->setTextFormat(Qt::PlainText);
    senderAvatar->setToolTip(nickname + " · " + QString::fromStdString(message.senderAccount));
    applyAvatarColorLJY(senderAvatar, outgoing ? ownPerson_ : peerPerson_, outgoing ? ownColor_ : peerColor_);
    auto* bubble = new ChatBubbleLJY(outgoing); bubble->setProperty("chatBubble", outgoing ? "outgoing" : "incoming");
    bubble->setObjectName("chatBubble");
    auto* inside = new QVBoxLayout(bubble); inside->setContentsMargins(outgoing ? 12 : 20, 8, outgoing ? 20 : 12, 8); inside->setSpacing(5);
    auto* body = new QLabel(QString::fromUtf8(message.body.data(), static_cast<int>(message.body.size())));
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->setObjectName("chatBody"); body->setTextFormat(Qt::PlainText); body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse); body->setMinimumWidth(0);
    body->setProperty("messageId", QString::fromStdString(message.id));
    inside->addWidget(body);
    auto* expand = new QPushButton("展开全文"); expand->setObjectName("chatExpand");
    expand->setFixedHeight(24); expand->setCursor(Qt::PointingHandCursor);
    inside->addWidget(expand, 0, Qt::AlignLeft);
    connect(expand, &QPushButton::clicked, this, [this, id = message.id, row] {
        if (expanded_.count(id)) expanded_.erase(id); else expanded_.insert(id);
        const int position = std::min(scroll_->verticalScrollBar()->value(), row->y());
        followLatest_ = false; fitBubbles();
        QTimer::singleShot(0, this, [this, position] { scroll_->verticalScrollBar()->setValue(position); });
    });
    auto* delivery = new QLabel; delivery->setObjectName("chatDelivery"); delivery->setTextFormat(Qt::PlainText);
    delivery->setProperty("muted", true); delivery->setWordWrap(true); updateDeliveryLabel(delivery, message);
    inside->addWidget(delivery);
    if (outgoing) line->addStretch();
    else line->addWidget(senderAvatar, 0, Qt::AlignTop);
    line->addWidget(bubble, 0, Qt::AlignTop);
    if (outgoing) line->addWidget(senderAvatar, 0, Qt::AlignTop);
    else line->addStretch();
    bubbles_->addWidget(row); deliveryLabels_[QString::fromStdString(message.id)] = delivery;
    fitBubbles();
    if (followLatest_ && !loadingHistory_) QTimer::singleShot(0, this, [this] { if (followLatest_) scroll_->verticalScrollBar()->setValue(scroll_->verticalScrollBar()->maximum()); });
}

void MessagesPageLJY::updateDeliveryLabel(QLabel* label, const ChatMessageLJY& message)
{
    const auto time = QDateTime::fromMSecsSinceEpoch(message.time).toString("MM-dd HH:mm");
    const bool outgoing = message.senderAccount == owner_;
    label->setText(time + (outgoing ? " · " + QString::fromUtf8(chatDeliveryTextLJY(message.delivery)) : " · 对方"));
    const bool failed = outgoing && (message.delivery == ChatDeliveryLJY::Failed || message.delivery == ChatDeliveryLJY::Unknown);
    label->setStyleSheet(failed ? "color: #b42318; background: transparent;" : "color: #6a8279; background: transparent;");
    label->setProperty("deliveryState", static_cast<int>(message.delivery));
}

void MessagesPageLJY::fitBubbles()
{
    const int width = std::max(150, static_cast<int>(scroll_->viewport()->width() * 0.82) - 60);
    // 按正文实际长度定宽，再按可用宽度换行，避免放大窗口后气泡仍挤成窄列。
    for (auto* frame : timeline_->findChildren<QFrame*>("chatBubble"))
    {
        auto* body = frame->findChild<QLabel*>("chatBody");
        auto* delivery = frame->findChild<QLabel*>("chatDelivery");
        body->ensurePolished(); delivery->ensurePolished();
        int natural = delivery->fontMetrics().horizontalAdvance(delivery->text());
        for (const auto& line : body->text().split('\n'))
            natural = std::max(natural, body->fontMetrics().horizontalAdvance(line));
        const int bubbleWidth = std::min(width, natural + 36);
        frame->setFixedWidth(bubbleWidth);
        const int textWidth = bubbleWidth - 34;
        // 超过六个显示行时只限制标签高度；完整正文不截断、不改写。
        const int fullHeight = body->heightForWidth(textWidth);
        const int previewHeight = body->fontMetrics().lineSpacing() * 6;
        auto* expand = frame->findChild<QPushButton*>("chatExpand");
        const bool longText = fullHeight > previewHeight;
        const bool expanded = expanded_.count(body->property("messageId").toString().toStdString()) != 0;
        expand->setVisible(longText); expand->setText(expanded ? "收起" : "展开全文");
        const int bodyHeight = longText && !expanded ? previewHeight : fullHeight;
        body->setFixedHeight(bodyHeight);
        const int height = bodyHeight + delivery->heightForWidth(textWidth) + 23 + (longText ? 29 : 0);
        frame->setFixedHeight(height);
        frame->parentWidget()->setFixedHeight(height);
    }
    bubbles_->invalidate();
}

bool MessagesPageLJY::eventFilter(QObject* object, QEvent* event)
{
    if ((object->objectName() == "chatFriendText" || object->objectName() == "chatPreview") && event->type() == QEvent::Resize)
        if (auto* label = qobject_cast<QLabel*>(object))
            label->setText(label->fontMetrics().elidedText(label->property("fullText").toString(), Qt::ElideRight, label->width()));
    if (object == editor_ && event->type() == QEvent::KeyPress)
    {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
        {
            if (key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) editor_->insertPlainText("\n");
            else if (ready_) sendText();
            else emit feedback("尚未连接，请先点击开始聊天；输入的草稿已保留。");
            return true;
        }
    }
    if (object == scroll_->viewport() && event->type() == QEvent::Resize) fitBubbles();
    return QWidget::eventFilter(object, event);
}


ChatHistoryLJY& MessagesPageLJY::history()
{
    if (!history_) history_ = std::make_unique<ChatHistoryLJY>(databasePath_.toUtf8().toStdString());
    return *history_;
}

bool MessagesPageLJY::storeMessage(const ChatMessageLJY& message)
{
    if (databasePath_.isEmpty()) return true;
    auto found = std::find_if(pending_.begin(), pending_.end(), [&](const auto& item) {
        return item.owner == owner_ && item.message.service == message.service && item.message.id == message.id;
    });
    // 已有失败项时继续按接收顺序暂存；ACK 只覆盖同条记录的状态。
    if (found != pending_.end()) { found->message = message; updateHistoryControls(); return false; }
    if (!pending_.empty()) { pending_.push_back({owner_, message}); updateHistoryControls(); return false; }
    try { history().save(owner_, message); return true; }
    catch (const std::exception& error)
    {
        pending_.push_back({owner_, message}); updateHistoryControls();
        emit feedback((message.senderAccount == owner_ ? "消息已提交，记录保存失败：" : "消息已收到，记录保存失败：")
            + QString::fromUtf8(error.what()) + "。请点击重试保存记录。", true);
        return false;
    }
}

bool MessagesPageLJY::retryHistory(bool reportSuccess)
{
    try
    {
        while (!pending_.empty())
        {
            history().save(pending_.front().owner, pending_.front().message);
            pending_.erase(pending_.begin());
        }
        updateHistoryControls();
        if (reportSuccess) emit feedback("聊天记录已保存到本地数据库。");
        return true;
    }
    catch (const std::exception& error)
    {
        updateHistoryControls();
        emit feedback("聊天记录保存失败：" + QString::fromUtf8(error.what()) + "。待保存记录仍在内存中。", true);
        return false;
    }
}

void MessagesPageLJY::updateHistoryControls()
{
    const bool persistent = !databasePath_.isEmpty();
    retry_->setVisible(!pending_.empty());
    earlier_->setVisible(persistent && !selected_.empty() && !historyEnded_.count(selected_));
    earlier_->setText(oldest_.count(selected_) ? "加载更早消息" : "读取聊天记录");
    historyHint_->setText(!pending_.empty() ? QString("%1 条记录待保存，请重试").arg(pending_.size()) :
        (persistent ? "聊天记录自动保存到本地" : "内存模式：关闭后不保留"));
    historyHint_->setStyleSheet(pending_.empty() ? "" : "color: #b42318;");
}

void MessagesPageLJY::loadHistory(bool earlier)
{
    if (databasePath_.isEmpty() || selected_.empty() || (!earlier && oldest_.count(selected_))) return;
    try
    {
        const auto cursor = oldest_.count(selected_) ? oldest_[selected_] : 0;
        const auto rows = history().load(owner_, selected_, cursor, 100, service_);
        auto& current = conversations_[selected_];
        std::vector<ChatMessageLJY> prefix;
        for (const auto& row : rows)
            if (std::none_of(current.begin(), current.end(), [&](const auto& m) { return m.id == row.message.id; }))
                prefix.push_back(row.message);
        current.insert(current.begin(), prefix.begin(), prefix.end());
        oldest_[selected_] = rows.empty() ? cursor : rows.front().rowId;
        if (rows.size() < 100) historyEnded_.insert(selected_);
        // 换回原账号时仍展示尚未写库的消息，但绝不混入其他账号的内容。
        for (const auto& item : pending_)
            if (item.owner == owner_ && item.message.service == service_ && (item.message.senderAccount == selected_ || item.message.receiverAccount == selected_))
            {
                auto found = std::find_if(current.begin(), current.end(), [&](const auto& m) { return m.id == item.message.id; });
                if (found == current.end()) current.push_back(item.message); else *found = item.message;
            }
        if (earlier)
        {
            followLatest_ = false;
            loadingHistory_ = true; renderConversation(); loadingHistory_ = false;
            followLatest_ = false;
            QTimer::singleShot(0, this, [this] { scroll_->verticalScrollBar()->setValue(0); });
            emit feedback(rows.empty() ? "没有更早的消息。" : "已加载聊天记录。");
        }
    }
    catch (const std::exception& error)
    { emit feedback("聊天记录读取失败：" + QString::fromUtf8(error.what()) + "。可点击读取聊天记录重试。", true); }
    updateHistoryControls();
}

bool MessagesPageLJY::prepareToClose()
{
    stopConnection();
    while (!retryHistory(false))
    {
        const auto answer = InlineDialogLJY::ask(this, "聊天记录尚未保存",
            "连接已停止，仍有聊天记录未写入数据库。可以重试保存、取消关闭，或放弃这些未保存记录。",
            {InlineDialogLJY::Retry, InlineDialogLJY::Cancel, InlineDialogLJY::Discard});
        if (answer == InlineDialogLJY::Cancel) return false;
        if (answer == InlineDialogLJY::Discard) { pending_.clear(); updateHistoryControls(); break; }
    }
    return true;
}

int MessagesPageLJY::unreadCount()
{
    const auto current=QString::fromStdString(selected_);
    if (isVisible() && followLatest_ && !settings_->isChecked() && unread_.value(current)>0) {
        unread_[current]=0;
        if (!databasePath_.isEmpty()) {
            QSettings read(databasePath_+".unread.ini",QSettings::IniFormat);
            read.setValue(QString::number(static_cast<int>(service_))+"/"+QString::fromStdString(owner_)+"/"+current,0);
        }
    }
    int total=0; for (int count:unread_) total+=count;
    for (auto* label:friends_->findChildren<QLabel*>("chatUnread")) {
        const int count=unread_.value(label->property("account").toString());
        label->setText(count>99 ? "99+" : QString::number(count)); label->setVisible(count>0);
    }
    updateReading();
    return total;
}

void MessagesPageLJY::updateReading()
{
    if (!latest_) return;
    const int count=unread_.value(QString::fromStdString(selected_));
    latest_->setVisible(count > 0 && !selected_.empty() && !settings_->isChecked());
    latest_->setText(QString("%1 条未读消息 · 点击查看最早未读").arg(count));
}

void MessagesPageLJY::jumpToUnread()
{
    const int count = unread_.value(QString::fromStdString(selected_));
    if (!count) return;
    try {
        auto& messages = conversations_[selected_];
        auto incomingCount = [&] { return std::count_if(messages.begin(), messages.end(),
            [this](const auto& m) { return m.senderAccount != owner_; }); };
        // 未读可能超过当前页；只向前补到能够定位为止。
        while (incomingCount() < count && !databasePath_.isEmpty() && !historyEnded_.count(selected_)) {
            const auto rows = history().load(owner_, selected_, oldest_[selected_], 100, service_);
            std::vector<ChatMessageLJY> prefix;
            for (const auto& row : rows)
                if (std::none_of(messages.begin(), messages.end(), [&](const auto& m) { return m.id == row.message.id; }))
                    prefix.push_back(row.message);
            messages.insert(messages.begin(), prefix.begin(), prefix.end());
            if (!rows.empty()) oldest_[selected_] = rows.front().rowId;
            if (rows.size() < 100 || prefix.empty()) historyEnded_.insert(selected_);
        }
        QString target; int remaining = count;
        for (auto it = messages.rbegin(); it != messages.rend(); ++it)
            if (it->senderAccount != owner_) {
                target = QString::fromStdString(it->id);
                if (--remaining == 0) break;
            }
        if (target.isEmpty()) return;
        followLatest_ = false; renderConversation(); updateHistoryControls();
        const int revision = ++scrollRevision_;
        QTimer::singleShot(0, this, [this, target, revision] {
            if (revision != scrollRevision_) return;
            scrollToMessageLJY(scroll_, timeline_, "chatBody", target);
            followLatest_ = scroll_->verticalScrollBar()->value() >= scroll_->verticalScrollBar()->maximum()-24;
            unreadCount();
        });
    } catch (const std::exception& e) { emit feedback("定位未读失败：" + QString::fromUtf8(e.what()), true); }
}
void MessagesPageLJY::updatePreview(const QString& number,const ChatMessageLJY& message)
{
    if (previews_.value(number).first>message.time) return;
    const auto summary=messageSummaryLJY(QString::fromStdString(message.body));
    previews_[number]={message.time,summary};
    const QSignalBlocker block(friends_);
    for (int i=0;i<friends_->count();++i) {
        auto* item=friends_->item(i);
        if (item->data(Qt::UserRole).toString()!=number) continue;
        item->setData(Qt::UserRole+11,message.time);
        if (auto* row=friends_->itemWidget(item)) {
            auto* label=row->findChild<QLabel*>("chatPreview");
            const auto text=messageDateLJY(message.time)+" "+QDateTime::fromMSecsSinceEpoch(message.time).toString("HH:mm")+" · "+summary;
            label->setProperty("fullText",text); label->setToolTip(text);
            label->setText(label->fontMetrics().elidedText(text,Qt::ElideRight,label->width()));
        }
    }
    friends_->sortItems();
}
