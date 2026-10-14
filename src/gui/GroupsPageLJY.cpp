#include "gui/CardSurfaceLJY.h"
#include "gui/GroupsPageLJY.h"
#include <QComboBox>
#include <QFrame>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include "gui/InlineDialogLJY.h"
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <set>

GroupsPageLJY::GroupsPageLJY(SessionManagerLJY& session, QWidget* parent)
    : QWidget(parent), session_(session)
{
    setObjectName("groupsPage");
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QVBoxLayout(this);
    // 页面边缘给阴影留白，避免滚动区域裁掉卡片投影。
    layout->setContentsMargins(20, 4, 20, 24);
    layout->setSpacing(20);
    auto* title = new QLabel("群管理");
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    identity_ = new QLabel;
    identity_->setTextFormat(Qt::PlainText);
    layout->addWidget(identity_);
    auto card = [](QBoxLayout* parent, const QString& title, int stretch = 0) {
        auto* frame = new QFrame;
        frame->setProperty("groupCard", true);
        applyCardSurfaceLJY(frame);
        auto* box = new QVBoxLayout(frame);
        box->setContentsMargins(16, 12, 16, 14);
        box->setSpacing(12);
        if (!title.isEmpty())
        {
            auto* heading = new QLabel(title);
            heading->setProperty("cardHeading", true);
            box->addWidget(heading);
        }
        parent->addWidget(frame, stretch);
        return box;
    };
    auto button = [](const QString& text, const char* name) {
        auto* result = new QPushButton(text);
        result->setObjectName(name);
        return result;
    };
    auto label = [](const char* name) {
        auto* result = new QLabel;
        result->setObjectName(name);
        result->setTextFormat(Qt::PlainText);
        result->setWordWrap(true);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return result;
    };
    auto table = [](const char* name, const QStringList& headings) {
        auto* result = new QTableWidget(0, headings.size());
        result->setObjectName(name);
        result->setHorizontalHeaderLabels(headings);
        result->setSelectionBehavior(QAbstractItemView::SelectRows);
        result->setSelectionMode(QAbstractItemView::SingleSelection);
        result->setEditTriggers(QAbstractItemView::NoEditTriggers);
        result->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        result->verticalHeader()->hide();
        result->setMinimumHeight(115);
        result->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        return result;
    };
    auto* searchCard = card(layout, "查找群聊");
    auto* searchRow = new QHBoxLayout;
    number_ = new QLineEdit;
    number_->setObjectName("groupNumber");
    number_->setPlaceholderText("输入当前服务的群号，也可查找尚未加入的群");
    auto* find = button("查找并选择", "findGroupButton");
    find->setProperty("primary", true);
    auto* reload = button("刷新", "refreshGroupsButton");
    searchRow->addWidget(number_, 1);
    searchRow->addWidget(find);
    searchRow->addWidget(reload);
    auto* chat = button("进入群聊", "groupMessageButton"); searchRow->addWidget(chat);
    connect(chat, &QPushButton::clicked, this, [this] {
        try { std::vector<ServiceAccountLJY> members; session_.group(selectedNumber_, members); emit messageRequested(selectedNumber_); }
        catch (const std::exception& e) { emit feedback(QString::fromUtf8(e.what()), true); }
    });
    searchCard->addLayout(searchRow);
    auto* columns = new QHBoxLayout;
    columns->setSpacing(20);
    auto* listCard = card(columns, "群列表", 2);
    groups_ = new QListWidget;
    groups_->setObjectName("groupsList");
    groups_->setMinimumWidth(145);
    listCard->addWidget(groups_, 1);
    auto* listHint = new QLabel("已加入的群及本人待处理的入群记录");
    listHint->setWordWrap(true);
    listHint->setProperty("muted", true);
    listCard->addWidget(listHint);
    auto* detailCard = card(columns, "", 4);
    summary_ = label("groupSummary");
    auto* membership = new QHBoxLayout;
    membership->addWidget(summary_, 1);
    apply_ = button("申请入群", "applyGroupButton");
    apply_->setProperty("primary", true);
    leave_ = button("退出群", "leaveGroupButton");
    membership->addWidget(apply_);
    membership->addWidget(leave_);
    detailCard->addLayout(membership);
    groupHint_ = label("groupHint");
    groupHint_->setProperty("muted", true);
    detailCard->addWidget(groupHint_);
    tabs_ = new QTabWidget;
    tabs_->setObjectName("groupTabs");
    auto* memberPage = new QWidget;
    auto* memberLayout = new QVBoxLayout(memberPage);
    memberLayout->setContentsMargins(0, 8, 0, 0);
    members_ = table("groupMembers", {"账号号码", "用户昵称", "群内身份"});
    memberLayout->addWidget(members_, 1);
    auto* memberActions = new QHBoxLayout;
    memberOperation_ = new QComboBox;
    memberOperation_->setObjectName("groupMemberOperation");
    memberOperation_->addItems({"移除成员", "设置管理员", "取消管理员", "转让群主"});
    memberAction_ = button("确认操作", "groupMemberAction");
    memberAction_->setProperty("coral", true);
    memberActions->addWidget(memberOperation_, 1);
    memberActions->addWidget(memberAction_);
    memberLayout->addLayout(memberActions);
    memberHint_ = label("groupMemberHint");
    memberLayout->addWidget(memberHint_);
    tabs_->addTab(memberPage, "群成员");
    auto* requestPage = new QWidget;
    auto* requestLayout = new QVBoxLayout(requestPage);
    requestLayout->setContentsMargins(0, 8, 0, 0);
    requests_ = table("groupRequests", {"编号", "申请 / 邀请", "状态"});
    requests_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    requests_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    requests_->verticalHeader()->setDefaultSectionSize(52);
    requestLayout->addWidget(requests_, 1);
    auto* requestActions = new QHBoxLayout;
    accept_ = button("同意 / 接受", "acceptGroupRequest");
    accept_->setProperty("primary", true);
    reject_ = button("拒绝", "rejectGroupRequest");
    requestActions->addStretch();
    requestActions->addWidget(accept_);
    requestActions->addWidget(reject_);
    requestLayout->addLayout(requestActions);
    requestHint_ = label("groupRequestHint");
    requestLayout->addWidget(requestHint_);
    inviteEditor_ = new QWidget;
    auto* inviteRow = new QHBoxLayout(inviteEditor_);
    inviteRow->setContentsMargins(0, 0, 0, 0);
    inviteNumber_ = new QLineEdit;
    inviteNumber_->setObjectName("groupInviteNumber");
    inviteNumber_->setPlaceholderText("被邀请人在当前服务的号码");
    invite_ = button("发送邀请", "inviteGroupButton");
    inviteRow->addWidget(inviteNumber_, 1);
    inviteRow->addWidget(invite_);
    requestLayout->addWidget(inviteEditor_);
    inviteHint_ = label("groupInviteHint");
    inviteHint_->setProperty("muted", true);
    requestLayout->addWidget(inviteHint_);
    tabs_->addTab(requestPage, "申请与邀请");
    auto* managementPage = new QWidget;
    auto* managementLayout = new QVBoxLayout(managementPage);
    managementLayout->setContentsMargins(0, 8, 0, 0);
    managementView_ = new QWidget;
    auto* view = new QVBoxLayout(managementView_);
    view->setContentsMargins(0, 0, 0, 0);
    auto* modeRow = new QHBoxLayout;
    mode_ = new QComboBox;
    mode_->setObjectName("groupMode");
    mode_->addItems({"QQ 管理模式", "微信管理模式"});
    switchMode_ = button("切换模式", "switchGroupModeButton");
    modeRow->addWidget(mode_, 1);
    modeRow->addWidget(switchMode_);
    view->addLayout(modeRow);
    subgroups_ = new QListWidget;
    subgroups_->setObjectName("subgroupsList");
    subgroups_->setMinimumHeight(100);
    subgroups_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    view->addWidget(subgroups_, 1);
    auto* childActions = new QHBoxLayout;
    createSubgroup_ = button("创建子群", "createSubgroupButton");
    createSubgroup_->setProperty("primary", true);
    openSubgroup_ = button("进入子群", "openSubgroupButton");
    parentGroup_ = button("返回父群", "parentGroupButton");
    childActions->addWidget(createSubgroup_);
    childActions->addWidget(openSubgroup_);
    childActions->addWidget(parentGroup_);
    view->addLayout(childActions);
    subgroupHint_ = label("subgroupHint");
    subgroupHint_->setProperty("muted", true);
    view->addWidget(subgroupHint_);
    managementLayout->addWidget(managementView_, 1);

    subgroupEditor_ = new QWidget;
    subgroupEditor_->setObjectName("subgroupEditor");
    auto* editor = new QVBoxLayout(subgroupEditor_);
    editor->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    subgroupNumber_ = new QLineEdit;
    subgroupNumber_->setObjectName("subgroupNumber");
    subgroupNumber_->setPlaceholderText("当前服务中未使用的正整数群号");
    subgroupName_ = new QLineEdit;
    subgroupName_->setObjectName("subgroupName");
    subgroupName_->setPlaceholderText("输入子群名称");
    subgroupOwner_ = new QComboBox;
    subgroupOwner_->setObjectName("subgroupOwner");
    form->addRow("子群群号", subgroupNumber_);
    form->addRow("子群名称", subgroupName_);
    form->addRow("子群群主", subgroupOwner_);
    editor->addLayout(form);
    editor->addWidget(new QLabel("勾选初始成员（群主自动加入）"));
    subgroupMembers_ = new QListWidget;
    subgroupMembers_->setObjectName("subgroupMembers");
    subgroupMembers_->setMinimumHeight(100);
    subgroupMembers_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    editor->addWidget(subgroupMembers_, 1);
    auto* editorActions = new QHBoxLayout;
    auto* saveChild = button("确认创建", "saveSubgroupButton");
    saveChild->setProperty("primary", true);
    auto* cancelChild = button("取消", "cancelSubgroupButton");
    editorActions->addStretch();
    editorActions->addWidget(saveChild);
    editorActions->addWidget(cancelChild);
    editor->addLayout(editorActions);
    managementLayout->addWidget(subgroupEditor_, 1);
    subgroupEditor_->hide();
    tabs_->addTab(managementPage, "子群与模式");
    connect(createSubgroup_, &QPushButton::clicked, this, &GroupsPageLJY::beginSubgroup);
    connect(openSubgroup_, &QPushButton::clicked, this, &GroupsPageLJY::openSubgroup);
    connect(subgroups_, &QListWidget::currentRowChanged, this, [this] { openSubgroup_->setEnabled(subgroups_->currentItem() != nullptr); });
    connect(parentGroup_, &QPushButton::clicked, this, [this] { if (parentNumber_) selectGroup(parentNumber_); });
    connect(saveChild, &QPushButton::clicked, this, &GroupsPageLJY::saveSubgroup);
    connect(cancelChild, &QPushButton::clicked, this, [this] { showManagement(); emit feedback("已取消创建子群。"); });
    connect(subgroupOwner_, &QComboBox::currentIndexChanged, this, [this] { updateSubgroupOwner(); });
    connect(switchMode_, &QPushButton::clicked, this, &GroupsPageLJY::switchMode);
    connect(mode_, &QComboBox::currentIndexChanged, this, [this] {
        switchMode_->setEnabled(selectedNumber_ != 0);
    });
    detailCard->addWidget(tabs_, 1);
    layout->addLayout(columns, 1);

    connect(find, &QPushButton::clicked, this, &GroupsPageLJY::findGroup);
    connect(number_, &QLineEdit::returnPressed, this, &GroupsPageLJY::findGroup);
    connect(reload, &QPushButton::clicked, this, [this] { refresh(); emit feedback("群列表已刷新。"); });
    connect(groups_, &QListWidget::currentRowChanged, this, [this] {
        if (auto* item = groups_->currentItem()) selectGroup(item->data(Qt::UserRole).toInt());
    });
    connect(apply_, &QPushButton::clicked, this, [this] { changeMembership(true); });
    connect(leave_, &QPushButton::clicked, this, [this] { changeMembership(false); });
    connect(invite_, &QPushButton::clicked, this, &GroupsPageLJY::inviteMember);
    connect(memberOperation_, &QComboBox::currentIndexChanged, this, [this] { updateMemberAction(true); });
    connect(members_, &QTableWidget::itemSelectionChanged, this, [this] { updateMemberAction(true); });
    connect(memberAction_, &QPushButton::clicked, this, &GroupsPageLJY::operateMember);
    connect(requests_, &QTableWidget::itemSelectionChanged, this, [this] { updateRequestActions(true); });
    connect(accept_, &QPushButton::clicked, this, [this] { handleRequest(true); });
    connect(reject_, &QPushButton::clicked, this, [this] { handleRequest(false); });
    refresh();
}

void GroupsPageLJY::refresh()
{
    const QSignalBlocker operationBlock(memberOperation_);
    const int next = session_.isLoggedIn() ? session_.currentAccount().accountId() : 0;
    if (next != accountId_)
    {
        selectedNumber_ = 0;
        parentNumber_ = 0;
        number_->clear();
        inviteNumber_->clear();
        tabs_->setCurrentIndex(0);
        memberOperation_->setCurrentIndex(0);
    }
    accountId_ = next;
    // 数据刷新时当前子群可能已经释放，退回仍存在的父群。
    if (next && selectedNumber_)
    {
        try { session_.selectedGroup(selectedNumber_); }
        catch (const std::exception&)
        {
            selectedNumber_ = 0;
            if (parentNumber_)
            {
                try { session_.selectedGroup(parentNumber_); selectedNumber_ = parentNumber_; }
                catch (const std::exception&) {}
            }
            parentNumber_ = 0;
        }
    }
    const QSignalBlocker block(groups_);
    groups_->clear();
    if (next)
    {
        const auto account = session_.currentAccount();
        const QStringList names{"QQ", "微信", "微博"};
        const auto joined = session_.groups();
        identity_->setText(names[static_cast<int>(account.serviceType())] + " · " +
            QString::fromStdString(account.externalId()) + "    已加入 " + QString::number(joined.size()) + " 个群");
        std::set<int> listed;
        auto add = [&](const GroupLJY& group, const QString& state) {
            if (!listed.insert(group.number()).second) return;
            auto* item = new QListWidgetItem(QString::fromStdString(group.name()) + "\n" +
                QString::number(group.number()) + " · " + state, groups_);
            item->setData(Qt::UserRole, group.number());
            if (group.number() == selectedNumber_) groups_->setCurrentItem(item);
        };
        for (const auto* group : joined) add(*group, "已加入");
        for (const auto& request : session_.groupRequests())
            if (request.state() == GroupRequestStateLJY::Pending && request.targetNumber() == account.externalId())
                add(session_.selectedGroup(request.groupNumber()), "待加入");
        if (selectedNumber_ && !listed.count(selectedNumber_))
        {
            try { add(session_.selectedGroup(selectedNumber_), "查找结果"); }
            catch (const std::exception&) { selectedNumber_ = 0; }
        }
    }
    else identity_->clear();
    showGroup();
}

void GroupsPageLJY::findGroup()
{
    bool ok = false;
    const QString text = number_->text().trimmed();
    const int number = text.toInt(&ok);
    for (const QChar ch : text) if (ch < QChar('0') || ch > QChar('9')) ok = false;
    if (!ok || number <= 0)
    {
        selectedNumber_ = 0;
        inviteNumber_->clear();
        refresh();
        emit feedback("查找失败：群号须为正整数。", true);
        return;
    }
    selectGroup(number);
}

void GroupsPageLJY::selectGroup(int number)
{
    try
    {
        const auto& selected = session_.selectedGroup(number);
        parentNumber_ = 0;
        if (selected.parentGroupId())
            for (const auto* group : session_.groups())
                if (group->groupId() == selected.parentGroupId()) { parentNumber_ = group->number(); break; }
        if (selectedNumber_ != number) members_->setCurrentCell(-1, -1);
        selectedNumber_ = number;
        inviteNumber_->clear();
        refresh();
        emit feedback("");
    }
    catch (const std::exception& error)
    {
        selectedNumber_ = 0;
        inviteNumber_->clear();
        refresh();
        emit feedback("查找失败：" + QString::fromUtf8(error.what()), true);
    }
}

void GroupsPageLJY::showGroup()
{
    const QString previousMember = selectedMember();
    const QSignalBlocker memberBlock(members_), requestBlock(requests_);
    members_->setRowCount(0);
    requests_->setRowCount(0);
    tabs_->setEnabled(selectedNumber_ != 0);
    apply_->hide();
    leave_->hide();
    inviteEditor_->hide();
    invite_->setEnabled(false);
    inviteNumber_->setEnabled(false);
    groupHint_->clear();
    inviteHint_->clear();
    if (!selectedNumber_)
        summary_->setText("请从左侧选择一个群，或在上方输入群号查找。");
    else
    {
        const auto& group = session_.selectedGroup(selectedNumber_);
        const bool joined = group.findMember(accountId_) != nullptr;
        const QString style = group.style() == GroupStyleLJY::QQ ? "QQ" : "微信";
        summary_->setText(QString::fromStdString(group.name()) + "\n群号：" + QString::number(group.number()) +
            "  ·  " + style + "管理模式  ·  " + (joined ? "已加入" : "尚未加入"));
        apply_->setVisible(!joined);
        leave_->setVisible(joined);
        (joined ? leave_ : apply_)->setEnabled(true);
        groupHint_->setText(joined ? "先选择成员，再点击所需操作。" : "加入后可查看成员资料。");
        if (joined)
        {
            std::vector<ServiceAccountLJY> accounts;
            session_.group(selectedNumber_, accounts);
            for (const auto& account : accounts)
            {
                const auto* member = group.findMember(account.accountId());
                const QString role = group.ownerAccountId() == account.accountId() ? "群主" :
                    (member->isAdministrator() ? (group.style() == GroupStyleLJY::QQ ? "管理员" : "普通成员（管理暂停）") : "普通成员");
                const int row = members_->rowCount();
                members_->insertRow(row);
                const QString number = QString::fromStdString(account.externalId());
                auto* item = new QTableWidgetItem(number);
                item->setData(Qt::UserRole, number);
                members_->setItem(row, 0, item);
                members_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(account.nickname())));
                members_->setItem(row, 2, new QTableWidgetItem(role));
                if (number == previousMember) members_->selectRow(row);
            }
        }
        const QStringList states{"待处理", "已同意", "已拒绝", "已失效"};
        for (const auto& request : session_.groupRequests(selectedNumber_))
        {
            const int row = requests_->rowCount();
            requests_->insertRow(row);
            auto* item = new QTableWidgetItem(QString::number(request.id()));
            item->setData(Qt::UserRole, request.id());
            requests_->setItem(row, 0, item);
            const QString detail = request.type() == GroupRequestTypeLJY::Application ?
                QString::fromStdString(request.targetNumber()) + " 申请入群" :
                QString::fromStdString(request.senderNumber()) + " 邀请\n" + QString::fromStdString(request.targetNumber());
            requests_->setItem(row, 1, new QTableWidgetItem(detail));
            requests_->setItem(row, 2, new QTableWidgetItem(states[static_cast<int>(request.state())]));
        }
        inviteEditor_->show();
        invite_->setEnabled(true);
        inviteNumber_->setEnabled(true);
        inviteHint_->setText("输入目标号码后点击发送邀请，邀请由对方接受后生效。");
    }
    updateMemberAction();
    updateRequestActions();
    showManagement();
}

QString GroupsPageLJY::selectedMember() const
{
    const auto* item = members_->item(members_->currentRow(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

GroupOperationLJY GroupsPageLJY::memberOperation() const
{
    switch (memberOperation_->currentIndex())
    {
    case 0: return GroupOperationLJY::Kick;
    case 3: return GroupOperationLJY::TransferOwner;
    default: return GroupOperationLJY::SetAdministrator;
    }
}

void GroupsPageLJY::updateMemberAction(bool clearFeedback)
{
    const QString target = selectedMember();
    memberAction_->setEnabled(selectedNumber_ != 0);
    memberOperation_->setEnabled(selectedNumber_ != 0);
    memberHint_->setText(target.isEmpty() ? "请先在成员表中选择目标账号。" : "当前目标：" + target + "；操作：“" + memberOperation_->currentText() + "”。");
    if (clearFeedback) emit feedback("");
}

void GroupsPageLJY::updateRequestActions(bool clearFeedback)
{
    accept_->setEnabled(selectedNumber_ != 0);
    reject_->setEnabled(selectedNumber_ != 0);
    requestHint_->setText("选择请求后点击同意或拒绝；这里只显示本人与当前群相关的记录。");
    if (clearFeedback) emit feedback("");
}

bool GroupsPageLJY::confirm(const QString& message, const QString& title)
{
    return InlineDialogLJY::ask(this, title, message) == InlineDialogLJY::Confirm;
}

void GroupsPageLJY::changeMembership(bool apply)
{
    try
    {
        session_.checkGroupOperation(selectedNumber_, apply ? GroupOperationLJY::Apply : GroupOperationLJY::Leave);
        if (!confirm(QString(apply ? "确认申请加入群 " : "确认退出群 ") + QString::number(selectedNumber_) + "？")) return;
        if (apply)
        {
            const int id = session_.applyToGroup(selectedNumber_);
            tabs_->setCurrentIndex(1);
            emit changed("入群申请已提交，请求编号：" + QString::number(id) + "，尚未保存到数据库。");
        }
        else
        {
            session_.leaveGroup(selectedNumber_);
            selectedNumber_ = 0;
            emit changed("已退出群，尚未保存到数据库。");
        }
    }
    catch (const std::exception& error) { emit feedback("操作失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::inviteMember()
{
    try
    {
        session_.checkGroupOperation(selectedNumber_, GroupOperationLJY::Invite);
        const auto target = session_.contactAccount(inviteNumber_->text().trimmed().toStdString());
        session_.checkGroupOperation(selectedNumber_, GroupOperationLJY::Invite, target.externalId());
        if (!confirm("群号：" + QString::number(selectedNumber_) + "\n邀请 " + QString::fromStdString(target.nickname()) +
            "（" + QString::fromStdString(target.externalId()) + "）入群？")) return;
        const int id = session_.inviteToGroup(selectedNumber_, target.externalId());
        inviteNumber_->clear();
        emit changed("邀请已发送，请求编号：" + QString::number(id) + "，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("邀请失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::operateMember()
{
    const QString target = selectedMember();
    if (target.isEmpty()) { emit feedback("操作失败：请先选择目标成员。", true); return; }
    try
    {
        session_.checkGroupOperation(selectedNumber_, memberOperation(), target.toStdString());
        const auto peer = session_.contactAccount(target.toStdString());
        QString detail = QString::fromStdString(peer.nickname()) + "（" + target + "）\n群号 " + QString::number(selectedNumber_);
        if (memberOperation_->currentIndex() == 3) detail += "\n转让后，你将不再是群主。";
        if (!confirm(detail, memberOperation_->currentText() + "？")) return;
        switch (memberOperation_->currentIndex())
        {
        case 0: session_.kickGroupMember(selectedNumber_, target.toStdString()); break;
        case 1: session_.setGroupAdministrator(selectedNumber_, target.toStdString(), true); break;
        case 2: session_.setGroupAdministrator(selectedNumber_, target.toStdString(), false); break;
        case 3: session_.transferGroupOwner(selectedNumber_, target.toStdString()); break;
        }
        emit changed("群成员操作已完成，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("操作失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::handleRequest(bool accept)
{
    const auto* item = requests_->item(requests_->currentRow(), 0);
    if (!item) { emit feedback("操作失败：请先选择请求。", true); return; }
    const int id = item->data(Qt::UserRole).toInt();
    try
    {
        session_.checkGroupRequest(selectedNumber_, id);
        if (!confirm(QString(accept ? "确认同意请求 " : "确认拒绝请求 ") + QString::number(id) + "？")) return;
        session_.handleGroupRequest(id, accept);
        emit changed(accept ? "请求已同意，成员已更新，尚未保存到数据库。" : "请求已拒绝，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("处理失败：" + QString::fromUtf8(error.what()), true); }
}


void GroupsPageLJY::showManagement()
{
    subgroupEditor_->hide();
    managementView_->show();
    subgroupNumber_->clear();
    subgroupName_->clear();
    const QSignalBlocker ownerBlock(subgroupOwner_), modeBlock(mode_);
    subgroupOwner_->clear();
    subgroupMembers_->clear();
    subgroups_->clear();
    openSubgroup_->setEnabled(false);
    createSubgroup_->setEnabled(selectedNumber_ != 0);
    mode_->setEnabled(selectedNumber_ != 0);
    switchMode_->setEnabled(selectedNumber_ != 0);
    parentGroup_->setVisible(selectedNumber_ && parentNumber_);
    if (!selectedNumber_) { subgroupHint_->setText("先选择有效群，再查看子群与管理模式。"); return; }
    const auto& group = session_.selectedGroup(selectedNumber_);
    mode_->setCurrentIndex(static_cast<int>(group.style()));
    if (!group.findMember(accountId_)) { subgroupHint_->setText("加入群后可查看子群列表。"); return; }
    for (const auto* child : session_.subgroups(selectedNumber_))
    {
        auto* item = new QListWidgetItem(QString::fromStdString(child->name()) + "\n群号 " +
            QString::number(child->number()) + " · " + QString::number(child->members().size()) + " 人", subgroups_);
        item->setData(Qt::UserRole, child->number());
    }
    subgroupHint_->setText(group.parentGroupId() ? "当前是一级子群，不能再创建子群；可返回父群。" :
        (subgroups_->count() ? "选择子群可进入；切换本群模式保留已有子群及请求。" : "当前没有直属子群；QQ 模式的父群群主可以创建。"));
}

void GroupsPageLJY::beginSubgroup()
{
    try
    {
        session_.checkGroupOperation(selectedNumber_, GroupOperationLJY::CreateSubgroup);
        std::vector<ServiceAccountLJY> accounts;
        session_.group(selectedNumber_, accounts);
        const QSignalBlocker block(subgroupOwner_);
        subgroupOwner_->clear();
        subgroupMembers_->clear();
        for (const auto& account : accounts)
        {
            const QString number = QString::fromStdString(account.externalId());
            const QString display = QString::fromStdString(account.nickname()) + " · " + number;
            subgroupOwner_->addItem(display, number);
            auto* item = new QListWidgetItem(display, subgroupMembers_);
            item->setData(Qt::UserRole, number);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
        subgroupOwner_->setCurrentIndex(subgroupOwner_->findData(QString::fromStdString(session_.currentAccount().externalId())));
        updateSubgroupOwner();
        managementView_->hide();
        subgroupEditor_->show();
        subgroupNumber_->setFocus();
        emit feedback("");
    }
    catch (const std::exception& error) { emit feedback("创建失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::updateSubgroupOwner()
{
    const QString owner = subgroupOwner_->currentData().toString();
    for (int row = 0; row < subgroupMembers_->count(); ++row)
    {
        auto* item = subgroupMembers_->item(row);
        const bool automatic = item->data(Qt::UserRole).toString() == owner;
        if (!(item->flags() & Qt::ItemIsEnabled)) item->setCheckState(Qt::Unchecked);
        item->setFlags(automatic ? item->flags() & ~Qt::ItemIsEnabled : item->flags() | Qt::ItemIsEnabled);
        if (automatic) item->setCheckState(Qt::Checked);
    }
}

void GroupsPageLJY::saveSubgroup()
{
    try
    {
        session_.checkGroupOperation(selectedNumber_, GroupOperationLJY::CreateSubgroup);
        const QString text = subgroupNumber_->text().trimmed();
        bool ok = false;
        const int number = text.toInt(&ok);
        for (const QChar ch : text) if (ch < QChar('0') || ch > QChar('9')) ok = false;
        if (!ok || number <= 0) throw std::invalid_argument("子群群号须为正整数。");
        const std::string owner = subgroupOwner_->currentData().toString().toStdString();
        std::vector<std::string> members;
        for (int row = 0; row < subgroupMembers_->count(); ++row)
        {
            const auto* item = subgroupMembers_->item(row);
            const auto number = item->data(Qt::UserRole).toString().toStdString();
            if (item->checkState() == Qt::Checked && number != owner) members.push_back(number);
        }
        if (!confirm("在群 " + QString::number(selectedNumber_) + " 下创建子群 " + QString::number(number) +
            "（" + subgroupName_->text().trimmed() + "）？\n群主：" + subgroupOwner_->currentText() +
            "\n初始人数：" + QString::number(members.size() + 1))) return;
        session_.createSubgroup(selectedNumber_, number, subgroupName_->text().trimmed().toUtf8().toStdString(), owner, members);
        emit changed("子群已创建，群号：" + QString::number(number) + "，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("创建失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::switchMode()
{
    try
    {
        session_.checkGroupOperation(selectedNumber_, GroupOperationLJY::SwitchMode);
        const auto style = static_cast<GroupStyleLJY>(mode_->currentIndex());
        if (session_.selectedGroup(selectedNumber_).style() == style) { emit feedback("当前已是所选管理模式。", true); return; }
        QString message = "群号 " + QString::number(selectedNumber_) + " · 成员、子群及请求保留。";
        if (style == GroupStyleLJY::WeChat) message += "\n本群管理员权限暂停，暂不能新建子群。";
        else message += "\n恢复本群管理员权限，可新建子群。";
        if (!confirm(message, "切换为" + mode_->currentText() + "？"))
        {
            const QSignalBlocker block(mode_);
            mode_->setCurrentIndex(static_cast<int>(session_.selectedGroup(selectedNumber_).style()));
            switchMode_->setEnabled(selectedNumber_ != 0);
            emit feedback("已取消切换群模式。");
            return;
        }
        session_.switchGroupMode(selectedNumber_, style);
        emit changed(style == GroupStyleLJY::WeChat ? "已切换为微信模式，已有子群及请求保留，尚未保存到数据库。" :
            "已切换为 QQ 模式，已有子群及请求保留，尚未保存到数据库。");
    }
    catch (const std::exception& error) { emit feedback("切换失败：" + QString::fromUtf8(error.what()), true); }
}

void GroupsPageLJY::openSubgroup()
{
    const auto* item = subgroups_->currentItem();
    if (item) selectGroup(item->data(Qt::UserRole).toInt());
}
