#ifndef VOCEKIT_HISTORY_MODES_H
#define VOCEKIT_HISTORY_MODES_H

#include "app_legacy_types.h"
#include "history_types.h"

#include <QString>
#include <QVector>

QString historyBuiltinModeId(const QString &modeTitle);
QString historyEntryEffectiveModeId(
    const HistoryEntry &entry,
    const QVector<CustomFunctionDef> &customFunctions
);
bool historyEntryMatchesModeId(
    const HistoryEntry &entry,
    const QString &modeId,
    const QVector<CustomFunctionDef> &customFunctions
);
QVector<HistoryTabDef> buildHistoryTabModes(
    const QVector<CustomFunctionDef> &customFunctions
);

#endif // VOCEKIT_HISTORY_MODES_H
