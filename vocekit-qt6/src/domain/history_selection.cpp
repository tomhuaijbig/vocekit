#include "history_selection.h"

#include <QtGlobal>

void HistorySelectionState::clear()
{
    m_filePaths.clear();
}

bool HistorySelectionState::isEmpty() const
{
    return m_filePaths.isEmpty();
}

int HistorySelectionState::count() const
{
    return m_filePaths.size();
}

bool HistorySelectionState::containsFilePath(const QString &filePath) const
{
    return m_filePaths.contains(normalizedFilePath(filePath));
}

bool HistorySelectionState::containsEntry(const HistoryEntry &entry) const
{
    return containsFilePath(entry.filePath);
}

void HistorySelectionState::setFileSelected(const QString &filePath, bool selected)
{
    const QString normalized = normalizedFilePath(filePath);
    if (normalized.isEmpty()) {
        return;
    }

    if (selected) {
        m_filePaths.insert(normalized);
    } else {
        m_filePaths.remove(normalized);
    }
}

void HistorySelectionState::setEntrySelected(const HistoryEntry &entry, bool selected)
{
    setFileSelected(entry.filePath, selected);
}

void HistorySelectionState::selectEntries(const QVector<HistoryEntry> &entries)
{
    for (const HistoryEntry &entry : entries) {
        setEntrySelected(entry, true);
    }
}

QStringList HistorySelectionState::selectedFilePaths() const
{
    return m_filePaths.values();
}

QVector<HistoryEntry> HistorySelectionState::selectedEntriesFrom(const QVector<HistoryEntry> &entries) const
{
    QVector<HistoryEntry> selected;
    selected.reserve(qMin(entries.size(), m_filePaths.size()));
    for (const HistoryEntry &entry : entries) {
        if (containsEntry(entry)) {
            selected.append(entry);
        }
    }
    return selected;
}

QString HistorySelectionState::normalizedFilePath(const QString &filePath)
{
    return filePath.trimmed();
}
