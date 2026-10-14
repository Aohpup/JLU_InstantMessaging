#include "gui/CardSurfaceLJY.h"
#include "gui/FriendsPageLJY.h"
#include "gui/AvatarLJY.h"
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QTextBoundaryFinder>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include "gui/InlineDialogLJY.h"
#include <QPlainTextEdit>
#include <QPushButton>

#include <QTabWidget>
#include <QVBoxLayout>

FriendsPageLJY::FriendsPageLJY(SessionManagerLJY& session, QWidget* parent)
    : QWidget(parent), session_(session)
{
    setObjectName("friendsPage");
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(20);
    auto* title = new QLabel("我的好友");
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    identity_ = new QLabel;
    identity_->setTextFormat(Qt::PlainText);
    layout->addWidget(identity_);
    auto* tabs = new QTabWidget;
    tabs->setObjectName("friendTabs");
    layout->addWidget(tabs, 1);

    // 卡片只整理控件布局，仍由原来的会话接口完成好友操作。
    auto card = [](QBoxLayout* parentLayout, const QString& title, int stretch = 0) {
        auto* frame = new QFrame;
        frame->setProperty("friendCard", true);
        applyCardSurfaceLJY(frame);
        auto* box = new QVBoxLayout(frame);
        box->setContentsMargins(16, 12, 16, 14);
        box->setSpacing(12);
        auto* heading = new QLabel(title);
        heading->setProperty("cardHeading", true);
        box->addWidget(heading);
        parentLayout->addWidget(frame, stretch);
        return box;
    };
    auto* listPage = new QWidget;
    auto* listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(20, 20, 20, 24);
    listLayout->setSpacing(20);
    auto* addCard = card(listLayout, "添加好友");
    auto* addRow = new QHBoxLayout;
    number_ = new QLineEdit;
    number_->setObjectName("addFriendNumber");
    number_->setPlaceholderText("输入对方在当前服务的账号号码");
    auto* addButton = new QPushButton("查找并添加");
    addButton->setObjectName("addFriendButton");
    addButton->setProperty("primary", true);
    addRow->addWidget(number_, 1);
    addRow->addWidget(addButton);
    addCard->addLayout(addRow);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(20);
    auto* listCard = card(columns, "好友列表", 2);
    friends_ = new QListWidget;
    friends_->setObjectName("friendsList");
    friends_->setMinimumWidth(160);
    friends_->setMinimumHeight(160);
    listCard->addWidget(friends_, 1);
    auto* detailLayout = card(columns, "好友资料", 3);
    auto* friendHeader = new QHBoxLayout;
    avatar_ = new QLabel;
    avatar_->setObjectName("friendAvatar");
    avatar_->setFixedSize(56, 56);
    avatar_->setAlignment(Qt::AlignCenter);
    avatar_->setTextFormat(Qt::PlainText);
    friendHeader->addWidget(avatar_);
    auto* friendIdentity = new QVBoxLayout;
    nickname_ = new QLabel;
    nickname_->setObjectName("friendNickname");
    nickname_->setTextFormat(Qt::PlainText);
    nickname_->setWordWrap(true);
    nickname_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    friendNumber_ = new QLabel;
    friendNumber_->setTextFormat(Qt::PlainText);
    friendNumber_->setWordWrap(true);
    friendNumber_->setProperty("muted", true);
    friendNumber_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    friendIdentity->addWidget(nickname_);
    friendIdentity->addWidget(friendNumber_);
    friendHeader->addLayout(friendIdentity, 1);
    detailLayout->addLayout(friendHeader);
    details_ = new QLabel;
    details_->setObjectName("friendDetails");
    details_->setTextFormat(Qt::PlainText);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailLayout->addWidget(details_);
    detailLayout->addStretch();
    remarkEditor_ = new QWidget;
    remarkEditor_->setObjectName("remarkEditor");
    auto* remarkLayout = new QVBoxLayout(remarkEditor_);
    remarkLayout->setContentsMargins(0, 0, 0, 0);
    remarkLayout->setSpacing(10);
    remarkLayout->addWidget(new QLabel("我的备注（留空可清除）"));
    remark_ = new QLineEdit;
    remark_->setObjectName("friendRemark");
    remarkLayout->addWidget(remark_);
    auto* actions = new QHBoxLayout;
    auto* saveRemarkButton = new QPushButton("保存备注");
    saveRemarkButton->setObjectName("saveRemarkButton");
    saveRemarkButton->setProperty("primary", true);
    auto* cancelRemarkButton = new QPushButton("取消");
    cancelRemarkButton->setObjectName("cancelRemarkButton");
    actions->addStretch();
    actions->addWidget(saveRemarkButton);
    actions->addWidget(cancelRemarkButton);
    remarkLayout->addLayout(actions);
    detailLayout->addWidget(remarkEditor_);
    remarkEditor_->hide();
    remarkButton_ = new QPushButton("修改备注");
    remarkButton_->setObjectName("friendRemarkButton");

    removeButton_ = new QPushButton("删除好友");
    removeButton_->setObjectName("removeFriendButton");
    messageButton_ = new QPushButton("发消息");
    messageButton_->setObjectName("friendMessageButton");
    messageButton_->setProperty("primary", true);
    connect(messageButton_, &QPushButton::clicked, this, [this] {
        if (friends_->currentItem()) emit messageRequested(friends_->currentItem()->data(Qt::UserRole).toString());
    });
    auto* friendActions = new QHBoxLayout;
    friendActions->setSpacing(8);
    for (auto* button : {messageButton_, remarkButton_, removeButton_})
    {
        button->setProperty("friendAction", true);
        button->setFixedSize(88, 34);
        button->setCursor(Qt::PointingHandCursor);
        friendActions->addWidget(button);
    }
    friendActions->addStretch();
    detailLayout->addLayout(friendActions);
    listLayout->addLayout(columns, 1);
    tabs->addTab(listPage, "我的好友");

    auto* queryPage = new QWidget;
    auto* queryLayout = new QVBoxLayout(queryPage);
    queryLayout->setContentsMargins(20, 20, 20, 24);
    queryLayout->setSpacing(20);
    auto* queryCard = card(queryLayout, "查询条件");
    auto* queryRow = new QHBoxLayout;
    auto* form = new QFormLayout;
    form->setSpacing(10);
    queryType_ = new QComboBox;
    queryType_->setObjectName("friendQueryType");
    queryType_->addItems({"同服务共同好友", "跨服务共同好友", "跨服务推荐", "寻找同城好友"});
    commonNumber_ = new QLineEdit;
    commonNumber_->setObjectName("commonFriendNumber");
    commonNumber_->setPlaceholderText("输入对方在当前服务的账号号码");
    source_ = new QComboBox;
    source_->setObjectName("friendSourceService");
    form->addRow("查询方式", queryType_);
    commonLabel_ = new QLabel("对方号码");
    form->addRow(commonLabel_, commonNumber_);
    sourceLabel_ = new QLabel("源服务");
    form->addRow(sourceLabel_, source_);
    queryRow->addLayout(form, 1);
    queryButton_ = new QPushButton("查询");
    auto* queryButton = queryButton_;
    queryButton->setObjectName("friendQueryButton");
    queryButton->setProperty("primary", true);
    queryButton->setMinimumWidth(90);
    queryRow->addWidget(queryButton, 0, Qt::AlignBottom);
    queryCard->addLayout(queryRow);
    auto* queryColumns = new QHBoxLayout;
    queryColumns->setSpacing(20);
    auto* resultCard = card(queryColumns, "查询结果", 3);
    results_ = new QListWidget;
    results_->setObjectName("friendQueryResults");
    results_->setMinimumWidth(180);
    results_->setMinimumHeight(100);
    resultCard->addWidget(results_, 1);
    recommendButton_ = new QPushButton("添加选中的好友");
    recommendButton_->setObjectName("addRecommendationButton");
    recommendButton_->setProperty("primary", true);
    resultCard->addWidget(recommendButton_);
    auto* reasonCard = card(queryColumns, "查询说明 / 未入选原因", 2);
    reasons_ = new QPlainTextEdit;
    reasons_->setObjectName("friendQueryReasons");
    reasons_->setReadOnly(true);
    reasons_->setMinimumWidth(160);
    reasons_->setMinimumHeight(100);
    reasonCard->addWidget(reasons_, 1);
    queryLayout->addLayout(queryColumns, 1);
    tabs->addTab(queryPage, "共同好友与推荐");
    auto* requestPage=new QWidget;
    auto* requestLayout=new QVBoxLayout(requestPage);
    requestLayout->setContentsMargins(20,20,20,24);
    auto* requestCard=card(requestLayout,"好友申请");
    auto* requestInfo=new QLabel("搜索或推荐添加会先发送申请；只有收到申请的账号同意后，双方才成为好友。");
    requestInfo->setWordWrap(true); requestCard->addWidget(requestInfo);
    requests_=new QListWidget; requests_->setObjectName("friendRequestsList"); requestCard->addWidget(requests_,1);
    auto* requestActions=new QHBoxLayout;
    auto* acceptRequest=new QPushButton("同意申请"); acceptRequest->setObjectName("acceptFriendRequest"); acceptRequest->setProperty("primary",true);
    auto* rejectRequest=new QPushButton("拒绝申请"); rejectRequest->setObjectName("rejectFriendRequest");
    requestActions->addStretch();requestActions->addWidget(acceptRequest);requestActions->addWidget(rejectRequest);
    requestCard->addLayout(requestActions); tabs->addTab(requestPage,"好友申请");
    connect(acceptRequest,&QPushButton::clicked,this,[this]{handleRequest(true);});
    connect(rejectRequest,&QPushButton::clicked,this,[this]{handleRequest(false);});
    connect(friends_, &QListWidget::currentRowChanged, this, [this] { showFriend(); });
    connect(addButton, &QPushButton::clicked, this, &FriendsPageLJY::addFriend);
    connect(number_, &QLineEdit::returnPressed, this, &FriendsPageLJY::addFriend);
    connect(removeButton_, &QPushButton::clicked, this, &FriendsPageLJY::removeFriend);
    connect(remarkButton_, &QPushButton::clicked, this, &FriendsPageLJY::beginRemarkEdit);
    connect(saveRemarkButton, &QPushButton::clicked, this, &FriendsPageLJY::saveRemark);
    connect(cancelRemarkButton, &QPushButton::clicked, this, &FriendsPageLJY::showFriend);
    connect(queryButton, &QPushButton::clicked, this, &FriendsPageLJY::query);
    connect(recommendButton_, &QPushButton::clicked, this, &FriendsPageLJY::addRecommendation);
    connect(queryType_, &QComboBox::currentIndexChanged, this, [this] { clearQuery(); });
    connect(source_, &QComboBox::currentIndexChanged, this, [this] { clearQuery(); });
    connect(commonNumber_, &QLineEdit::textChanged, this, [this] { clearQuery(); });
    connect(results_, &QListWidget::currentRowChanged, this, [this] {
        recommendButton_->setEnabled(recommendationReady_ && results_->currentItem());
    });
    refresh();
}

void FriendsPageLJY::refresh()
{
    const QString selected = friends_->currentItem() ? friends_->currentItem()->data(Qt::UserRole).toString() : QString();
    const int nextId = session_.isLoggedIn() ? session_.currentAccount().accountId() : 0;
    const bool sameAccount = accountId_ == nextId;
    accountId_ = nextId;
    showRequests();
    friends_->clear();
    source_->clear();
    clearQuery();
    if (!sameAccount) { number_->clear(); commonNumber_->clear(); }
    if (!nextId) { identity_->clear(); showFriend(); return; }
    const auto current = session_.currentAccount();
    const QStringList names{"QQ", "微信", "微博"};
    const auto accounts = session_.friends();
    identity_->setText(names[static_cast<int>(current.serviceType())] + " · " +
        QString::fromStdString(current.externalId()) + "    好友数：" + QString::number(accounts.size()));
    for (const auto& peer : accounts)
    {
        const QString number = QString::fromStdString(peer.externalId());
        const QString remark = QString::fromStdString(session_.friendRemark(peer.externalId()));
        auto* item = new QListWidgetItem((remark.isEmpty() ? QString::fromStdString(peer.nickname()) : remark) + "\n" + number, friends_);
        item->setData(Qt::UserRole, number);
        if (sameAccount && number == selected) friends_->setCurrentItem(item);
    }
    for (const auto& account : session_.accounts())
        if (account.serviceType() != current.serviceType())
            source_->addItem(names[static_cast<int>(account.serviceType())], static_cast<int>(account.serviceType()));
    if (!friends_->currentItem() && friends_->count()) friends_->setCurrentRow(0);
    showFriend();
}

void FriendsPageLJY::openFriend(const QString& number)
{
    refresh();
    findChild<QTabWidget*>("friendTabs")->setCurrentIndex(0);
    for (int i = 0; i < friends_->count(); ++i)
        if (friends_->item(i)->data(Qt::UserRole).toString() == number)
        {
            friends_->setCurrentRow(i); friends_->scrollToItem(friends_->item(i));
            showFriend(); return;
        }
    emit feedback("该账号不在当前服务的好友列表中。", true);
}

void FriendsPageLJY::showFriend()
{
    remarkEditor_->hide();
    messageButton_->show();
    remarkButton_->show();
    removeButton_->show();
    const bool selected = friends_->currentItem() != nullptr;
    messageButton_->setEnabled(selected && session_.isLoggedIn());
    messageButton_->setToolTip("与当前服务好友进行文字聊天");
    remark_->setEnabled(selected);
    remarkButton_->setEnabled(selected);
    removeButton_->setEnabled(selected);
    if (!selected)
    {
        avatar_->setText("友");
        applyAvatarColorLJY(avatar_, 0);
        nickname_->setText("尚未选择好友");
        friendNumber_->clear();
        details_->setText("从左侧选择好友查看资料，也可以在上方输入号码添加好友。");
        remark_->clear();
        return;
    }
    try
    {
        const auto number = friends_->currentItem()->data(Qt::UserRole).toString().toStdString();
        const auto account = session_.friendAccount(number);
        const auto person = session_.friendPerson(number);
        applyAvatarColorLJY(avatar_, person.personId(), person.avatarColor());
        const QString nickname = QString::fromStdString(account.nickname());
        QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, nickname);
        avatar_->setText(nickname.isEmpty() ? "友" : nickname.left(boundary.toNextBoundary()));
        nickname_->setText(nickname);
        friendNumber_->setText("账号号码：" + QString::fromStdString(number));
        details_->setText("姓名：" + QString::fromStdString(person.name()) +
            "\n\n生日：" + QString::fromStdString(person.birthDate()) + "\n\n所在地：" + QString::fromStdString(person.location()));
        remark_->setText(QString::fromStdString(session_.friendRemark(number)));
        details_->setText(details_->text() + "\n\n我的备注：" + (remark_->text().isEmpty() ? "未设置" : remark_->text()));
    }
    catch (const std::exception& error) { emit feedback( QString::fromUtf8(error.what()), true); }
}

bool FriendsPageLJY::confirm(const QString& text)
{
    return InlineDialogLJY::ask(this, "确认好友操作", text) == InlineDialogLJY::Confirm;
}

void FriendsPageLJY::addFriend()
{
    try
    {
        // 先检查不存在、本人和已有好友，再显示确认框。
        const auto peer = session_.friendCandidate(number_->text().trimmed().toStdString());
        if (!confirm("账号：" + QString::fromStdString(peer.externalId()) + "\n用户昵称：" +
            QString::fromStdString(peer.nickname()) + "\n确认发送好友申请？对方同意后才成为好友。")) return;
        session_.sendFriendRequest(peer.externalId());
        number_->clear();
        emit changed("好友申请已发送，等待对方同意，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback( "操作失败：" + QString::fromUtf8(error.what()), true); }
}

void FriendsPageLJY::removeFriend()
{
    if (!friends_->currentItem()) return;
    const auto number = friends_->currentItem()->data(Qt::UserRole).toString();
    if (!confirm("确认删除好友 " + number + "？双方好友关系和各自备注都会删除。")) return;
    try { session_.removeFriend(number.toStdString()); emit changed("好友已删除，尚未保存到数据库。"); }
    catch (const std::exception& error) { emit feedback( "操作失败：" + QString::fromUtf8(error.what()), true); }
}

void FriendsPageLJY::beginRemarkEdit()
{
    if (!friends_->currentItem()) return;
    remarkEditor_->show();
    messageButton_->hide();
    remarkButton_->hide();
    removeButton_->hide();
    emit feedback("");
    remark_->setFocus();
    remark_->selectAll();
}

void FriendsPageLJY::saveRemark()
{
    if (!friends_->currentItem()) return;
    try
    {
        session_.updateFriendRemark(friends_->currentItem()->data(Qt::UserRole).toString().toStdString(), remark_->text().toUtf8().toStdString());
        emit changed("本人备注已修改，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback( "操作失败：" + QString::fromUtf8(error.what()), true); }
}

void FriendsPageLJY::clearQuery()
{
    recommendationReady_ = false;
    cityOffset_ = 0;
    results_->clear();
    recommendButton_->setEnabled(false);
    recommendButton_->show();
    recommendButton_->setText(queryType_->currentIndex() >= 2 ? "发送好友申请" : "已是好友");
    const bool sameService = queryType_->currentIndex() == 0;
    commonNumber_->setVisible(sameService);
    commonLabel_->setVisible(sameService);
    const bool city=queryType_->currentIndex()==3;
    source_->setVisible(!sameService && !city);
    sourceLabel_->setVisible(!sameService && !city);
    queryButton_->setText(city ? "寻找同城好友" : "查询");
    reasons_->setPlainText(sameService ? "输入同服务账号，查询双方的共同好友。" :
        "选择另一项已开通服务，结果号码属于当前服务。共同好友已经是好友；需要添加时选择“跨服务推荐”。涉及微信时需有效绑定。");
}

void FriendsPageLJY::query()
{
    const int nextOffset=cityOffset_;
    clearQuery();
    cityOffset_=nextOffset;
    try
    {
        std::vector<ServiceAccountLJY> accounts;
        QStringList reasons;
        if (queryType_->currentIndex() == 0)
            accounts = session_.commonFriends(commonNumber_->text().trimmed().toStdString());
        else if (queryType_->currentIndex()==3)
        {
            const auto candidates=session_.cityCandidates();
            if (cityOffset_ >= static_cast<int>(candidates.size())) cityOffset_=0;
            const int end=std::min(cityOffset_+3,static_cast<int>(candidates.size()));
            for (int i=cityOffset_;i<end;++i) accounts.push_back(candidates[i]);
            cityOffset_=end;
            recommendationReady_=true;
            queryButton_->setText("换一批");
            reasons << "同城依据：个人资料中的所在地完全相同。" << "每批最多 3 人，显示完所有候选后从头循环。";
        }
        else
        {
            if (source_->currentIndex() < 0) { emit feedback( "请先在账号页开通另一项服务。", true); return; }
            resultSource_ = static_cast<ServiceTypeLJY>(source_->currentData().toInt());
            const bool recommend = queryType_->currentIndex() == 2;
            const auto result = recommend ? session_.friendRecommendations(resultSource_) : session_.crossServiceCommonFriends(resultSource_);
            accounts = result.accounts();
            for (const auto& reason : result.skippedReasons()) reasons.append(QString::fromStdString(reason));
            recommendationReady_ = recommend;
        }
        for (const auto& account : accounts)
        {
            auto* item = new QListWidgetItem(QString::fromStdString(account.nickname()) + " | " + QString::fromStdString(account.externalId()), results_);
            item->setData(Qt::UserRole, QString::fromStdString(account.externalId()));
        }
        reasons_->setPlainText(reasons.isEmpty() ? "没有未入选记录。" : reasons.join('\n'));
        emit feedback( accounts.empty() ? "没有符合条件的账号。" : "查询到 " + QString::number(accounts.size()) + " 个账号。");
        if (results_->count()) results_->setCurrentRow(0);
    }
    catch (const std::exception& error) { emit feedback( "查询失败：" + QString::fromUtf8(error.what()), true); }
}

void FriendsPageLJY::addRecommendation()
{
    if (!recommendationReady_ || !results_->currentItem()) return;
    const auto number = results_->currentItem()->data(Qt::UserRole).toString();
    if (!confirm("确认向当前服务账号 " + number + " 发送好友申请？")) return;
    try
    {
        session_.sendFriendRequest(number.toStdString());
        emit changed("推荐好友申请已发送，等待对方同意，尚未保存到数据库。继续推荐时请重新查询。");
    }
    catch (const std::exception& error) { emit feedback( "操作失败：" + QString::fromUtf8(error.what()), true); }
}

void FriendsPageLJY::showRequests()
{
    requests_->clear();
    if (!accountId_) return;
    const QStringList states{"待处理","已同意","已拒绝"};
    for (const auto& request : session_.friendRequests())
    {
        const bool incoming=request.recipientId()==accountId_;
        const auto peer=session_.contactAccount(incoming ? request.senderId() : request.recipientId());
        auto* item=new QListWidgetItem((incoming ? "收到 · " : "发出 · ")+QString::fromStdString(peer.nickname())+
            "（"+QString::fromStdString(peer.externalId())+"） · "+states[request.state()],requests_);
        item->setData(Qt::UserRole,QString::fromStdString(peer.externalId()));
    }
}
void FriendsPageLJY::handleRequest(bool accept)
{
    if (!requests_->currentItem()) { emit feedback("请先选择一条好友申请。",true); return; }
    try
    {
        const auto peer=requests_->currentItem()->data(Qt::UserRole).toString();
        if (!confirm((accept ? "同意来自 " : "拒绝来自 ")+peer+" 的好友申请？")) return;
        session_.handleFriendRequest(peer.toStdString(),accept);
        emit changed(accept ? "已同意申请，双方已成为好友，尚未保存到数据库。" : "已拒绝好友申请，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("处理失败："+QString::fromUtf8(error.what()),true); }
}
