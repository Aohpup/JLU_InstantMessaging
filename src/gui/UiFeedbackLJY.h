#pragma once

#include <QLabel>
#include <QGraphicsEffect>
#include <QPainter>
#include <QVariantAnimation>

// 只偏移警告条的绘制，不移动窗口或改变布局；结束时回到原位。
class FeedbackShakeEffectLJY : public QGraphicsEffect
{
public:
    explicit FeedbackShakeEffectLJY(QObject* parent) : QGraphicsEffect(parent) {}
    void setOffset(int value) { offset_ = value; update(); }
    int offset() const { return offset_; }
protected:
    QRectF boundingRectFor(const QRectF& rect) const override { return rect.adjusted(-6, 0, 6, 0); }
    void draw(QPainter* painter) override
    {
        // 先取得整条提示的图像，再偏移绘制；直接平移 drawSource 不会移动当前控件画面。
        QPoint origin;
        const auto pixmap = sourcePixmap(Qt::LogicalCoordinates, &origin, NoPad);
        painter->drawPixmap(origin + QPoint(offset_, 0), pixmap);
    }
private:
    int offset_ = 0;
};

// 保留原来的浅红警告条，失败时轻微左右震动约 320 毫秒；普通消息不动。
inline void setFeedbackLJY(QLabel* label, const QString& text, bool failed = false)
{
    auto* effect = static_cast<FeedbackShakeEffectLJY*>(label->graphicsEffect());
    if (!effect) {
        effect = new FeedbackShakeEffectLJY(label); label->setGraphicsEffect(effect);
        auto* animation = new QVariantAnimation(effect); animation->setObjectName("feedbackShake");
        animation->setDuration(320);
        animation->setKeyValues({{0.0,0},{0.15,-5},{0.3,5},{0.45,-4},{0.6,4},{0.8,-2},{1.0,0}});
        QObject::connect(animation,&QVariantAnimation::valueChanged,effect,[effect](const QVariant& value) { effect->setOffset(value.toInt()); });
    }
    auto* animation = effect->findChild<QVariantAnimation*>("feedbackShake");
    animation->stop(); effect->setOffset(0);
    label->setStyleSheet(failed
        ? "color: #b42318; background: #fdeaea; border: 1px solid #f1b5b5;"
          "border-left: 4px solid #d75e65; border-radius: 6px; padding: 8px 12px;"
        : "color: #202020; background: transparent; border: none; padding: 8px 12px;");
    label->setToolTip(text);
    label->setTextFormat(Qt::PlainText);
    label->setText(failed && !text.isEmpty() ? "！  " + text : text);
    if (failed && !text.isEmpty()) animation->start();
}
