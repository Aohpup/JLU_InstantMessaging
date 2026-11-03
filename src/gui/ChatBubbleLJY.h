#pragma once
#include <QFrame>
#include <QPainter>
#include <QPainterPath>

// 气泡和指向头像的三角合成一个轮廓，避免接缝；正文仍由普通 QLabel 显示。
class ChatBubbleLJY : public QFrame
{
public:
    explicit ChatBubbleLJY(bool outgoing) : outgoing_(outgoing) {}
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        const QRectF body(outgoing_ ? 0.5 : 8.5, 0.5, width() - 9.0, height() - 1.0);
        QPainterPath rounded; rounded.addRoundedRect(body, 12, 12);
        const double edge = outgoing_ ? body.right() : body.left();
        QPainterPath tail; tail.moveTo(edge, 12);
        tail.lineTo(edge + (outgoing_ ? 7 : -7), 18); tail.lineTo(edge, 24); tail.closeSubpath();
        painter.setPen(QColor(outgoing_ ? "#BFCBDC" : "#DCE0E7"));
        painter.setBrush(QColor(outgoing_ ? "#DFEBFC" : "#FCFDFF"));
        painter.drawPath(rounded.united(tail));
    }
private:
    bool outgoing_;
};
