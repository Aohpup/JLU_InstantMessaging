#pragma once
#include <QFrame>
#include <QGraphicsDropShadowEffect>

// 卡片和侧栏身份按钮统一使用柔和投影；只绘制阴影，正文保持清晰，底色由 theme.qss 管理。
inline void applyCardSurfaceLJY(QWidget* card)
{
    card->setProperty("surfaceCard", true);
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(38, 54, 74, 24));
    card->setGraphicsEffect(shadow);
}
