#ifndef VOCEKIT_HISTORY_FILTER_H
#define VOCEKIT_HISTORY_FILTER_H

#include "app_legacy_types.h"
#include "history_types.h"

#include <QString>
#include <QVector>

struct HistoryFilter
{
    QString modeId = QStringLiteral("__all");
    QString searchText;
    QVector<CustomFunctionDef> customFunctions;
};

bool historyEntryMatchesFilter(
    const HistoryEntry &entry,
    const HistoryFilter &filter
);
QVector<HistoryEntry> filterHistoryEntries(
    const QVector<HistoryEntry> &entries,
    const HistoryFilter &filter
);
QVector<int> historyEntryIndexesMatchingFilter(
    const QVector<HistoryEntry> &entries,
    const HistoryFilter &filter
);

#endif // VOCEKIT_HISTORY_FILTER_H
