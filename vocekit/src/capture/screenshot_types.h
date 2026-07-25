#ifndef VOCEKIT_SCREENSHOT_TYPES_H
#define VOCEKIT_SCREENSHOT_TYPES_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

enum class ScreenshotSelectionHandle
{
    None,
    Move,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

QString screenshotTriggerPrimary();
QString screenshotTriggerSeparate();
QString screenshotTriggerLauncher();
QString screenshotTriggerSeparateAndLauncher();
QString normalizeScreenshotTriggerMode(const QString &value);

bool screenshotTriggerUsesPrimary(const QString &value);
bool screenshotTriggerUsesSeparate(const QString &value);
bool screenshotTriggerUsesLauncher(const QString &value);

QString screenshotHotkeyLogicalId(const QString &functionId);
bool parseScreenshotHotkeyLogicalId(
    const QString &logicalId,
    QString *functionId
);

QStringList mapScreenshotResultLines(
    const QString &resultText,
    int blockCount
);

QRect normalizedScreenshotSelection(
    const QPoint &first,
    const QPoint &second,
    const QSize &desktopSize
);
bool isValidScreenshotSelection(const QRect &selection);

ScreenshotSelectionHandle screenshotSelectionHandleAt(
    const QRect &selection,
    const QPoint &point,
    int hitRadius
);
QRect movedScreenshotSelection(
    const QRect &selection,
    const QPoint &delta,
    const QSize &desktopSize
);
QRect resizedScreenshotSelection(
    const QRect &selection,
    ScreenshotSelectionHandle handle,
    const QPoint &point,
    const QSize &desktopSize,
    int minimumSize
);

#endif // VOCEKIT_SCREENSHOT_TYPES_H
