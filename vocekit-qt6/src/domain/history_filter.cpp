#include "history_filter.h"

#include "history_modes.h"
#include "history_text.h"

bool historyEntryMatchesFilter(
    const HistoryEntry &entry,
    const HistoryFilter &filter
)
{
    const QString modeId = filter.modeId.trimmed().isEmpty()
        ? QStringLiteral("__all")
        : filter.modeId;
    if (!historyEntryMatchesModeId(entry, modeId, filter.customFunctions)) {
        return false;
    }
    return historyEntryMatchesSearchText(entry, filter.searchText);
}

QVector<HistoryEntry> filterHistoryEntries(
    const QVector<HistoryEntry> &entries,
    const HistoryFilter &filter
)
{
    QVector<HistoryEntry> filtered;
    filtered.reserve(entries.size());
    for (const HistoryEntry &entry : entries) {
        if (historyEntryMatchesFilter(entry, filter)) {
            filtered.append(entry);
        }
    }
    return filtered;
}

QVector<int> historyEntryIndexesMatchingFilter(
    const QVector<HistoryEntry> &entries,
    const HistoryFilter &filter
)
{
    QVector<int> indexes;
    indexes.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        if (historyEntryMatchesFilter(entries.at(i), filter)) {
            indexes.append(i);
        }
    }
    return indexes;
}
