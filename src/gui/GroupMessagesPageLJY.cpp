#include "gui/GroupMessagesPageLJY.h"
#include "gui/CardSurfaceLJY.h"
#include "gui/ChatBubbleLJY.h"
#include "gui/AvatarLJY.h"
#include "gui/InlineDialogLJY.h"
#include <QComboBox>
#include <QApplication>
#include <QClipboard>
#include <QNetworkInterface>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>
#include <algorithm>

GroupMessagesPageLJY::GroupMessagesPageLJY(SessionManagerLJY &session, const QString &path, QWidget *parent)
    : QWidget(parent), session_(session), databasePath_(path)
{
    setObjectName("groupMessagesPage");
    settingsPath_ = path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
                                         "/group.connection.ini"
                                   : path + ".group.connection.ini";
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 12);
    root->setSpacing(16);
    auto *listCard = new QFrame;
    applyCardSurfaceLJY(listCard);
    listCard->setMinimumWidth(170);
    listCard->setMaximumWidth(235);
    auto *left = new QVBoxLayout(listCard);
    auto *heading = new QLabel("我的群聊");
    heading->setProperty("cardHeading", true);
    left->addWidget(heading);
    groups_ = new QListWidget;
    groups_->setObjectName("chatGroups");
    left->addWidget(groups_, 1);
    auto *tip = new QLabel("选择群即自动加入\n首位进入者自动开房");
    tip->setProperty("muted", true);
    left->addWidget(tip);
    root->addWidget(listCard, 1);
    auto *card = new QFrame;
    applyCardSurfaceLJY(card);
    auto *box = new QVBoxLayout(card);
    box->setContentsMargins(14, 12, 14, 12);
    auto *top = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    title_ = new QLabel("选择一个群聊");
    title_->setObjectName("groupChatTitle");
    title_->setProperty("cardHeading", true);
    title_->setTextFormat(Qt::PlainText);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    state_ = new QLabel("进入群聊后自动连接");
    state_->setObjectName("groupChatState");
    state_->setWordWrap(true);
    state_->setProperty("muted", true);
    state_->setTextFormat(Qt::PlainText);
    count_ = new QLabel("0 人在线");
    count_->setObjectName("groupChatOnline");
    count_->setProperty("muted", true);
    titles->addWidget(title_);
    titles->addWidget(state_);
    titles->addWidget(count_);
    top->addLayout(titles, 1);
    settings_ = new QPushButton("更多设置");
    settings_->setObjectName("groupChatSettings");
    settings_->setCheckable(true);
    top->addWidget(settings_);
    box->addLayout(top);
    settingsPanel_ = new QWidget;
    auto *form = new QGridLayout(settingsPanel_);
    form->setContentsMargins(0, 0, 0, 0);
    mode_ = new QComboBox;
    mode_->setObjectName("groupChatMode");
    mode_->addItems({"本机演示", "局域网自动发现"});
    port_ = new QSpinBox;
    port_->setObjectName("groupChatPort");
    port_->setRange(1, 65535);
    port_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    ip_ = new QLineEdit;
    ip_->setObjectName("groupChatIp");
    ip_->setPlaceholderText("留空自动发现；也可指定已有房间 IP");
    ip_->setMinimumWidth(30);
    form->addWidget(new QLabel("连接方式"), 0, 0);
    form->addWidget(mode_, 0, 1);
    form->addWidget(new QLabel("房间端口"), 1, 0);
    form->addWidget(port_, 1, 1);
    form->addWidget(new QLabel("指定 IP"), 2, 0);
    form->addWidget(ip_, 2, 1);
    form->setColumnStretch(1, 1);
    auto *localIps = new QComboBox;
    localIps->setMinimumWidth(30);
    for (const auto &adapter : QNetworkInterface::allInterfaces())
        if ((adapter.flags() & QNetworkInterface::IsUp) && !(adapter.flags() & QNetworkInterface::IsLoopBack))
            for (const auto &entry : adapter.addressEntries())
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                    localIps->addItem(adapter.humanReadableName() + " · " + entry.ip().toString(),
                                      entry.ip().toString());
    form->addWidget(new QLabel("本机 IPv4"), 3, 0);
    form->addWidget(localIps, 3, 1);
    auto *copy = new QPushButton("复制 IP");
    connect(copy, &QPushButton::clicked, this, [this, localIps] {
        if (localIps->currentData().toString().isEmpty())
        {
            emit feedback("没有可用的局域网地址。", true);
            return;
        }
        QApplication::clipboard()->setText(localIps->currentData().toString());
        emit feedback("已复制所选网卡的 IPv4。");
    });
    auto *help = new QLabel("本机无需配置。局域网由首位成员开房，其余成员自动发现。\n如网络不允许发现，填写主"
                            "持端 IP 和实际端口。保存后自动重新加入。");
    help->setWordWrap(true);
    help->setProperty("muted", true);
    form->addWidget(help, 4, 0, 1, 2);
    auto *actions = new QHBoxLayout;
    auto *cancel = new QPushButton("取消");
    cancel->setObjectName("groupChatCancelSettings");
    auto *save = new QPushButton("保存并应用");
    save->setObjectName("groupChatSaveSettings");
    save->setProperty("primary", true);
    actions->addWidget(copy);
    actions->addStretch();
    actions->addWidget(cancel);
    actions->addWidget(save);
    form->addLayout(actions, 5, 0, 1, 2);
    form->setRowStretch(6, 1);
    box->addWidget(settingsPanel_, 1);
    settingsPanel_->hide();
    earlier_ = new QPushButton("加载更早消息");
    earlier_->setObjectName("groupChatEarlier");
    box->addWidget(earlier_);
    scroll_ = new QScrollArea;
    scroll_->setObjectName("groupChatScroll");
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setMinimumHeight(60);
    timeline_ = new QWidget;
    timeline_->setObjectName("groupChatTimeline");
    bubbles_ = new QVBoxLayout(timeline_);
    bubbles_->setAlignment(Qt::AlignTop);
    bubbles_->setContentsMargins(4, 8, 4, 8);
    bubbles_->setSpacing(10);
    bubbles_->setSizeConstraint(QLayout::SetMinimumSize);
    scroll_->setWidget(timeline_);
    box->addWidget(scroll_, 1);
    scroll_->viewport()->installEventFilter(this);
    connect(scroll_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] {
        if (followLatest_)
            scroll_->verticalScrollBar()->setValue(scroll_->verticalScrollBar()->maximum());
    });
    connect(scroll_->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int value) { followLatest_ = value >= scroll_->verticalScrollBar()->maximum() - 24; });
    composer_ = new QWidget;
    auto *composer = new QVBoxLayout(composer_);
    composer->setContentsMargins(0, 0, 0, 0);
    editor_ = new QPlainTextEdit;
    editor_->setObjectName("groupChatEditor");
    editor_->setFixedHeight(64);
    editor_->setPlaceholderText("Enter 发送，Shift/Ctrl+Enter 换行");
    editor_->installEventFilter(this);
    composer->addWidget(editor_);
    auto *footer = new QHBoxLayout;
    auto *keys = new QLabel("Enter 发送 · Shift/Ctrl+Enter 换行");
    keys->setProperty("muted", true);
    footer->addWidget(keys);
    footer->addStretch();
    send_ = new QPushButton("发送");
    send_->setObjectName("groupChatSend");
    send_->setProperty("primary", true);
    footer->addWidget(send_);
    composer->addLayout(footer);
    box->addWidget(composer_);
    historyHint_ = new QLabel;
    historyHint_->setObjectName("groupChatHistoryHint");
    historyHint_->setWordWrap(true);
    historyHint_->setProperty("muted", true);
    box->addWidget(historyHint_);
    retry_ = new QPushButton("重试保存群记录");
    retry_->setObjectName("groupChatRetryHistory");
    box->addWidget(retry_);
    retry_->hide();
    root->addWidget(card, 3);
    connect(settings_, &QPushButton::toggled, this, [this](bool shown) {
        if (shown)
            loadSettings();
        settingsPanel_->setVisible(shown);
        scroll_->setVisible(!shown);
        composer_->setVisible(!shown);
        earlier_->setVisible(!shown && !databasePath_.isEmpty() && group_);
    });
    connect(cancel, &QPushButton::clicked, this, [this] { settings_->setChecked(false); });
    connect(mode_, &QComboBox::currentIndexChanged, this,
            [this] { ip_->setEnabled(mode_->currentIndex() == 1); });
    connect(save, &QPushButton::clicked, this, [this] {
        const auto ip = ip_->text().trimmed();
        if (mode_->currentIndex() == 1 && !ip.isEmpty() &&
            (QHostAddress(ip).protocol() != QAbstractSocket::IPv4Protocol || QHostAddress(ip).isNull() ||
             QHostAddress(ip).isMulticast() || ip == "255.255.255.255"))
        {
            emit feedback("请填写有效 IPv4，或留空自动发现。", true);
            return;
        }
        QSettings config(settingsPath_, QSettings::IniFormat);
        const auto prefix = QString::number(service_) + "/" + QString::number(group_) + "/";
        config.setValue(prefix + "mode", mode_->currentIndex());
        config.setValue(prefix + "port", port_->value());
        config.setValue(prefix + "ip", ip);
        config.sync();
        if (config.status() != QSettings::NoError)
        {
            emit feedback("群连接设置保存失败。", true);
            return;
        }
        settings_->setChecked(false);
        join();
    });
    connect(groups_, &QListWidget::currentRowChanged, this, [this] { selectGroup(); });
    connect(send_, &QPushButton::clicked, this, &GroupMessagesPageLJY::sendText);
    connect(earlier_, &QPushButton::clicked, this, [this] { loadHistory(true); });
    connect(retry_, &QPushButton::clicked, this, [this] { retryHistory(); });
    connect(&room_, &GroupChatClientLJY::status, this, [this](const QString &text, bool failed) {
        state_->setText(text);
        emit feedback(text, failed);
    });
    connect(&room_, &GroupChatClientLJY::online, this, [this](int count) {
        online_ = count;
        count_->setText(QString::number(count) + " 人在线");
    });
    connect(&room_, &GroupChatClientLJY::connectionChanged, this, [this](bool ready) {
        send_->setEnabled(ready);
        if (!ready)
            preserveUnconfirmed();
    });
    connect(&room_, &GroupChatClientLJY::message, this, &GroupMessagesPageLJY::receive);
    auto *timeout = new QTimer(this);
    timeout->setInterval(1000);
    connect(timeout, &QTimer::timeout, this, [this] {
        if (!sendTimes_.isEmpty() &&
            QDateTime::currentMSecsSinceEpoch() - *std::min_element(sendTimes_.begin(), sendTimes_.end()) >
                10000)
            preserveUnconfirmed();
    });
    timeout->start();
    refresh();
}
GroupMessagesPageLJY::~GroupMessagesPageLJY()
{
    room_.disconnect(this);
    room_.leave();
}
QString GroupMessagesPageLJY::draftKey() const
{
    return QString::number(service_) + ":" + owner_ + ":" + QString::number(group_);
}
GroupChatMembersLJY GroupMessagesPageLJY::members() const
{
    std::vector<ServiceAccountLJY> accounts;
    session_.group(group_, accounts);
    GroupChatMembersLJY result;
    for (const auto &account : accounts)
        result.insert(QString::fromStdString(account.externalId()),
                      {QString::fromStdString(account.nickname()), (account.personId() - 1) % 6});
    if (result.contains(owner_))
        result[owner_].second = session_.currentPerson().avatarColor();
    return result;
}
void GroupMessagesPageLJY::stop()
{
    room_.leave();
    state_->setText(group_ ? "已离开房间，重新选择群可自动加入。" : "选择群后自动加入聊天。");
    drafts_[draftKey()] = editor_->toPlainText();
    cached_[draftKey()] = messages_;
}
void GroupMessagesPageLJY::refresh()
{
    const auto owner =
        session_.isLoggedIn() ? QString::fromStdString(session_.currentAccount().externalId()) : QString();
    const int service = session_.isLoggedIn() ? static_cast<int>(session_.currentAccount().serviceType()) : 0;
    if (owner != owner_ || service != service_)
    {
        stop();
        owner_ = owner;
        service_ = service;
        group_ = 0;
        messages_.clear();
        editor_->clear();
        render();
    }
    QSignalBlocker block(groups_);
    groups_->clear();
    bool found = false;
    if (session_.isLoggedIn())
        for (const auto *group : session_.groups())
        {
            auto *item = new QListWidgetItem(QString::fromStdString(group->name()) + "\n群号 " +
                                                 QString::number(group->number()),
                                             groups_);
            item->setData(Qt::UserRole, group->number());
            if (group->number() == group_)
            {
                groups_->setCurrentItem(item);
                title_->setText(QString::fromStdString(group->name()) + " · " + QString::number(group_));
                found = true;
            }
        }
    if (group_ && !found)
    {
        stop();
        group_ = 0;
        messages_.clear();
        editor_->clear();
        render();
        state_->setText("已离开该群，群连接已停止。");
    }
    if (found)
        room_.updateMembers(members());
    settings_->setEnabled(group_ != 0);
    editor_->setEnabled(group_ != 0);
    send_->setEnabled(room_.ready());
    earlier_->setVisible(group_ && !databasePath_.isEmpty() && !settings_->isChecked());
}
void GroupMessagesPageLJY::openGroup(int number)
{
    refresh();
    for (int i = 0; i < groups_->count(); ++i)
        if (groups_->item(i)->data(Qt::UserRole).toInt() == number)
        {
            if (group_ == number)
            {
                join();
                return;
            }
            groups_->setCurrentRow(i);
            return;
        }
    emit feedback("请先加入该群，再进入群聊。", true);
}
void GroupMessagesPageLJY::selectGroup()
{
    const int next = groups_->currentItem() ? groups_->currentItem()->data(Qt::UserRole).toInt() : 0;
    if (next == group_)
        return;
    stop();
    group_ = next;
    oldest_ = 0;
    messages_ = cached_.value(draftKey());
    followLatest_ = true;
    editor_->setPlainText(drafts_.value(draftKey()));
    settings_->setChecked(false);
    if (!group_)
    {
        render();
        return;
    }
    refresh();
    loadHistory();
    join();
}
void GroupMessagesPageLJY::loadSettings()
{
    QSettings config(settingsPath_, QSettings::IniFormat);
    const auto prefix = QString::number(service_) + "/" + QString::number(group_) + "/";
    mode_->setCurrentIndex(config.value(prefix + "mode", 0).toInt() == 1 ? 1 : 0);
    port_->setValue(config.value(prefix + "port", GroupChatClientLJY::defaultPort(service_, group_)).toInt());
    ip_->setText(config.value(prefix + "ip").toString());
    ip_->setEnabled(mode_->currentIndex() == 1);
}
void GroupMessagesPageLJY::join()
{
    if (!group_)
        return;
    try
    {
        loadSettings();
        room_.enter(QString::number(service_) + ":" + QString::number(group_), owner_, members(),
                    mode_->currentIndex() == 1, port_->value(),
                    mode_->currentIndex() == 1 ? ip_->text().trimmed() : QString());
    }
    catch (const std::exception &e)
    {
        emit feedback(QString::fromUtf8(e.what()), true);
    }
}
void GroupMessagesPageLJY::sendText()
{
    try
    {
        members();
        const auto body = editor_->toPlainText();
        const auto id = room_.sendText(body);
        sending_[id] = body;
        sendTimes_[id] = QDateTime::currentMSecsSinceEpoch();
        editor_->clear();
        historyHint_->setText("发送中，正在等待房间确认…");
    }
    catch (const std::exception &e)
    {
        emit feedback(QString::fromUtf8(e.what()), !room_.ready() ? false : true);
    }
}
void GroupMessagesPageLJY::preserveUnconfirmed()
{
    if (sending_.isEmpty())
        return;
    QStringList text;
    for (const auto &body : sending_)
        text.append(body);
    if (!editor_->toPlainText().isEmpty())
        text.append(editor_->toPlainText());
    editor_->setPlainText(text.join("\n"));
    sending_.clear();
    sendTimes_.clear();
    emit feedback("群消息结果未知，未确认文字已放回输入框；请查看记录后决定是否重发。", true);
}
GroupChatHistoryLJY &GroupMessagesPageLJY::history()
{
    if (!history_)
        history_ = std::make_unique<GroupChatHistoryLJY>(databasePath_);
    return *history_;
}
void GroupMessagesPageLJY::receive(const QJsonObject &frame)
{
    if (frame.value("room").toString() != QString::number(service_) + ":" + QString::number(group_))
        return;
    auto message = GroupChatMessageLJY::fromFrame(frame);
    const auto request = frame.value("request").toString();
    if (message.sender == owner_)
    {
        sending_.remove(request);
        sendTimes_.remove(request);
    }
    if (std::any_of(messages_.begin(), messages_.end(), [&](const auto &m) { return m.id == message.id; }))
        return;
    messages_.push_back(message);
    if (!databasePath_.isEmpty())
    {
        try
        {
            history().save(service_, owner_, group_, message);
        }
        catch (const std::exception &e)
        {
            pending_.push_back({service_, group_, owner_, message});
            retry_->show();
            emit feedback("群消息已收到，但记录保存失败：" + QString::fromUtf8(e.what()), true);
        }
    }
    historyHint_->setText(pending_.empty() ? "群记录自动保存 · 房间已接收不表示所有成员已读"
                                           : "有群记录尚未保存，请重试。");
    const int position = scroll_->verticalScrollBar()->value();
    render();
    if (!followLatest_)
        scroll_->verticalScrollBar()->setValue(position);
}
void GroupMessagesPageLJY::loadHistory(bool earlier)
{
    if (databasePath_.isEmpty())
    {
        render();
        return;
    }
    try
    {
        // 多读一条判断是否还有更早记录，页面每批仍只显示 100 条。
        auto rows = history().load(service_, owner_, group_, earlier ? oldest_ : 0, 101);
        const bool hasEarlier = rows.size() > 100;
        if (hasEarlier)
            rows.erase(rows.begin());
        if (!rows.empty())
            oldest_ = rows.front().rowId;
        if (earlier)
        {
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                                      [this](const auto &row) {
                                          return std::any_of(
                                              messages_.begin(), messages_.end(),
                                              [&](const auto &old) { return old.id == row.id; });
                                      }),
                       rows.end());
            messages_.insert(messages_.begin(), rows.begin(), rows.end());
        }
        else
        {
            messages_ = rows;
            for (const auto &pending : pending_)
                if (pending.service == service_ && pending.owner == owner_ && pending.group == group_)
                    if (std::none_of(messages_.begin(), messages_.end(),
                                     [&](const auto &old) { return old.id == pending.message.id; }))
                        messages_.push_back(pending.message);
        }
        if (earlier)
            followLatest_ = false;
        render();
        if (earlier)
        {
            // 点击按钮是主动查看旧消息，布局完成后直接显示本批最早一条。
            QTimer::singleShot(0, this, [this] {
                followLatest_ = false;
                scroll_->verticalScrollBar()->setValue(0);
            });
            emit feedback(rows.empty() ? "没有更早的群消息。" :
                QString("已加载 %1 条更早的群消息%2")
                    .arg(rows.size()).arg(hasEarlier ? "。" : "，已到最早消息。"));
        }
        earlier_->setEnabled(hasEarlier);
        earlier_->setText(hasEarlier ? "加载更早消息" :
            (messages_.empty() ? "暂无群聊天记录" : "已到最早消息"));
    }
    catch (const std::exception &e)
    {
        earlier_->setEnabled(true);
        earlier_->setText("重试加载聊天记录");
        emit feedback("群历史读取失败：" + QString::fromUtf8(e.what()), true);
        render();
    }
}
bool GroupMessagesPageLJY::retryHistory(bool report)
{
    try
    {
        while (!pending_.empty())
        {
            const auto &p = pending_.front();
            history().save(p.service, p.owner, p.group, p.message);
            pending_.erase(pending_.begin());
        }
        retry_->hide();
        if (report)
            emit feedback("群聊天记录已保存。");
        return true;
    }
    catch (const std::exception &e)
    {
        retry_->show();
        emit feedback("群记录保存失败：" + QString::fromUtf8(e.what()), true);
        return false;
    }
}
bool GroupMessagesPageLJY::prepareToClose()
{
    stop();
    while (!retryHistory(false))
    {
        const auto answer = InlineDialogLJY::ask(
            this, "群记录尚未保存", "连接已停止。可重试保存、取消关闭，或明确放弃未保存的群记录。",
            {InlineDialogLJY::Retry, InlineDialogLJY::Cancel, InlineDialogLJY::Discard});
        if (answer == InlineDialogLJY::Cancel)
            return false;
        if (answer == InlineDialogLJY::Discard)
        {
            pending_.clear();
            retry_->hide();
            break;
        }
    }
    return true;
}
void GroupMessagesPageLJY::render()
{
    while (auto *item = bubbles_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    if (messages_.empty())
    {
        auto *empty =
            new QLabel(group_ ? "还没有群消息。加入后即可发送。" : "选择本人参加的群，自动进入聊天。");
        empty->setWordWrap(true);
        empty->setProperty("muted", true);
        bubbles_->addWidget(empty);
    }
    for (const auto &message : messages_)
    {
        const bool own = message.sender == owner_;
        auto *row = new QWidget;
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(8);
        QTextBoundaryFinder first(QTextBoundaryFinder::Grapheme, message.nickname);
        auto *avatar =
            new QLabel(message.nickname.isEmpty() ? "群" : message.nickname.left(first.toNextBoundary()));
        avatar->setTextFormat(Qt::PlainText);
        avatar->setObjectName("chatSenderAvatar");
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(36, 36);
        applyAvatarColorLJY(avatar, 0, message.color);
        auto *bubble = new ChatBubbleLJY(own);
        bubble->setObjectName("groupChatBubble");
        auto *inner = new QVBoxLayout(bubble);
        inner->setContentsMargins(own ? 12 : 20, 8, own ? 20 : 12, 8);
        inner->setSpacing(5);
        auto *name = new QLabel(message.nickname + " · " + message.sender);
        name->setTextFormat(Qt::PlainText);
        name->setObjectName("groupSender");
        name->setProperty("muted", true);
        name->setWordWrap(true);
        inner->addWidget(name);
        auto *body = new QLabel(message.body);
        body->setObjectName("groupChatBody");
        body->setProperty("messageId", message.id);
        body->setProperty("expanded", expanded_.contains(message.id));
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setMinimumWidth(0);
        inner->addWidget(body);
        auto *expand = new QPushButton("展开全文");
        expand->setObjectName("chatExpand");
        expand->setFixedHeight(24);
        inner->addWidget(expand, 0, Qt::AlignLeft);
        connect(expand, &QPushButton::clicked, this, [this, body] {
            const auto id = body->property("messageId").toString();
            if (expanded_.contains(id))
                expanded_.remove(id);
            else
                expanded_.insert(id);
            body->setProperty("expanded", expanded_.contains(id));
            followLatest_ = false;
            fitBubbles();
        });
        auto *stamp = new QLabel(QDateTime::fromMSecsSinceEpoch(message.time).toString("MM-dd HH:mm") +
                                 (own ? " · 房间已接收" : ""));
        stamp->setProperty("muted", true);
        stamp->setWordWrap(true);
        stamp->setObjectName("groupStamp");
        inner->addWidget(stamp);
        if (own)
            line->addStretch();
        else
            line->addWidget(avatar, 0, Qt::AlignTop);
        line->addWidget(bubble, 0, Qt::AlignTop);
        if (own)
            line->addWidget(avatar, 0, Qt::AlignTop);
        else
            line->addStretch();
        bubbles_->addWidget(row);
    }
    fitBubbles();
    if (followLatest_)
        QTimer::singleShot(0, this, [this] {
            scroll_->verticalScrollBar()->setValue(scroll_->verticalScrollBar()->maximum());
        });
}
void GroupMessagesPageLJY::fitBubbles()
{
    const int width = std::max(145, static_cast<int>(scroll_->viewport()->width() * 0.82) - 55);
    for (auto *bubble : timeline_->findChildren<QWidget *>("groupChatBubble"))
    {
        auto *body = bubble->findChild<QLabel *>("groupChatBody");
        auto *name = bubble->findChild<QLabel *>("groupSender");
        auto *stamp = bubble->findChild<QLabel *>("groupStamp");
        auto *expand = bubble->findChild<QPushButton *>("chatExpand");
        body->ensurePolished();
        int natural = std::max(name->fontMetrics().horizontalAdvance(name->text()),
                               stamp->fontMetrics().horizontalAdvance(stamp->text()));
        for (const auto &line : body->text().split('\n'))
            natural = std::max(natural, body->fontMetrics().horizontalAdvance(line));
        const int actual = std::min(width, natural + 36);
        bubble->setFixedWidth(actual);
        const int textWidth = actual - 34;
        const int full = body->heightForWidth(textWidth), preview = body->fontMetrics().lineSpacing() * 6;
        const bool longText = full > preview, expanded = body->property("expanded").toBool();
        expand->setVisible(longText);
        expand->setText(expanded ? "收起" : "展开全文");
        const int height = longText && !expanded ? preview : full;
        body->setFixedHeight(height);
        const int total = height + name->heightForWidth(textWidth) + stamp->heightForWidth(textWidth) + 28 +
                          (longText ? 29 : 0);
        bubble->setFixedHeight(total);
        bubble->parentWidget()->setFixedHeight(total);
    }
    bubbles_->invalidate();
}
bool GroupMessagesPageLJY::eventFilter(QObject *object, QEvent *event)
{
    if (object == editor_ && event->type() == QEvent::KeyPress)
    {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
        {
            if (key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))
                editor_->insertPlainText("\n");
            else
                sendText();
            return true;
        }
    }
    if (object == scroll_->viewport() && event->type() == QEvent::Resize)
        fitBubbles();
    return QWidget::eventFilter(object, event);
}
