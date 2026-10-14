#include "gui/InlineDialogLJY.h"
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QFrame>
#include <algorithm>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

InlineDialogLJY::InlineDialogLJY(QWidget* host) : QWidget(host), host_(host)
{
    setObjectName("inlineDialog");
    setAttribute(Qt::WA_StyledBackground);
    setGeometry(host->rect());
    qApp->installEventFilter(this);
}
bool InlineDialogLJY::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==host_ && event->type()==QEvent::Resize) setGeometry(host_->rect());
    auto cancel=[this] {
        auto* button=findChild<QPushButton*>("dialogChoice0");
        if (!button) button=findChild<QPushButton*>("dialogChoice4");
        if (button) button->click();
    };
    // 确认期间关闭窗口先取消当前卡片，避免未完成的操作在关闭后继续执行。
    if (watched==host_->window() && event->type()==QEvent::Close) { cancel(); event->ignore(); return true; }
    if (event->type()==QEvent::KeyPress)
    {
        auto* key=static_cast<QKeyEvent*>(event);
        if (key->key()==Qt::Key_Escape) { cancel(); return true; }
        // Tab 只在卡片按钮之间循环，Enter 操作当前按钮，不触发背景页面。
        const auto buttons=findChildren<QPushButton*>();
        if ((key->key()==Qt::Key_Tab || key->key()==Qt::Key_Backtab) && !buttons.isEmpty())
        {
            int index=buttons.indexOf(qobject_cast<QPushButton*>(QApplication::focusWidget()));
            const int direction=(key->key()==Qt::Key_Backtab || key->modifiers().testFlag(Qt::ShiftModifier)) ? -1 : 1;
            index=(index+direction+buttons.size())%buttons.size(); buttons[index]->setFocus(); return true;
        }
        if (key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter)
            if (auto* button=qobject_cast<QPushButton*>(QApplication::focusWidget()))
                if (isAncestorOf(button)) { button->click(); return true; }
        auto* target=qobject_cast<QWidget*>(watched);
        if (target && target->window()==host_->window() && target!=this && !isAncestorOf(target)) return true;
    }
    return QWidget::eventFilter(watched,event);
}
InlineDialogLJY::Choice InlineDialogLJY::ask(QWidget* parent, const QString& title, const QString& message, const QList<Choice>& choices)
{
    Q_INIT_RESOURCE(gui_assets);
    QWidget startupHost;
    QWidget* host=parent ? parent->window() : &startupHost;
    if (auto* main=qobject_cast<QMainWindow*>(host)) host=main->centralWidget();
    if (!parent)
    {
        startupHost.resize(660,440); startupHost.setWindowTitle("JLU 即时通信");
        startupHost.setWindowIcon(QIcon(":/gui/app.png"));
        QFile theme(":/gui/theme.qss"); if(theme.open(QIODevice::ReadOnly)) startupHost.setStyleSheet(QString::fromUtf8(theme.readAll()));
        startupHost.show();
    }
    InlineDialogLJY panel(host);
    auto* layout=new QVBoxLayout(&panel);
    layout->setContentsMargins(28,28,28,28);
    layout->addStretch();
    auto* card=new QFrame;
    card->setObjectName("dialogCard"); card->setMaximumWidth(520); card->setMinimumWidth(std::min(480,host->width()-56));
    auto* box=new QVBoxLayout(card); box->setContentsMargins(24,22,24,22); box->setSpacing(18);
    auto* heading=new QLabel(title); heading->setObjectName("dialogTitle"); heading->setTextFormat(Qt::PlainText); box->addWidget(heading);
    auto* body=new QLabel(message); body->setObjectName("dialogMessage"); body->setTextFormat(Qt::PlainText); body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (message.size() <= 300) box->addWidget(body);
    else {
    auto* scroll=new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setWidget(body);
    scroll->setMinimumHeight(160); scroll->setMaximumHeight(220); box->addWidget(scroll);
    }
    auto* actions=new QHBoxLayout; actions->setSpacing(10); actions->addStretch(); box->addLayout(actions);
    const QStringList labels{"取消", "确认", "重试保存", "放弃修改并退出", "知道了"};
    int buttonWidth = 100;
    for (auto choice : choices) buttonWidth = std::max(buttonWidth, panel.fontMetrics().horizontalAdvance(labels[choice]) + 28);
    Choice result=Cancel; QEventLoop loop; QPushButton* focus=nullptr;
    for (auto choice : choices)
    {
        auto* button=new QPushButton(labels[choice]); button->setObjectName("dialogChoice"+QString::number(choice));
        button->setProperty("primary",choice==Confirm || choice==Retry || choice==Ok);
        button->setFixedSize(buttonWidth,40); button->setStyleSheet("padding: 0;");
        if (choice==Discard) button->setProperty("coral",true);
        actions->addWidget(button);
        QObject::connect(button,&QPushButton::clicked,&panel,[&,choice]{result=choice;loop.quit();});
        if (!focus || choice==Cancel) focus=button;
    }
    layout->addWidget(card,0,Qt::AlignHCenter); layout->addStretch();
    panel.show(); panel.raise(); if (focus) focus->setFocus();
    loop.exec(); qApp->removeEventFilter(&panel); panel.hide();
    return result;
}
