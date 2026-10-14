#pragma once
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include "gui/InlineDialogLJY.h"

// 测试只操作实际内置确认层，不给正式代码增加测试入口。
inline QWidget* activeDialogLJY()
{
    for (auto* top : QApplication::topLevelWidgets())
        if (auto* panel=top->findChild<QWidget*>("inlineDialog")) if(panel->isVisible()) return panel;
    return nullptr;
}
inline QPushButton* dialogButtonLJY(QWidget* panel,QMessageBox::StandardButton choice)
{
    int value=InlineDialogLJY::Cancel;
    if(choice==QMessageBox::Yes) value=InlineDialogLJY::Confirm;
    else if(choice==QMessageBox::Retry) value=InlineDialogLJY::Retry;
    else if(choice==QMessageBox::Discard) value=InlineDialogLJY::Discard;
    else if(choice==QMessageBox::Ok) value=InlineDialogLJY::Ok;
    return panel->findChild<QPushButton*>("dialogChoice"+QString::number(value));
}
inline void answerDialogLJY(QMessageBox::StandardButton choice)
{
    QTimer::singleShot(0,[choice]{if(auto* panel=activeDialogLJY()) if(auto* button=dialogButtonLJY(panel,choice)) button->click();});
}
