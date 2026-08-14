#include "selection_coordinate_mapper.h"

#include <QGuiApplication>
#include <QLineF>
#include <QScreen>
#include <QtMath>

#include <limits>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString normalizedDeviceName(const QString &value)
{
    QString name = value.trimmed().toLower();
    const QString prefix = QStringLiteral("\\\\.\\");
    if (name.startsWith(prefix)) {
        name.remove(0, prefix.size());
    }
    return name;
}

qint64 squaredDistance(const QPoint &left, const QPoint &right)
{
    const qint64 dx = qint64(left.x()) - qint64(right.x());
    const qint64 dy = qint64(left.y()) - qint64(right.y());
    return dx * dx + dy * dy;
}

int mappedOffset(int value, int physicalExtent, int logicalExtent)
{
    if (physicalExtent <= 0 || logicalExtent <= 0) {
        return value;
    }
    return qFloor(
        (double(value) * double(logicalExtent))
        / double(physicalExtent)
    );
}

int mappedExtent(int value, int physicalExtent, int logicalExtent)
{
    if (physicalExtent <= 0 || logicalExtent <= 0) {
        return value;
    }
    return qMax(
        1,
        qRound(
            (double(value) * double(logicalExtent))
            / double(physicalExtent)
        )
    );
}

#ifdef Q_OS_WIN
struct PhysicalMonitor
{
    QString deviceName;
    QRect geometry;
};

BOOL CALLBACK collectMonitor(
    HMONITOR monitor,
    HDC,
    LPRECT,
    LPARAM data)
{
    QVector<PhysicalMonitor> *monitors =
        reinterpret_cast<QVector<PhysicalMonitor> *>(data);
    if (!monitors) {
        return TRUE;
    }
    MONITORINFOEXW info;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }
    PhysicalMonitor value;
    value.deviceName = QString::fromWCharArray(info.szDevice);
    value.geometry = QRect(
        info.rcMonitor.left,
        info.rcMonitor.top,
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top
    );
    monitors->append(value);
    return TRUE;
}
#endif

QScreen *bestScreenForPhysicalMonitor(
    const QString &deviceName,
    const QRect &physicalGeometry,
    const QList<QScreen *> &screens)
{
    for (QScreen *screen : screens) {
        if (screen
            && selectionDisplayDeviceNamesMatch(
                deviceName,
                screen->name()
            )) {
            return screen;
        }
    }

    QScreen *best = nullptr;
    qint64 bestOverlap = -1;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (QScreen *screen : screens) {
        if (!screen) {
            continue;
        }
        const QRect overlap = physicalGeometry.intersected(screen->geometry());
        const qint64 area = qint64(overlap.width()) * overlap.height();
        const qint64 distance = squaredDistance(
            physicalGeometry.center(),
            screen->geometry().center()
        );
        if (area > bestOverlap
            || (area == bestOverlap && distance < bestDistance)) {
            best = screen;
            bestOverlap = area;
            bestDistance = distance;
        }
    }
    return best;
}

} // namespace

bool selectionDisplayDeviceNamesMatch(
    const QString &left,
    const QString &right)
{
    const QString normalizedLeft = normalizedDeviceName(left);
    return !normalizedLeft.isEmpty()
        && normalizedLeft == normalizedDeviceName(right);
}

QPoint selectionPhysicalToLogical(
    const QPoint &physicalPoint,
    const SelectionMonitorGeometry &monitor)
{
    if (!monitor.physicalGeometry.isValid()
        || !monitor.logicalGeometry.isValid()) {
        return physicalPoint;
    }
    return QPoint(
        monitor.logicalGeometry.left()
            + mappedOffset(
                physicalPoint.x() - monitor.physicalGeometry.left(),
                monitor.physicalGeometry.width(),
                monitor.logicalGeometry.width()
            ),
        monitor.logicalGeometry.top()
            + mappedOffset(
                physicalPoint.y() - monitor.physicalGeometry.top(),
                monitor.physicalGeometry.height(),
                monitor.logicalGeometry.height()
            )
    );
}

QRect selectionPhysicalToLogical(
    const QRect &physicalRect,
    const SelectionMonitorGeometry &monitor)
{
    if (!physicalRect.isValid()) {
        return QRect();
    }
    return QRect(
        selectionPhysicalToLogical(physicalRect.topLeft(), monitor),
        QSize(
            mappedExtent(
                physicalRect.width(),
                monitor.physicalGeometry.width(),
                monitor.logicalGeometry.width()
            ),
            mappedExtent(
                physicalRect.height(),
                monitor.physicalGeometry.height(),
                monitor.logicalGeometry.height()
            )
        )
    );
}

SelectionMonitorGeometry selectionMonitorForPhysicalPoint(
    const QPoint &physicalPoint,
    const QVector<SelectionMonitorGeometry> &monitors)
{
    SelectionMonitorGeometry best;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (const SelectionMonitorGeometry &monitor : monitors) {
        if (monitor.physicalGeometry.contains(physicalPoint)) {
            return monitor;
        }
        const qint64 distance = squaredDistance(
            monitor.physicalGeometry.center(),
            physicalPoint
        );
        if (distance < bestDistance) {
            best = monitor;
            bestDistance = distance;
        }
    }
    return best;
}

QVector<SelectionMonitorGeometry> selectionMonitorGeometries()
{
    QVector<SelectionMonitorGeometry> result;
    const QList<QScreen *> screens = QGuiApplication::screens();
#ifdef Q_OS_WIN
    QVector<PhysicalMonitor> physicalMonitors;
    EnumDisplayMonitors(
        nullptr,
        nullptr,
        collectMonitor,
        reinterpret_cast<LPARAM>(&physicalMonitors)
    );
    for (const PhysicalMonitor &physical : physicalMonitors) {
        const QScreen *screen = bestScreenForPhysicalMonitor(
            physical.deviceName,
            physical.geometry,
            screens
        );
        SelectionMonitorGeometry monitor;
        monitor.deviceName = physical.deviceName;
        monitor.physicalGeometry = physical.geometry;
        monitor.logicalGeometry = screen
            ? screen->geometry()
            : physical.geometry;
        monitor.logicalAvailableGeometry = screen
            ? screen->availableGeometry()
            : physical.geometry;
        result.append(monitor);
    }
#else
    for (QScreen *screen : screens) {
        if (!screen) {
            continue;
        }
        SelectionMonitorGeometry monitor;
        monitor.deviceName = screen->name();
        monitor.physicalGeometry = screen->geometry();
        monitor.logicalGeometry = screen->geometry();
        monitor.logicalAvailableGeometry = screen->availableGeometry();
        result.append(monitor);
    }
#endif
    return result;
}

SelectionSnapshot selectionSnapshotFromPhysicalProbe(
    const SelectionPhysicalProbeResult &physical,
    const QVector<SelectionMonitorGeometry> &monitors)
{
    SelectionSnapshot snapshot = physical.snapshotWithoutGeometry;
    snapshot.cursorPosition = physical.cursorPhysicalPosition;
    if (!monitors.isEmpty()) {
        snapshot.cursorPosition = selectionPhysicalToLogical(
            physical.cursorPhysicalPosition,
            selectionMonitorForPhysicalPoint(
                physical.cursorPhysicalPosition,
                monitors
            )
        );
    }
    for (const QRect &physicalRect : physical.physicalRectangles) {
        if (!physicalRect.isValid()) {
            continue;
        }
        const SelectionMonitorGeometry monitor =
            selectionMonitorForPhysicalPoint(
                physicalRect.center(),
                monitors
            );
        const QRect logicalRect = monitors.isEmpty()
            ? physicalRect
            : selectionPhysicalToLogical(physicalRect, monitor);
        if (logicalRect.isValid()) {
            snapshot.rectangles.append(logicalRect);
        }
    }
    snapshot.anchorRect = selectionAnchorRectangle(
        snapshot.rectangles,
        snapshot.cursorPosition
    );
    return snapshot;
}
