#pragma once
#include <QWidget>
#include <QStringList>

// 主窗口内部的遮罩卡片，不创建独立系统弹窗；启动失败时使用同样样式的小窗口承载。
class InlineDialogLJY : public QWidget
{
public:
    enum Choice { Cancel, Confirm, Retry, Discard, Ok };
    // 默认选取消，关闭或 Escape 也按取消处理；保存失败可提供重试和放弃。
    static Choice ask(QWidget* parent, const QString& title, const QString& message,
                      const QList<Choice>& choices = {Confirm, Cancel});
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    explicit InlineDialogLJY(QWidget* host);
    QWidget* host_;
};
