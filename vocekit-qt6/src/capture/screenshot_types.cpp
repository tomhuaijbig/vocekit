#include "screenshot_types.h"

#include <QRegularExpression>

QString screenshotTriggerPrimary()
{
    return QStringLiteral("primary");
}

QString screenshotTriggerSeparate()
{
    return QStringLiteral("separate");
}

QString screenshotTriggerLauncher()
{
    return QStringLiteral("launcher");
}

QString screenshotTriggerSeparateAndLauncher()
{
    return QStringLiteral("separateAndLauncher");
}

QString normalizeScreenshotTriggerMode(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == screenshotTriggerPrimary()
        || normalized == screenshotTriggerSeparate()
        || normalized == screenshotTriggerLauncher()
        || normalized == screenshotTriggerSeparateAndLauncher()) {
        return normalized;
    }
    return screenshotTriggerSeparate();
}

bool screenshotTriggerUsesPrimary(const QString &value)
{
    return normalizeScreenshotTriggerMode(value) == screenshotTriggerPrimary();
}

bool screenshotTriggerUsesSeparate(const QString &value)
{
    const QString normalized = normalizeScreenshotTriggerMode(value);
    return normalized == screenshotTriggerSeparate()
        || normalized == screenshotTriggerSeparateAndLauncher();
}

bool screenshotTriggerUsesLauncher(const QString &value)
{
    const QString normalized = normalizeScreenshotTriggerMode(value);
    return normalized == screenshotTriggerLauncher()
        || normalized == screenshotTriggerSeparateAndLauncher();
}

QString screenshotHotkeyLogicalId(const QString &functionId)
{
    return QStringLiteral("screenshot:") + functionId.trimmed();
}

bool parseScreenshotHotkeyLogicalId(
    const QString &logicalId,
    QString *functionId)
{
    const QString prefix = QStringLiteral("screenshot:");
    if (!logicalId.startsWith(prefix)) {
        return false;
    }
    const QString parsed = logicalId.mid(prefix.size()).trimmed();
    if (parsed.isEmpty()) {
        return false;
    }
    if (functionId) {
        *functionId = parsed;
    }
    return true;
}

QStringList mapScreenshotResultLines(
    const QString &resultText,
    int blockCount)
{
    if (blockCount <= 0) {
        return QStringList();
    }

    QStringList lines;
    const QStringList sourceLines = resultText.split(
        QRegularExpression(QStringLiteral("[\\r\\n]+")),
        Qt::KeepEmptyParts
    );
    for (const QString &line : sourceLines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            lines.append(trimmed);
        }
    }
    return lines.size() == blockCount ? lines : QStringList();
}

QRect normalizedScreenshotSelection(
    const QPoint &first,
    const QPoint &second,
    const QSize &desktopSize)
{
    const QRect desktopRect(QPoint(0, 0), desktopSize);
    if (!desktopRect.isValid()) {
        return QRect();
    }

    const QPoint clampedFirst(
        qBound(desktopRect.left(), first.x(), desktopRect.right()),
        qBound(desktopRect.top(), first.y(), desktopRect.bottom())
    );
    const QPoint clampedSecond(
        qBound(desktopRect.left(), second.x(), desktopRect.right()),
        qBound(desktopRect.top(), second.y(), desktopRect.bottom())
    );
    const QPoint topLeft(
        qMin(clampedFirst.x(), clampedSecond.x()),
        qMin(clampedFirst.y(), clampedSecond.y())
    );
    const QPoint bottomRight(
        qMax(clampedFirst.x(), clampedSecond.x()),
        qMax(clampedFirst.y(), clampedSecond.y())
    );
    return QRect(topLeft, bottomRight);
}

bool isValidScreenshotSelection(const QRect &selection)
{
    return selection.width() >= 8 && selection.height() >= 8;
}

ScreenshotSelectionHandle screenshotSelectionHandleAt(
    const QRect &selection,
    const QPoint &point,
    int hitRadius)
{
    if (!selection.isValid()) {
        return ScreenshotSelectionHandle::None;
    }

    const int radius = qMax(2, hitRadius);
    const auto contains = [point, radius](const QPoint &center) {
        return QRect(
            center.x() - radius,
            center.y() - radius,
            radius * 2 + 1,
            radius * 2 + 1
        ).contains(point);
    };
    const QPoint topCenter(selection.center().x(), selection.top());
    const QPoint rightCenter(selection.right(), selection.center().y());
    const QPoint bottomCenter(selection.center().x(), selection.bottom());
    const QPoint leftCenter(selection.left(), selection.center().y());

    if (contains(selection.topLeft())) {
        return ScreenshotSelectionHandle::TopLeft;
    }
    if (contains(topCenter)) {
        return ScreenshotSelectionHandle::Top;
    }
    if (contains(selection.topRight())) {
        return ScreenshotSelectionHandle::TopRight;
    }
    if (contains(rightCenter)) {
        return ScreenshotSelectionHandle::Right;
    }
    if (contains(selection.bottomRight())) {
        return ScreenshotSelectionHandle::BottomRight;
    }
    if (contains(bottomCenter)) {
        return ScreenshotSelectionHandle::Bottom;
    }
    if (contains(selection.bottomLeft())) {
        return ScreenshotSelectionHandle::BottomLeft;
    }
    if (contains(leftCenter)) {
        return ScreenshotSelectionHandle::Left;
    }
    return selection.contains(point)
        ? ScreenshotSelectionHandle::Move
        : ScreenshotSelectionHandle::None;
}

QRect movedScreenshotSelection(
    const QRect &selection,
    const QPoint &delta,
    const QSize &desktopSize)
{
    if (!selection.isValid() || desktopSize.isEmpty()) {
        return QRect();
    }

    QRect moved = selection.translated(delta);
    const int maximumX = qMax(0, desktopSize.width() - moved.width());
    const int maximumY = qMax(0, desktopSize.height() - moved.height());
    moved.moveLeft(qBound(0, moved.left(), maximumX));
    moved.moveTop(qBound(0, moved.top(), maximumY));
    return moved;
}

QRect resizedScreenshotSelection(
    const QRect &selection,
    ScreenshotSelectionHandle handle,
    const QPoint &point,
    const QSize &desktopSize,
    int minimumSize)
{
    if (!selection.isValid() || desktopSize.isEmpty()) {
        return QRect();
    }
    if (handle == ScreenshotSelectionHandle::Move) {
        return movedScreenshotSelection(
            selection,
            point - selection.topLeft(),
            desktopSize
        );
    }
    if (handle == ScreenshotSelectionHandle::None) {
        return selection;
    }

    const QRect desktop(QPoint(0, 0), desktopSize);
    const QPoint bounded(
        qBound(desktop.left(), point.x(), desktop.right()),
        qBound(desktop.top(), point.y(), desktop.bottom())
    );
    const int minSize = qMax(8, minimumSize);
    int left = selection.left();
    int top = selection.top();
    int right = selection.right();
    int bottom = selection.bottom();

    const bool changesLeft =
        handle == ScreenshotSelectionHandle::TopLeft
        || handle == ScreenshotSelectionHandle::Left
        || handle == ScreenshotSelectionHandle::BottomLeft;
    const bool changesRight =
        handle == ScreenshotSelectionHandle::TopRight
        || handle == ScreenshotSelectionHandle::Right
        || handle == ScreenshotSelectionHandle::BottomRight;
    const bool changesTop =
        handle == ScreenshotSelectionHandle::TopLeft
        || handle == ScreenshotSelectionHandle::Top
        || handle == ScreenshotSelectionHandle::TopRight;
    const bool changesBottom =
        handle == ScreenshotSelectionHandle::BottomLeft
        || handle == ScreenshotSelectionHandle::Bottom
        || handle == ScreenshotSelectionHandle::BottomRight;

    if (changesLeft) {
        left = qMin(bounded.x(), right - minSize + 1);
    }
    if (changesRight) {
        right = qMax(bounded.x(), left + minSize - 1);
    }
    if (changesTop) {
        top = qMin(bounded.y(), bottom - minSize + 1);
    }
    if (changesBottom) {
        bottom = qMax(bounded.y(), top + minSize - 1);
    }

    left = qBound(desktop.left(), left, desktop.right());
    top = qBound(desktop.top(), top, desktop.bottom());
    right = qBound(desktop.left(), right, desktop.right());
    bottom = qBound(desktop.top(), bottom, desktop.bottom());
    return QRect(QPoint(left, top), QPoint(right, bottom));
}
