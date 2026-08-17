#ifndef VOCEKIT_SCREEN_POSITION_H
#define VOCEKIT_SCREEN_POSITION_H

#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>

inline QRect availableScreenGeometryAt(const QPoint &point)
{
    QScreen *screen = QGuiApplication::screenAt(point);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen ? screen->availableGeometry() : QRect();
}

inline QRect primaryAvailableScreenGeometry()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect();
}

// 将窗口左上角限制在当前屏幕可见区域内，避免保存过的位置跑到屏幕外。
inline QPoint clampedTopLeftToScreen(const QPoint &topLeft, const QSize &size)
{
    const QRect screen = availableScreenGeometryAt(topLeft);
    if (!screen.isValid()) {
        return topLeft;
    }
    const int maxX = qMax(screen.left(), screen.right() - size.width() + 1);
    const int maxY = qMax(screen.top(), screen.bottom() - size.height() + 1);
    return QPoint(
        qBound(screen.left(), topLeft.x(), maxX),
        qBound(screen.top(), topLeft.y(), maxY)
    );
}

#endif // VOCEKIT_SCREEN_POSITION_H
