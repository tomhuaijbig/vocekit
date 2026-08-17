#ifndef VOCEKIT_HISTORY_PAGE_H
#define VOCEKIT_HISTORY_PAGE_H

#include <QWidget>

#include "../domain/history_modes.h"
#include "../domain/history_selection.h"

#include <QString>
#include <QVector>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;

struct HistoryPageCallbacks
{
    std::function<void()> refresh;
    std::function<QString()> recordDirectoryPath;
    std::function<void()> backup;
    std::function<void()> importRecords;
    std::function<void()> exportText;
    std::function<void()> exportAudio;
    std::function<void()> exportDetails;
    std::function<void()> exportAll;
    std::function<void(const QString &)> searchChanged;
    std::function<void()> toggleBatchMode;
    std::function<void()> selectAll;
    std::function<void()> clearSelection;
    std::function<void()> deleteSelected;
};

struct HistoryPageRefreshCallbacks
{
    std::function<QVector<HistoryTabDef>()> tabModes;
    std::function<QVector<HistoryEntry>()> loadEntries;
    std::function<QWidget *(const QString &, const QVector<HistoryEntry> &)> createView;
};

// 历史记录页面外壳：只负责页面标题、工具栏、搜索框和标签容器。
// 历史数据加载、记录详情、导入导出等行为通过回调交给上层调度。
class HistoryPage : public QWidget
{
public:
    explicit HistoryPage(
        const HistoryPageCallbacks &callbacks,
        QWidget *parent = nullptr
    );

    QString searchText() const;
    bool batchMode() const;
    bool hasSelectedEntries() const;
    int selectedCount() const;
    QStringList selectedFilePaths() const;
    QVector<HistoryEntry> selectedEntriesFrom(const QVector<HistoryEntry> &entries) const;
    bool containsSelectedEntry(const HistoryEntry &entry) const;
    void setEntrySelected(const HistoryEntry &entry, bool selected);
    void setBatchMode(bool enabled);
    void toggleBatchMode();
    void selectEntries(const QVector<HistoryEntry> &entries);
    void clearSelection();
    void refreshBatchState();
    bool historyCacheValid() const;
    QVector<HistoryEntry> cachedHistoryEntries() const;
    void invalidateHistoryCache();
    void refreshHistoryTabs(
        bool forceReload,
        const QString &loadingMessage,
        const HistoryPageRefreshCallbacks &callbacks
    );
    bool hasTabs() const;
    int currentTabIndex() const;
    int tabCount() const;
    QString modeIdAt(int index, const QString &fallbackModeId) const;
    QString currentModeId(const QString &fallbackModeId) const;

private:
    void setBatchState(bool batchMode, int selectedCount);
    bool historyLoadInProgress() const;
    int beginHistoryLoad();
    bool finishHistoryLoad(int generation, const QVector<HistoryEntry> &entries);
    void resetTabLoadedState();
    void rebuildTabPlaceholders(
        const QVector<HistoryTabDef> &modes,
        int previousIndex,
        const QString &message,
        const std::function<void(int)> &onCurrentChanged
    );
    bool populateTab(int index, QWidget *content);
    void populateCachedHistoryTab(
        int index,
        const HistoryPageRefreshCallbacks &callbacks
    );
    void configureTabs();
    QWidget *tabPlaceholder(
        const QString &modeId,
        const QString &message
    ) const;

    HistoryPageCallbacks m_callbacks;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_batchToggleButton = nullptr;
    QPushButton *m_selectAllButton = nullptr;
    QPushButton *m_clearSelectionButton = nullptr;
    QPushButton *m_batchDeleteButton = nullptr;
    QLabel *m_selectionLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QString m_searchText;
    bool m_batchMode = false;
    HistorySelectionState m_selection;
    QVector<HistoryEntry> m_historyEntriesCache;
    bool m_historyCacheValid = false;
    bool m_historyLoadInProgress = false;
    int m_historyLoadGeneration = 0;
};

#endif // VOCEKIT_HISTORY_PAGE_H
