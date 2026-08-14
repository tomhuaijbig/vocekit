#ifndef VOCEKIT_SELECTION_CONTEXT_PLACEMENT_H
#define VOCEKIT_SELECTION_CONTEXT_PLACEMENT_H

#include <QPoint>
#include <QRect>
#include <QSize>

struct SelectionSurfacePlacement
{
    QPoint toolbarTopLeft;
    QPoint cardTopLeft;
    bool toolbarAbove = false;
    bool cardAbove = false;
};

SelectionSurfacePlacement placeSelectionSurfaces(
    const QRect &anchorRect,
    const QPoint &cursorPosition,
    const QSize &toolbarSize,
    const QSize &cardSize,
    const QRect &availableGeometry,
    int gap
);

#endif // VOCEKIT_SELECTION_CONTEXT_PLACEMENT_H
