#pragma once
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>

// 列表只隐藏不匹配行，保留当前聊天和草稿；清空搜索即可恢复全部。
inline void filterListLJY(QListWidget* list)
{
    auto* field = list->parentWidget()->findChild<QLineEdit*>(list->objectName()+"Search");
    if (!field) return;
    int count = 0;
    for (int i=0; i<list->count(); ++i) {
        auto* item=list->item(i);
        const auto text=item->data(Qt::UserRole+10).isValid() ? item->data(Qt::UserRole+10).toString() : item->text();
        const bool match=text.contains(field->text().trimmed(),Qt::CaseInsensitive);
        item->setHidden(!match); count+=match;
    }
    if (auto* hint=list->parentWidget()->findChild<QLabel*>(list->objectName()+"SearchHint"))
        hint->setVisible(count==0 && !field->text().trimmed().isEmpty());
}
inline void addListFilterLJY(QVBoxLayout* box, QListWidget* list, const QString& placeholder)
{
    auto* field=new QLineEdit;
    field->setObjectName(list->objectName()+"Search"); field->setPlaceholderText(placeholder);
    field->setClearButtonEnabled(true); box->addWidget(field);
    auto* hint=new QLabel("没有匹配的结果"); hint->setObjectName(list->objectName()+"SearchHint");
    hint->setProperty("muted",true); box->addWidget(hint); hint->hide();
    QObject::connect(field,&QLineEdit::textChanged,list,[list]{filterListLJY(list);});
}
// 最近消息倒序；同一时间用号码排序，刷新时不会随机换位。
class ConversationItemLJY : public QListWidgetItem
{
public:
    explicit ConversationItemLJY(QListWidget* list) : QListWidgetItem(list) {}
    bool operator<(const QListWidgetItem& other) const override {
        const auto a=data(Qt::UserRole+11).toLongLong(), b=other.data(Qt::UserRole+11).toLongLong();
        return a!=b ? a>b : data(Qt::UserRole).toString()<other.data(Qt::UserRole).toString();
    }
};
inline QString messageSummaryLJY(const QString& text)
{
    const auto line=text.simplified();
    return line.size()>48 ? line.left(48)+"…" : line;
}
inline QString messageDateLJY(long long time)
{
    const auto date=QDateTime::fromMSecsSinceEpoch(time).date();
    if (date==QDate::currentDate()) return "今天";
    if (date==QDate::currentDate().addDays(-1)) return "昨天";
    return date.toString("yyyy-MM-dd");
}
inline QLabel* dateSeparatorLJY(long long time)
{
    auto* label=new QLabel(messageDateLJY(time)); label->setObjectName("chatDateSeparator");
    label->setAlignment(Qt::AlignCenter); label->setProperty("muted",true);
    label->setContentsMargins(0,8,0,4); return label;
}

// 红底白字的小胶囊统一用于好友、群和页签未读数。
inline void styleUnreadBadgeLJY(QLabel* badge)
{
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(20); badge->setMinimumWidth(20);
    badge->setStyleSheet("color:white;background:#e53935;font-size:12px;font-weight:bold;border-radius:10px;padding:0 5px;");
}
// 浅色横条占满聊天区宽度，不再挤在发送按钮旁边。
inline void styleUnreadBannerLJY(QPushButton* button)
{
    button->setFixedHeight(32);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet("QPushButton { background:#e4f0eb; color:#245e4d; border:1px solid #c9dfd4; border-radius:6px; padding:3px 8px; }"
        "QPushButton:hover { background:#d5e9de; }");
    button->hide();
}
// 把第一条未读的气泡顶部放入视野；长消息也从开头读起。
inline void scrollToMessageLJY(QScrollArea* scroll, QWidget* timeline, const char* bodyName, const QString& id)
{
    timeline->layout()->activate();
    for (auto* label : timeline->findChildren<QLabel*>(bodyName))
        if (label->property("messageId").toString() == id) {
            const QSignalBlocker block(scroll->verticalScrollBar());
            auto* row = label->parentWidget()->parentWidget();
            scroll->verticalScrollBar()->setValue(row->mapTo(timeline, QPoint()).y());
            break;
        }
}
// 文字已由行内标签显示；默认绘制只保留选中和悬停背景，避免重复叠字。
class GroupRowDelegateLJY : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option,index);
        option->text.clear();
    }
};
// 群列表用独立红色数字角标，名称和摘要保持原来的阅读颜色。
inline void updateGroupRowLJY(QListWidget* list, QListWidgetItem* item, int count)
{
    auto* row = list->itemWidget(item);
    if (!row) {
        row = new QWidget;
        auto* box = new QVBoxLayout(row); box->setContentsMargins(8,8,8,8); box->setSpacing(3);
        auto* top = new QHBoxLayout; top->setSpacing(4);
        auto* name = new QLabel; name->setObjectName("groupListName"); name->setTextFormat(Qt::PlainText);
        name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); top->addWidget(name,1);
        auto* badge = new QLabel; badge->setObjectName("groupUnread"); badge->setAlignment(Qt::AlignCenter);
        styleUnreadBadgeLJY(badge);
        top->addWidget(badge); box->addLayout(top);
        auto* number = new QLabel; number->setObjectName("groupListNumber"); number->setProperty("muted",true); box->addWidget(number);
        auto* preview = new QLabel; preview->setObjectName("groupListPreview"); preview->setTextFormat(Qt::PlainText);
        preview->setProperty("muted",true); preview->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); box->addWidget(preview);
        item->setSizeHint(QSize(0,92)); list->setItemWidget(item,row);
    }
    const auto title=item->data(Qt::UserRole+1).toString().section('\n',0,0);
    auto* name=row->findChild<QLabel*>("groupListName"); name->setText(title); name->setToolTip(title);
    row->findChild<QLabel*>("groupListNumber")->setText("群号 "+item->data(Qt::UserRole).toString());
    auto* preview=row->findChild<QLabel*>("groupListPreview");
    const auto text=item->data(Qt::UserRole+12).toString(); preview->setText(preview->fontMetrics().elidedText(text,Qt::ElideRight,std::max(80,list->viewport()->width()-28))); preview->setToolTip(text);
    auto* badge=row->findChild<QLabel*>("groupUnread"); badge->setText(count>99 ? "99+" : QString::number(count)); badge->setVisible(count>0);
    item->setData(Qt::UserRole+13,count);
}
