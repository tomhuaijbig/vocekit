#ifndef VOCEKIT_HISTORY_ENTRY_ACTIONS_CONTROLLER_H
#define VOCEKIT_HISTORY_ENTRY_ACTIONS_CONTROLLER_H

#include "../domain/history_types.h"

#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

class HistoryEntryActionsController
{
public:
    struct Callbacks
    {
        std::function<QString()> recordDirectoryPath;
        std::function<QStringList()> favoriteFolders;
        std::function<bool(const QString &)> addFavoriteFolder;
        std::function<bool()> saveSettings;
        std::function<bool()> hasSelectedEntries;
        std::function<int()> selectedCount;
        std::function<QStringList()> selectedFilePaths;
        std::function<void()> clearSelection;
        std::function<void()> updateBatchButtons;
        std::function<void(const QStringList &, bool)> historyChanged;
    };

    HistoryEntryActionsController(QWidget *parent, const Callbacks &callbacks);

    void deleteSelectedHistoryEntries();
    QString createFavoriteFolderDialog();
    void playHistoryAudio(const HistoryEntry &entry);
    void toggleHistoryFavorite(const QString &filePath);
    void setHistoryFavoriteFolder(const QString &filePath, const QString &folder);
    void deleteHistoryEntry(const HistoryEntry &entry);

private:
    void notifyHistoryChangedAfterMutation(
        const QStringList &recordIds,
        bool resetRequired = false
    );

    QString recordDirectoryPath() const;
    QStringList favoriteFolders() const;

    QWidget *m_parent = nullptr;
    Callbacks m_callbacks;
};

#endif // VOCEKIT_HISTORY_ENTRY_ACTIONS_CONTROLLER_H
