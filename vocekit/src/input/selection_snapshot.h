#ifndef VOCEKIT_SELECTION_SNAPSHOT_H
#define VOCEKIT_SELECTION_SNAPSHOT_H

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

using SelectedTextNativeWindowHandle = void *;

enum class SelectionAcquisitionMethod
{
    None,
    UiAutomation,
    ClipboardFallback
};

enum class SelectionSensitivity
{
    Normal,
    Password,
    Protected,
    PermissionDenied,
    SecureDesktop
};

struct SelectionSnapshot
{
    quint64 generation = 0;
    QString text;
    QVector<QRect> rectangles;
    QRect anchorRect;
    QPoint cursorPosition;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
    quint32 targetProcessId = 0;
    QString targetExecutable;
    SelectionAcquisitionMethod method = SelectionAcquisitionMethod::None;
    SelectionSensitivity sensitivity = SelectionSensitivity::Normal;

    bool isUsable() const
    {
        return sensitivity == SelectionSensitivity::Normal
            && !text.trimmed().isEmpty();
    }
};

struct SelectionProbeRequest
{
    SelectedTextNativeWindowHandle targetWindow = nullptr;
    QPoint cursorPhysicalPosition;
};

struct SelectionPhysicalProbeResult
{
    SelectionSnapshot snapshotWithoutGeometry;
    QVector<QRect> physicalRectangles;
    QPoint cursorPhysicalPosition;
};

QRect selectionAnchorRectangle(
    const QVector<QRect> &rectangles,
    const QPoint &cursorPosition
);
QVector<QRect> selectionRectanglesFromFlatBounds(
    const QVector<double> &values
);
bool selectionInputDesktopIsSecure(
    bool inputDesktopOpened,
    const QString &desktopName
);
bool selectionClipboardOwnershipMatches(
    quint32 expectedSequence,
    quint32 currentSequence,
    quint32 targetProcessId,
    quint32 clipboardOwnerProcessId,
    bool targetStillForeground
);

#endif // VOCEKIT_SELECTION_SNAPSHOT_H
