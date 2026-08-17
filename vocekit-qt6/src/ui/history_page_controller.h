#ifndef VOCEKIT_HISTORY_PAGE_CONTROLLER_H
#define VOCEKIT_HISTORY_PAGE_CONTROLLER_H

#include "../domain/app_legacy_types.h"
#include "../domain/history_types.h"
#include "../domain/history_modes.h"

#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class HistoryPage;
class QListWidget;
class QWidget;

// 历史页只需要这些运行配置，不直接了解完整应用设置。
struct HistoryPageSettingsSnapshot
{
    QString recordDirectoryPath;
    QVector<CustomFunctionDef> customFunctions;
    QStringList favoriteFolders;
    int initialLoadCount = 30;
    int loadMoreCount = 30;
    QString speechProvider;
    bool useSystemProxy = false;
    QMap<QString, QString> speechNetworkPolicies;
};

struct HistoryPageAccess
{
    std::function<HistoryPageSettingsSnapshot()> snapshotProvider;
    std::function<bool(const QString &)> addFavoriteFolder;
    std::function<bool()> saveSettings;
    std::function<void(const QStringList &, bool)> historyChanged;
};

// 历史页控制器：集中管理历史页的筛选、刷新、批量选择、导入导出和详情弹窗。
// HubWindow 只保留页面切换入口，避免继续直接拼装历史页内部控件和业务动作。
class HistoryPageController
{
public:
    HistoryPageController(
        QWidget *parent,
        const HistoryPageAccess &access
    );

    QWidget *page();
    HistoryPage *pageWidget() const;
    bool pageCreated() const;

    void updateBatchButtons();
    void refreshViewsFromCache();
    void refreshTabs(bool forceReload = false);
    void invalidateCache();
    bool historyCacheValid() const;

    void deleteSelectedEntries();
    void selectCurrentFilteredEntries();
    void clearSelectedEntries();

    QVector<HistoryEntry> loadEntries() const;
    QVector<HistoryEntry> currentFilteredEntries() const;
    QVector<HistoryEntry> selectedCurrentEntries() const;
    QString currentModeId() const;
    QVector<HistoryTabDef> tabModes() const;
    QWidget *viewForMode(
        const QString &modeId,
        const QVector<HistoryEntry> &entries,
        int maxRows = 0
    );

    void backupRecords();
    void importRecords();
    void exportText();
    void exportAudio();
    void exportDetails();
    void exportAll();

    void playAudio(const HistoryEntry &entry);
    void toggleFavorite(const QString &filePath);
    void setFavoriteFolder(const QString &filePath, const QString &folder);
    void deleteEntry(const HistoryEntry &entry);
    void showDetail(const HistoryEntry &entry);

private:
    QString modelText(const HistoryEntry &entry) const;
    QString recognizedText(const HistoryEntry &entry) const;
    QString detailPlainText(const HistoryEntry &entry) const;
    bool updateRetriedSegment(
        HistoryEntry *entry,
        const HistorySegmentRetryResult &result
    );
    QJsonObject entryToJson(const HistoryEntry &entry) const;
    HistoryPageSettingsSnapshot settingsSnapshot() const;
    void notifyHistoryChanged(
        const QStringList &recordIds,
        bool resetRequired
    );

    QWidget *rowWidget(const HistoryEntry &entry, QListWidget *list);
    QString createFavoriteFolderDialog();
    class HistoryTransferController transferController();
    class HistoryEntryActionsController actionsController();
    struct HistoryPageRefreshCallbacks refreshCallbacks();

    QWidget *m_parent = nullptr;
    HistoryPageAccess m_access;
    QPointer<HistoryPage> m_page;
};

#endif // VOCEKIT_HISTORY_PAGE_CONTROLLER_H
