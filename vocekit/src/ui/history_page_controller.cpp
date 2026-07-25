#include "history_page_controller.h"

#include "../domain/history_filter.h"
#include "../domain/history_modes.h"
#include "../domain/history_text.h"
#include "../providers/model_catalog.h"
#include "../storage/history_paths.h"
#include "../storage/history_record_service.h"
#include "../storage/history_store.h"
#include "attention_message.h"
#include "history_detail_dialog.h"
#include "history_entry_actions_controller.h"
#include "history_list_row.h"
#include "history_list_view.h"
#include "history_page.h"
#include "history_transfer_controller.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

HistoryPageController::HistoryPageController(
    QWidget *parent,
    const HistoryPageAccess &access
)
    : m_parent(parent), m_access(access)
{
}

QWidget *HistoryPageController::page()
{
    if (m_page) {
        return m_page;
    }

    HistoryPageCallbacks callbacks;
    callbacks.refresh = [this]() { refreshTabs(true); };
    callbacks.recordDirectoryPath = [this]() {
        return settingsSnapshot().recordDirectoryPath;
    };
    callbacks.backup = [this]() { backupRecords(); };
    callbacks.importRecords = [this]() { importRecords(); };
    callbacks.exportText = [this]() { exportText(); };
    callbacks.exportAudio = [this]() { exportAudio(); };
    callbacks.exportDetails = [this]() { exportDetails(); };
    callbacks.exportAll = [this]() { exportAll(); };
    callbacks.searchChanged = [this](const QString &) {
        refreshViewsFromCache();
    };
    callbacks.toggleBatchMode = [this]() {
        if (m_page) {
            m_page->toggleBatchMode();
        }
        updateBatchButtons();
        refreshViewsFromCache();
    };
    callbacks.selectAll = [this]() { selectCurrentFilteredEntries(); };
    callbacks.clearSelection = [this]() { clearSelectedEntries(); };
    callbacks.deleteSelected = [this]() { deleteSelectedEntries(); };

    m_page = new HistoryPage(callbacks);
    updateBatchButtons();
    refreshTabs();
    return m_page;
}

HistoryPage *HistoryPageController::pageWidget() const
{
    return m_page;
}

bool HistoryPageController::pageCreated() const
{
    return !m_page.isNull();
}

void HistoryPageController::updateBatchButtons()
{
    if (m_page) {
        m_page->refreshBatchState();
    }
}

void HistoryPageController::refreshViewsFromCache()
{
    refreshTabs(false);
}

void HistoryPageController::refreshTabs(bool forceReload)
{
    if (!m_page) {
        return;
    }
    m_page->refreshHistoryTabs(
        forceReload,
        tr8("正在加载历史记录..."),
        refreshCallbacks()
    );
}

void HistoryPageController::invalidateCache()
{
    if (m_page) {
        m_page->invalidateHistoryCache();
    }
}

bool HistoryPageController::historyCacheValid() const
{
    return m_page && m_page->historyCacheValid();
}

void HistoryPageController::deleteSelectedEntries()
{
    actionsController().deleteSelectedHistoryEntries();
}

void HistoryPageController::selectCurrentFilteredEntries()
{
    const QVector<HistoryEntry> entries = currentFilteredEntries();
    if (entries.isEmpty()) {
        showAttentionInformation(m_parent, tr8("没有可选择记录"), tr8("当前筛选条件下没有历史记录。"));
        return;
    }
    if (m_page) {
        m_page->selectEntries(entries);
    }
    updateBatchButtons();
    refreshViewsFromCache();
}

void HistoryPageController::clearSelectedEntries()
{
    if (m_page) {
        m_page->clearSelection();
    }
    updateBatchButtons();
    refreshViewsFromCache();
}

QVector<HistoryEntry> HistoryPageController::loadEntries() const
{
    return HistoryStore(
        historyRootPath(settingsSnapshot().recordDirectoryPath)
    ).loadEntries();
}

QVector<HistoryEntry> HistoryPageController::currentFilteredEntries() const
{
    HistoryFilter filter;
    filter.modeId = currentModeId();
    filter.searchText = m_page ? m_page->searchText() : QString();
    filter.customFunctions = settingsSnapshot().customFunctions;
    const QVector<HistoryEntry> entries = (m_page && m_page->historyCacheValid())
        ? m_page->cachedHistoryEntries()
        : loadEntries();
    return filterHistoryEntries(entries, filter);
}

QVector<HistoryEntry> HistoryPageController::selectedCurrentEntries() const
{
    return m_page
        ? m_page->selectedEntriesFrom(currentFilteredEntries())
        : QVector<HistoryEntry>();
}

QString HistoryPageController::currentModeId() const
{
    if (!m_page) {
        return QStringLiteral("__all");
    }
    return m_page->currentModeId(QStringLiteral("__all"));
}

QVector<HistoryTabDef> HistoryPageController::tabModes() const
{
    return buildHistoryTabModes(settingsSnapshot().customFunctions);
}

void HistoryPageController::backupRecords()
{
    transferController().backupHistoryRecords();
}

void HistoryPageController::importRecords()
{
    transferController().importHistoryRecords();
}

void HistoryPageController::exportText()
{
    transferController().exportHistoryText();
}

void HistoryPageController::exportAudio()
{
    transferController().exportHistoryAudio();
}

void HistoryPageController::exportDetails()
{
    transferController().exportHistoryDetails();
}

void HistoryPageController::exportAll()
{
    transferController().exportHistoryAll();
}

void HistoryPageController::playAudio(const HistoryEntry &entry)
{
    actionsController().playHistoryAudio(entry);
}

void HistoryPageController::toggleFavorite(const QString &filePath)
{
    actionsController().toggleHistoryFavorite(filePath);
}

void HistoryPageController::setFavoriteFolder(const QString &filePath, const QString &folder)
{
    actionsController().setHistoryFavoriteFolder(filePath, folder);
}

void HistoryPageController::deleteEntry(const HistoryEntry &entry)
{
    actionsController().deleteHistoryEntry(entry);
}

void HistoryPageController::showDetail(const HistoryEntry &entry)
{
    HistoryDetailDialog::Callbacks callbacks;
    callbacks.elapsedText = [](qint64 elapsedMs) {
        return historyElapsedDurationText(elapsedMs);
    };
    callbacks.modelText = [this](const HistoryEntry &detailEntry) {
        return modelText(detailEntry);
    };
    callbacks.recognizedText = [this](const HistoryEntry &detailEntry) {
        return recognizedText(detailEntry);
    };
    callbacks.detailPlainText = [this](const HistoryEntry &detailEntry) {
        return detailPlainText(detailEntry);
    };
    callbacks.speechProvider = [this]() {
        return settingsSnapshot().speechProvider;
    };
    callbacks.useSystemProxy = [this]() {
        return settingsSnapshot().useSystemProxy;
    };
    callbacks.speechNetworkPolicy = [this](const QString &modeId) {
        return settingsSnapshot().speechNetworkPolicies.value(modeId);
    };
    callbacks.updateRetriedSegment = [this](
        HistoryEntry *detailEntry,
        const HistorySegmentRetryResult &result
    ) {
        return updateRetriedSegment(detailEntry, result);
    };
    callbacks.historyChanged = [this, entry]() {
        notifyHistoryChanged(
            QStringList() << entry.filePath,
            false
        );
    };

    HistoryDetailDialog dialog(entry, callbacks, m_parent);
    dialog.exec();
}

QString HistoryPageController::modelText(const HistoryEntry &entry) const
{
    return modelDisplayText(entry.model);
}

QString HistoryPageController::recognizedText(const HistoryEntry &entry) const
{
    return historyEntryRecognizedText(entry);
}

QString HistoryPageController::detailPlainText(const HistoryEntry &entry) const
{
    return historyEntryDetailPlainText(entry, modelText(entry));
}

bool HistoryPageController::updateRetriedSegment(
    HistoryEntry *entry,
    const HistorySegmentRetryResult &result
)
{
    return HistoryRecordService(
        historyRootPath(settingsSnapshot().recordDirectoryPath)
    )
        .updateRetriedSegment(entry, result);
}

HistoryPageSettingsSnapshot HistoryPageController::settingsSnapshot() const
{
    return m_access.snapshotProvider
        ? m_access.snapshotProvider()
        : HistoryPageSettingsSnapshot();
}

void HistoryPageController::notifyHistoryChanged(
    const QStringList &recordIds,
    bool resetRequired
)
{
    if (m_access.historyChanged) {
        m_access.historyChanged(recordIds, resetRequired);
        return;
    }

    invalidateCache();
    refreshTabs(resetRequired);
}

QJsonObject HistoryPageController::entryToJson(const HistoryEntry &entry) const
{
    return historyEntryExportObject(
        entry,
        modelText(entry),
        !entry.audio.trimmed().isEmpty() && QFileInfo::exists(entry.audio)
    );
}

QWidget *HistoryPageController::rowWidget(const HistoryEntry &entry, QListWidget *list)
{
    HistoryRowCallbacks callbacks;
    callbacks.favoriteFolders = [this]() {
        return settingsSnapshot().favoriteFolders;
    };
    callbacks.titleText = [](const HistoryEntry &rowEntry) {
        return historyEntryTitleText(rowEntry);
    };
    callbacks.previewText = [](const HistoryEntry &rowEntry) {
        return historyEntryPreviewText(rowEntry);
    };
    callbacks.rowHeight = [](const HistoryEntry &rowEntry, int viewportWidth) {
        return historyEntryRowHeight(rowEntry, viewportWidth);
    };
    callbacks.isSelected = [this](const HistoryEntry &rowEntry) {
        return m_page && m_page->containsSelectedEntry(rowEntry);
    };
    callbacks.setSelected = [this](const HistoryEntry &rowEntry, bool checked) {
        if (m_page) {
            m_page->setEntrySelected(rowEntry, checked);
        }
    };
    callbacks.openDetail = [this](const HistoryEntry &rowEntry) {
        showDetail(HistoryStore::entryFromFile(rowEntry.filePath));
    };
    callbacks.playAudio = [this](const HistoryEntry &rowEntry) {
        playAudio(rowEntry);
    };
    callbacks.copyContent = [](const HistoryEntry &rowEntry) {
        QApplication::clipboard()->setText(rowEntry.output.trimmed().isEmpty() ? rowEntry.input : rowEntry.output);
    };
    callbacks.toggleFavorite = [this](const HistoryEntry &rowEntry) {
        toggleFavorite(rowEntry.filePath);
    };
    callbacks.createFavoriteFolder = [this]() {
        return createFavoriteFolderDialog();
    };
    callbacks.setFavoriteFolder = [this](const QString &filePath, const QString &folder) {
        setFavoriteFolder(filePath, folder);
    };
    callbacks.deleteEntry = [this](const HistoryEntry &rowEntry) {
        deleteEntry(rowEntry);
    };
    return createHistoryRowWidget(entry, list, m_page && m_page->batchMode(), callbacks, list);
}

QWidget *HistoryPageController::viewForMode(
    const QString &modeId,
    const QVector<HistoryEntry> &entries,
    int maxRows
)
{
    const HistoryPageSettingsSnapshot snapshot = settingsSnapshot();
    HistoryListViewOptions options;
    options.modeId = modeId;
    options.searchText = m_page ? m_page->searchText() : QString();
    options.customFunctions = snapshot.customFunctions;
    options.favoriteFolders = snapshot.favoriteFolders;
    options.initialLoadCount = snapshot.initialLoadCount;
    options.loadMoreCount = snapshot.loadMoreCount;
    options.maxRows = maxRows;

    HistoryListViewCallbacks callbacks;
    callbacks.createRow = [this](const HistoryEntry &entry, QListWidget *list) {
        return rowWidget(entry, list);
    };
    callbacks.createFavoriteFolder = [this]() {
        createFavoriteFolderDialog();
    };

    return createHistoryListView(entries, options, callbacks);
}

QString HistoryPageController::createFavoriteFolderDialog()
{
    const QString folder = actionsController().createFavoriteFolderDialog();
    if (!folder.isEmpty()) {
        notifyHistoryChanged(QStringList(), true);
    }
    return folder;
}

HistoryTransferController HistoryPageController::transferController()
{
    HistoryTransferController::Callbacks callbacks;
    callbacks.recordDirectoryPath = [this]() {
        return settingsSnapshot().recordDirectoryPath;
    };
    callbacks.filteredEntries = [this]() { return currentFilteredEntries(); };
    callbacks.selectedEntries = [this]() { return selectedCurrentEntries(); };
    callbacks.currentModeId = [this]() { return currentModeId(); };
    callbacks.searchText = [this]() { return m_page ? m_page->searchText() : QString(); };
    callbacks.detailPlainText = [this](const HistoryEntry &entry) { return detailPlainText(entry); };
    callbacks.entryJson = [this](const HistoryEntry &entry) { return entryToJson(entry); };
    callbacks.historyChanged = [this]() {
        notifyHistoryChanged(QStringList(), true);
    };
    return HistoryTransferController(m_parent, callbacks);
}

HistoryEntryActionsController HistoryPageController::actionsController()
{
    HistoryEntryActionsController::Callbacks callbacks;
    callbacks.recordDirectoryPath = [this]() {
        return settingsSnapshot().recordDirectoryPath;
    };
    callbacks.favoriteFolders = [this]() {
        return settingsSnapshot().favoriteFolders;
    };
    callbacks.addFavoriteFolder = [this](const QString &name) {
        return m_access.addFavoriteFolder
            ? m_access.addFavoriteFolder(name)
            : false;
    };
    callbacks.saveSettings = [this]() {
        return m_access.saveSettings ? m_access.saveSettings() : false;
    };
    callbacks.hasSelectedEntries = [this]() { return m_page && m_page->hasSelectedEntries(); };
    callbacks.selectedCount = [this]() { return m_page ? m_page->selectedCount() : 0; };
    callbacks.selectedFilePaths = [this]() { return m_page ? m_page->selectedFilePaths() : QStringList(); };
    callbacks.clearSelection = [this]() {
        if (m_page) {
            m_page->clearSelection();
        }
    };
    callbacks.updateBatchButtons = [this]() { updateBatchButtons(); };
    callbacks.historyChanged = [this](
        const QStringList &recordIds,
        bool resetRequired
    ) {
        notifyHistoryChanged(recordIds, resetRequired);
    };
    return HistoryEntryActionsController(m_parent, callbacks);
}

HistoryPageRefreshCallbacks HistoryPageController::refreshCallbacks()
{
    HistoryPageRefreshCallbacks callbacks;
    callbacks.tabModes = [this]() {
        return tabModes();
    };
    callbacks.loadEntries = [this]() {
        return loadEntries();
    };
    callbacks.createView = [this](const QString &mode, const QVector<HistoryEntry> &entries) {
        return viewForMode(mode, entries);
    };
    return callbacks;
}
