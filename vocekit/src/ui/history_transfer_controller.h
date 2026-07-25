#ifndef VOCEKIT_HISTORY_TRANSFER_CONTROLLER_H
#define VOCEKIT_HISTORY_TRANSFER_CONTROLLER_H

#include "../domain/history_types.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <functional>

class QWidget;

class HistoryTransferController
{
public:
    struct Callbacks
    {
        std::function<QString()> recordDirectoryPath;
        std::function<QVector<HistoryEntry>()> filteredEntries;
        std::function<QVector<HistoryEntry>()> selectedEntries;
        std::function<QString()> currentModeId;
        std::function<QString()> searchText;
        std::function<QString(const HistoryEntry &)> detailPlainText;
        std::function<QJsonObject(const HistoryEntry &)> entryJson;
        std::function<void()> historyChanged;
    };

    HistoryTransferController(QWidget *parent, const Callbacks &callbacks);

    void backupHistoryRecords();
    void importHistoryRecords();
    void exportHistoryText();
    void exportHistoryDetails();
    void exportHistoryAudio();
    void exportHistoryAll();

private:
    bool selectedHistoryEntriesForExport(QVector<HistoryEntry> *entries) const;
    bool writeHistoryTextExport(const QVector<HistoryEntry> &entries, const QString &path) const;
    bool writeHistoryDetailsExport(const QVector<HistoryEntry> &entries, const QString &path) const;
    bool exportHistoryAudioFiles(const QVector<HistoryEntry> &entries, const QString &targetPath, int *exported, QString *error) const;

    QString recordDirectoryPath() const;
    QString currentModeId() const;
    QString searchText() const;
    static QString exportTimestamp();

    QWidget *m_parent = nullptr;
    Callbacks m_callbacks;
};

#endif // VOCEKIT_HISTORY_TRANSFER_CONTROLLER_H
