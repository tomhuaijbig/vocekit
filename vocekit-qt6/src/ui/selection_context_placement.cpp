#include "selection_context_placement.h"

#include <QVector>

namespace {

struct ToolbarCandidate
{
    QPoint topLeft;
    bool above = false;
};

int clampedCoordinate(int value, int minimum, int extent, int size)
{
    if (size >= extent) {
        return minimum;
    }
    return qBound(minimum, value, minimum + extent - size);
}

QPoint clampTopLeft(
    const QPoint &topLeft,
    const QSize &size,
    const QRect &available)
{
    if (!available.isValid()) {
        return topLeft;
    }
    return QPoint(
        clampedCoordinate(
            topLeft.x(),
            available.left(),
            available.width(),
            size.width()
        ),
        clampedCoordinate(
            topLeft.y(),
            available.top(),
            available.height(),
            size.height()
        )
    );
}

bool verticallyFits(
    int top,
    int height,
    const QRect &available)
{
    return height <= available.height()
        && top >= available.top()
        && top + height <= available.top() + available.height();
}

bool surfaceFits(
    const QPoint &topLeft,
    const QSize &size,
    const QRect &available)
{
    return size.width() <= available.width()
        && size.height() <= available.height()
        && available.contains(QRect(topLeft, size));
}

} // namespace

SelectionSurfacePlacement placeSelectionSurfaces(
    const QRect &anchorRect,
    const QPoint &cursorPosition,
    const QSize &toolbarSize,
    const QSize &cardSize,
    const QRect &availableGeometry,
    int gap)
{
    SelectionSurfacePlacement result;
    if (!availableGeometry.isValid()) {
        return result;
    }
    const int spacing = qMax(0, gap);
    if (toolbarSize.width() > availableGeometry.width()
        || toolbarSize.height() > availableGeometry.height()) {
        result.toolbarTopLeft = availableGeometry.topLeft();
        result.cardTopLeft = availableGeometry.topLeft();
        return result;
    }

    QVector<ToolbarCandidate> candidates;
    if (anchorRect.isValid()) {
        const int anchorCenterX =
            anchorRect.left() + anchorRect.width() / 2;
        const int anchorX = anchorCenterX - toolbarSize.width() / 2;
        ToolbarCandidate below;
        below.topLeft = QPoint(
            anchorX,
            anchorRect.top() + anchorRect.height() + spacing
        );
        candidates.append(below);

        ToolbarCandidate above;
        above.topLeft = QPoint(
            anchorX,
            anchorRect.top() - spacing - toolbarSize.height()
        );
        above.above = true;
        candidates.append(above);
    }

    ToolbarCandidate cursorBelow;
    cursorBelow.topLeft = QPoint(
        cursorPosition.x() + spacing,
        cursorPosition.y() + spacing
    );
    candidates.append(cursorBelow);

    ToolbarCandidate cursorAbove;
    cursorAbove.topLeft = QPoint(
        cursorPosition.x() + spacing,
        cursorPosition.y() - spacing - toolbarSize.height()
    );
    cursorAbove.above = true;
    candidates.append(cursorAbove);

    ToolbarCandidate selected = candidates.constFirst();
    bool found = false;
    for (const ToolbarCandidate &candidate : candidates) {
        ToolbarCandidate clamped = candidate;
        clamped.topLeft.setX(clampedCoordinate(
            clamped.topLeft.x(),
            availableGeometry.left(),
            availableGeometry.width(),
            toolbarSize.width()
        ));
        if (verticallyFits(
                clamped.topLeft.y(),
                toolbarSize.height(),
                availableGeometry)) {
            selected = clamped;
            found = true;
            break;
        }
    }
    if (!found) {
        selected.topLeft = clampTopLeft(
            selected.topLeft,
            toolbarSize,
            availableGeometry
        );
    }
    result.toolbarTopLeft = selected.topLeft;
    result.toolbarAbove = selected.above;

    if (cardSize.width() > availableGeometry.width()
        || cardSize.height() > availableGeometry.height()) {
        result.cardTopLeft = availableGeometry.topLeft();
        return result;
    }
    const int cardX = result.toolbarTopLeft.x()
        + (toolbarSize.width() - cardSize.width()) / 2;
    const QPoint belowCard(
        cardX,
        result.toolbarTopLeft.y() + toolbarSize.height() + spacing
    );
    const QPoint clampedBelow = clampTopLeft(
        belowCard,
        cardSize,
        availableGeometry
    );
    if (surfaceFits(clampedBelow, cardSize, availableGeometry)
        && clampedBelow.y() == belowCard.y()) {
        result.cardTopLeft = clampedBelow;
        return result;
    }

    result.cardAbove = true;
    const QPoint aboveCard(
        cardX,
        result.toolbarTopLeft.y() - spacing - cardSize.height()
    );
    result.cardTopLeft = clampTopLeft(
        aboveCard,
        cardSize,
        availableGeometry
    );
    return result;
}
