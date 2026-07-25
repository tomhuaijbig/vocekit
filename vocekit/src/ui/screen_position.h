#ifndef VOCEKIT_SCREEN_POSITION_H
#define VOCEKIT_SCREEN_POSITION_H

#include <QtWidgets>

// 将窗口左上角限制在当前屏幕可见区域内，避免保存过的位置跑到屏幕外。
inline QPoint clampedTopLeftToScreen(const QPoint &topLeft, const QSize &size)
{
    int screenNumber = QApplication::desktop()->screenNumber(topLeft);
    if (screenNumber < 0) {
        screenNumber = QApplication::desktop()->screenNumber(QCursor::pos());
    }
    const QRect screen = QApplication::desktop()->availableGeometry(screenNumber);
    const int maxX = qMax(screen.left(), screen.right() - size.width() + 1);
    const int maxY = qMax(screen.top(), screen.bottom() - size.height() + 1);
    return QPoint(
        qBound(screen.left(), topLeft.x(), maxX),
        qBound(screen.top(), topLeft.y(), maxY)
    );
}

#endif // VOCEKIT_SCREEN_POSITION_H
