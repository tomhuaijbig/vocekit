#include "tab_bar_wheel_filter.h"

#include <QEvent>
#include <QTabBar>
#include <QWheelEvent>
#include <QtGlobal>

TabBarWheelFilter::TabBarWheelFilter(QObject *parent)
    : QObject(parent)
{
}

bool TabBarWheelFilter::eventFilter(QObject *watched, QEvent *event)
{
    auto *tabBar = qobject_cast<QTabBar *>(watched);
    if (!tabBar || event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }

    auto *wheel = static_cast<QWheelEvent *>(event);
    const QPoint delta = wheel->angleDelta();
    const int amount = qAbs(delta.x()) > qAbs(delta.y()) ? delta.x() : delta.y();
    if (amount == 0) {
        return QObject::eventFilter(watched, event);
    }

    const int direction = amount < 0 ? 1 : -1;
    const int nextIndex = qBound(
        0,
        tabBar->currentIndex() + direction,
        tabBar->count() - 1
    );
    if (nextIndex == tabBar->currentIndex()) {
        return QObject::eventFilter(watched, event);
    }

    tabBar->setCurrentIndex(nextIndex);
    event->accept();
    return true;
}
