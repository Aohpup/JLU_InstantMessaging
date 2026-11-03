#pragma once
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

// 按消息、群聊、账号、好友列表绘制透明图标；多尺寸保证缩放清晰，不依赖图片背景或图标字体。
inline QIcon sidebarIconLJY(int kind)
{
    QIcon icon;
    for (int size : {24, 32, 48, 64, 96}) {
        QPixmap image(size, size); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 32.0, size / 32.0);
        const QColor dark("#254E87"), accent("#3675D3");
        painter.setPen(Qt::NoPen); painter.setBrush(dark);
        if (kind == 0) {
            QPainterPath bubble;
            bubble.moveTo(7,24); bubble.cubicTo(1,18,3,6,12,4);
            bubble.cubicTo(24,0,33,13,27,23); bubble.cubicTo(23,29,15,30,10,27);
            bubble.lineTo(4,29); bubble.closeSubpath();
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(dark,2.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter.drawPath(bubble); painter.setPen(Qt::NoPen); painter.setBrush(accent);
            for (int x : {11,17,23}) painter.drawEllipse(QPointF(x,15),1.7,1.7);
        } else if (kind == 1) {
            QPainterPath bubble;
            bubble.addRoundedRect(QRectF(13,3,16,17),5,5);
            QPainterPath tail; tail.moveTo(24,17);tail.lineTo(30,23);tail.lineTo(28,15);tail.closeSubpath();
            painter.drawPath(bubble.united(tail));
            painter.drawEllipse(QRectF(3,9,7,7));
            QPainterPath rear;rear.addRoundedRect(QRectF(1,18,11,11),4,4);painter.drawPath(rear);
            // 前景人物的留白使用透明挖空，与侧栏和选中底色自然融合。
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.drawEllipse(QRectF(11,10,13,13));painter.drawRoundedRect(QRectF(7,21,24,12),6,6);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawEllipse(QRectF(13,12,9,9));painter.drawRoundedRect(QRectF(9,23,20,8),4,4);
            painter.setBrush(accent);painter.drawRoundedRect(QRectF(19,26,6,2),1,1);
        } else if (kind == 2) {
            painter.drawEllipse(QRectF(8,3,14,14));painter.drawRoundedRect(QRectF(3,20,20,11),5,5);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);painter.drawEllipse(QRectF(16,17,16,16));
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setBrush(dark);painter.drawEllipse(QRectF(17,18,14,14));
            painter.setBrush(accent);painter.drawEllipse(QRectF(19,20,10,10));
            painter.setPen(QPen(dark,2,Qt::SolidLine,Qt::RoundCap));
            painter.drawLine(QPointF(24,22),QPointF(24,28));painter.drawLine(QPointF(21,25),QPointF(27,25));
        } else {
            QPainterPath body, opening;body.addRoundedRect(QRectF(4,3,24,28),4,4);
            opening.addRoundedRect(QRectF(7,10,18,18),2,2);painter.drawPath(body.subtracted(opening));
            painter.setBrush(accent);painter.drawRoundedRect(QRectF(8,5,3,3),1,1);painter.drawRoundedRect(QRectF(14,5,3,3),1,1);
            for (int y : {14,19,24}) {
                painter.setBrush(y==19 ? accent : dark);painter.drawEllipse(QPointF(10,y),1.7,1.7);
                painter.setBrush(y==24 ? accent : dark);painter.drawRoundedRect(QRectF(14,y-1,8,2),1,1);
            }
        }
        painter.end(); icon.addPixmap(image);
    }
    return icon;
}
