#ifndef VOCEKIT_SELECTION_COORDINATE_MAPPER_H
#define VOCEKIT_SELECTION_COORDINATE_MAPPER_H

#include "selection_snapshot.h"

struct SelectionMonitorGeometry
{
    QString deviceName;
    QRect physicalGeometry;
    QRect logicalGeometry;
    QRect logicalAvailableGeometry;
};

bool selectionDisplayDeviceNamesMatch(
    const QString &left,
    const QString &right
);
QPoint selectionPhysicalToLogical(
    const QPoint &physicalPoint,
    const SelectionMonitorGeometry &monitor
);
QRect selectionPhysicalToLogical(
    const QRect &physicalRect,
    const SelectionMonitorGeometry &monitor
);
SelectionMonitorGeometry selectionMonitorForPhysicalPoint(
    const QPoint &physicalPoint,
    const QVector<SelectionMonitorGeometry> &monitors
);
QVector<SelectionMonitorGeometry> selectionMonitorGeometries();
SelectionSnapshot selectionSnapshotFromPhysicalProbe(
    const SelectionPhysicalProbeResult &physical,
    const QVector<SelectionMonitorGeometry> &monitors
);

#endif // VOCEKIT_SELECTION_COORDINATE_MAPPER_H
