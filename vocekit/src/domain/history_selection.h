#ifndef VOCEKIT_HISTORY_SELECTION_H
#define VOCEKIT_HISTORY_SELECTION_H

#include "history_types.h"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class HistorySelectionState
{
public:
    void clear();
    bool isEmpty() const;
    int count() const;

    bool containsFilePath(const QString &filePath) const;
    bool containsEntry(const HistoryEntry &entry) const;

    void setFileSelected(const QString &filePath, bool selected);
    void setEntrySelected(const HistoryEntry &entry, bool selected);
    void selectEntries(const QVector<HistoryEntry> &entries);

    QStringList selectedFilePaths() const;
    QVector<HistoryEntry> selectedEntriesFrom(const QVector<HistoryEntry> &entries) const;

private:
    static QString normalizedFilePath(const QString &filePath);

    QSet<QString> m_filePaths;
};

#endif // VOCEKIT_HISTORY_SELECTION_H
