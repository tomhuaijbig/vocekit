#include "history_page.h"

#include "history_directory_menu.h"
#include "tab_bar_wheel_filter.h"
#include "ui_style.h"

#include <QtConcurrent>
#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

static void clearHistoryPageLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearHistoryPageLayout(item->layout());
        }
        delete item;
    }
}

HistoryPage::HistoryPage(
    const HistoryPageCallbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent), m_callbacks(callbacks)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    auto *title = new QLabel(tr8("历史记录"));
    title->setFont(appFont(24, QFont::DemiBold));
    titleBox->addWidget(title);

    auto *refresh = new QPushButton(tr8("刷新"));
    refresh->setFixedHeight(38);
    refresh->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(refresh, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.refresh) {
            m_callbacks.refresh();
        }
    });

    auto *openFolder = new QPushButton(tr8("打开目录"));
    openFolder->setFixedHeight(38);
    openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    openFolder->setMenu(recordDirectoryOpenMenu(openFolder, this, [this]() {
        return m_callbacks.recordDirectoryPath ? m_callbacks.recordDirectoryPath() : QString();
    }));

    auto *backup = new QPushButton(tr8("备份"));
    backup->setMinimumSize(72, 38);
    backup->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(backup, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.backup) {
            m_callbacks.backup();
        }
    });

    auto *importButton = new QPushButton(tr8("导入"));
    importButton->setMinimumSize(72, 38);
    importButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(importButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.importRecords) {
            m_callbacks.importRecords();
        }
    });

    auto *exportButton = new QPushButton(tr8("导出"));
    exportButton->setMinimumSize(72, 38);
    exportButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *exportMenu = new QMenu(exportButton);
    exportMenu->addAction(tr8("文本导出"), this, [this]() {
        if (m_callbacks.exportText) {
            m_callbacks.exportText();
        }
    });
    exportMenu->addAction(tr8("录音导出"), this, [this]() {
        if (m_callbacks.exportAudio) {
            m_callbacks.exportAudio();
        }
    });
    exportMenu->addAction(tr8("详细记录导出"), this, [this]() {
        if (m_callbacks.exportDetails) {
            m_callbacks.exportDetails();
        }
    });
    exportMenu->addAction(tr8("全部导出"), this, [this]() {
        if (m_callbacks.exportAll) {
            m_callbacks.exportAll();
        }
    });
    exportButton->setMenu(exportMenu);

    top->addLayout(titleBox, 1);
    top->addWidget(refresh);
    top->addWidget(openFolder);
    top->addWidget(backup);
    top->addWidget(importButton);
    top->addWidget(exportButton);
    layout->addLayout(top);

    auto *tools = new QHBoxLayout;
    tools->setSpacing(10);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setMinimumHeight(40);
    m_searchEdit->setPlaceholderText(tr8("搜索历史记录、识别文本、输出结果或错误信息"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 8px;"
        "  padding: 0 12px;"
        "  color: #111827;"
        "}"
    ));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_searchText = text;
        if (m_callbacks.searchChanged) {
            m_callbacks.searchChanged(text);
        }
    });

    m_batchToggleButton = new QPushButton(tr8("选择记录"));
    m_batchToggleButton->setFont(appFont(10, QFont::DemiBold));
    m_batchToggleButton->setMinimumSize(112, 42);
    m_batchToggleButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_batchToggleButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(m_batchToggleButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.toggleBatchMode) {
            m_callbacks.toggleBatchMode();
        }
    });

    m_selectAllButton = new QPushButton(tr8("全选当前"));
    m_selectAllButton->setFont(appFont(10, QFont::DemiBold));
    m_selectAllButton->setMinimumSize(112, 42);
    m_selectAllButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_selectAllButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(m_selectAllButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.selectAll) {
            m_callbacks.selectAll();
        }
    });

    m_clearSelectionButton = new QPushButton(tr8("清空选择"));
    m_clearSelectionButton->setFont(appFont(10, QFont::DemiBold));
    m_clearSelectionButton->setMinimumSize(112, 42);
    m_clearSelectionButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_clearSelectionButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(m_clearSelectionButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.clearSelection) {
            m_callbacks.clearSelection();
        }
    });

    m_batchDeleteButton = new QPushButton(tr8("删除选中"));
    m_batchDeleteButton->setFont(appFont(10, QFont::DemiBold));
    m_batchDeleteButton->setMinimumSize(126, 42);
    m_batchDeleteButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_batchDeleteButton->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
    connect(m_batchDeleteButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.deleteSelected) {
            m_callbacks.deleteSelected();
        }
    });

    m_selectionLabel = new QLabel(tr8("未选择"));
    m_selectionLabel->setMinimumHeight(42);
    m_selectionLabel->setMinimumWidth(92);
    m_selectionLabel->setAlignment(Qt::AlignCenter);
    m_selectionLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #007f5f; font-weight: 600; padding: 0 6px; }"
    ));

    tools->addWidget(m_searchEdit, 1);
    tools->addWidget(m_batchToggleButton);
    tools->addWidget(m_selectAllButton);
    tools->addWidget(m_clearSelectionButton);
    tools->addWidget(m_batchDeleteButton);
    tools->addWidget(m_selectionLabel);
    layout->addLayout(tools);

    m_tabs = new QTabWidget;
    configureTabs();
    layout->addWidget(m_tabs, 1);
}

void HistoryPage::configureTabs()
{
    if (!m_tabs || !m_tabs->tabBar()) {
        return;
    }
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setUsesScrollButtons(true);
    m_tabs->tabBar()->setElideMode(Qt::ElideNone);
    m_tabs->tabBar()->installEventFilter(new TabBarWheelFilter(m_tabs->tabBar()));
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
        "QTabBar::tab { padding: 9px 16px; color: #667085; }"
        "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
        "QTabBar QToolButton { width: 28px; background: #ffffff; border: 1px solid #d0d5dd; color: #111827; }"
        "QTabBar QToolButton:hover { background: #eef2ff; }"
    ));
}

void HistoryPage::setBatchState(bool batchMode, int selectedCount)
{
    const bool hasSelection = selectedCount > 0;
    if (m_batchToggleButton) {
        m_batchToggleButton->setText(batchMode ? tr8("退出选择") : tr8("选择记录"));
    }
    if (m_selectAllButton) {
        m_selectAllButton->setVisible(batchMode);
    }
    if (m_clearSelectionButton) {
        m_clearSelectionButton->setVisible(batchMode);
        m_clearSelectionButton->setEnabled(hasSelection);
    }
    if (m_batchDeleteButton) {
        m_batchDeleteButton->setVisible(batchMode);
        m_batchDeleteButton->setEnabled(hasSelection);
        m_batchDeleteButton->setText(hasSelection
            ? tr8("删除选中(") + QString::number(selectedCount) + tr8(")")
            : tr8("删除选中"));
    }
    if (m_selectionLabel) {
        m_selectionLabel->setVisible(batchMode);
        m_selectionLabel->setText(hasSelection
            ? tr8("已选择 ") + QString::number(selectedCount) + tr8(" 条")
            : tr8("未选择"));
    }
}

QString HistoryPage::searchText() const
{
    return m_searchText;
}

bool HistoryPage::batchMode() const
{
    return m_batchMode;
}

bool HistoryPage::hasSelectedEntries() const
{
    return !m_selection.isEmpty();
}

int HistoryPage::selectedCount() const
{
    return m_selection.count();
}

QStringList HistoryPage::selectedFilePaths() const
{
    return m_selection.selectedFilePaths();
}

QVector<HistoryEntry> HistoryPage::selectedEntriesFrom(const QVector<HistoryEntry> &entries) const
{
    return m_selection.selectedEntriesFrom(entries);
}

bool HistoryPage::containsSelectedEntry(const HistoryEntry &entry) const
{
    return m_selection.containsEntry(entry);
}

void HistoryPage::setEntrySelected(const HistoryEntry &entry, bool selected)
{
    m_selection.setEntrySelected(entry, selected);
    refreshBatchState();
}

void HistoryPage::setBatchMode(bool enabled)
{
    if (m_batchMode == enabled) {
        refreshBatchState();
        return;
    }

    m_batchMode = enabled;
    if (!m_batchMode) {
        m_selection.clear();
    }
    refreshBatchState();
}

void HistoryPage::toggleBatchMode()
{
    setBatchMode(!m_batchMode);
}

void HistoryPage::selectEntries(const QVector<HistoryEntry> &entries)
{
    m_batchMode = true;
    m_selection.selectEntries(entries);
    refreshBatchState();
}

void HistoryPage::clearSelection()
{
    m_selection.clear();
    refreshBatchState();
}

void HistoryPage::refreshBatchState()
{
    setBatchState(m_batchMode, m_selection.count());
}

bool HistoryPage::historyCacheValid() const
{
    return m_historyCacheValid;
}

bool HistoryPage::historyLoadInProgress() const
{
    return m_historyLoadInProgress;
}

QVector<HistoryEntry> HistoryPage::cachedHistoryEntries() const
{
    return m_historyEntriesCache;
}

void HistoryPage::invalidateHistoryCache()
{
    m_historyEntriesCache.clear();
    m_historyCacheValid = false;
    m_historyLoadInProgress = false;
    ++m_historyLoadGeneration;
}

int HistoryPage::beginHistoryLoad()
{
    m_historyLoadInProgress = true;
    return ++m_historyLoadGeneration;
}

bool HistoryPage::finishHistoryLoad(int generation, const QVector<HistoryEntry> &entries)
{
    if (generation != m_historyLoadGeneration) {
        return false;
    }
    m_historyLoadInProgress = false;
    m_historyEntriesCache = entries;
    m_historyCacheValid = true;
    return true;
}

void HistoryPage::refreshHistoryTabs(
    bool forceReload,
    const QString &loadingMessage,
    const HistoryPageRefreshCallbacks &callbacks
)
{
    if (!hasTabs() || !callbacks.tabModes || !callbacks.loadEntries || !callbacks.createView) {
        return;
    }

    const int previousIndex = currentTabIndex();
    const auto populateCurrent = [this, callbacks](int index) {
        populateCachedHistoryTab(index, callbacks);
    };

    if (m_historyCacheValid && !forceReload) {
        if (tabCount() == 0) {
            rebuildTabPlaceholders(callbacks.tabModes(), previousIndex, QString(), populateCurrent);
        }
        if (tabCount() > 0) {
            populateCachedHistoryTab(qMin(previousIndex, tabCount() - 1), callbacks);
        }
        return;
    }

    if (m_historyLoadInProgress && !forceReload) {
        if (tabCount() == 0) {
            rebuildTabPlaceholders(callbacks.tabModes(), previousIndex, loadingMessage, populateCurrent);
        }
        return;
    }

    rebuildTabPlaceholders(callbacks.tabModes(), previousIndex, loadingMessage, populateCurrent);

    const int targetIndex = qMin(previousIndex, tabCount() - 1);
    const int generation = beginHistoryLoad();
    auto *watcher = new QFutureWatcher<QVector<HistoryEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<HistoryEntry>>::finished, this, [this, watcher, generation, targetIndex, callbacks]() {
        if (finishHistoryLoad(generation, watcher->result())) {
            resetTabLoadedState();
            populateCachedHistoryTab(qMin(targetIndex, tabCount() - 1), callbacks);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([callbacks]() {
        return callbacks.loadEntries ? callbacks.loadEntries() : QVector<HistoryEntry>();
    }));
}

bool HistoryPage::hasTabs() const
{
    return m_tabs != nullptr;
}

int HistoryPage::currentTabIndex() const
{
    return m_tabs ? qMax(0, m_tabs->currentIndex()) : 0;
}

int HistoryPage::tabCount() const
{
    return m_tabs ? m_tabs->count() : 0;
}

QString HistoryPage::currentModeId(const QString &fallbackModeId) const
{
    QWidget *page = m_tabs ? m_tabs->currentWidget() : nullptr;
    const QString mode = page ? page->property("historyMode").toString() : QString();
    return mode.trimmed().isEmpty() ? fallbackModeId : mode;
}

QString HistoryPage::modeIdAt(int index, const QString &fallbackModeId) const
{
    QWidget *page = (m_tabs && index >= 0 && index < m_tabs->count())
        ? m_tabs->widget(index)
        : nullptr;
    const QString mode = page ? page->property("historyMode").toString() : QString();
    return mode.trimmed().isEmpty() ? fallbackModeId : mode;
}

void HistoryPage::resetTabLoadedState()
{
    if (!m_tabs) {
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *page = m_tabs->widget(i);
        if (page) {
            page->setProperty("historyLoaded", false);
        }
    }
}

void HistoryPage::rebuildTabPlaceholders(
    const QVector<HistoryTabDef> &modes,
    int previousIndex,
    const QString &message,
    const std::function<void(int)> &onCurrentChanged
)
{
    if (!m_tabs) {
        return;
    }

    QObject::disconnect(m_tabs, SIGNAL(currentChanged(int)), this, nullptr);
    while (m_tabs->count() > 0) {
        QWidget *page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        delete page;
    }

    for (const HistoryTabDef &mode : modes) {
        m_tabs->addTab(tabPlaceholder(mode.id, message), mode.title);
    }

    connect(m_tabs, &QTabWidget::currentChanged, this, [onCurrentChanged](int index) {
        if (onCurrentChanged) {
            onCurrentChanged(index);
        }
    });

    if (m_tabs->count() > 0) {
        const int index = qBound(0, previousIndex, m_tabs->count() - 1);
        m_tabs->setCurrentIndex(index);
    }
}

bool HistoryPage::populateTab(int index, QWidget *content)
{
    if (!m_tabs || index < 0 || index >= m_tabs->count() || !content) {
        if (content) {
            content->deleteLater();
        }
        return false;
    }

    QWidget *page = m_tabs->widget(index);
    if (!page || page->property("historyLoaded").toBool()) {
        content->deleteLater();
        return false;
    }

    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    if (!layout) {
        content->deleteLater();
        return false;
    }

    clearHistoryPageLayout(layout);
    layout->addWidget(content);
    page->setProperty("historyLoaded", true);
    return true;
}

void HistoryPage::populateCachedHistoryTab(
    int index,
    const HistoryPageRefreshCallbacks &callbacks
)
{
    if (!callbacks.createView || index < 0 || index >= tabCount()) {
        return;
    }

    const QString mode = modeIdAt(index, QStringLiteral("__all"));
    populateTab(index, callbacks.createView(mode, m_historyEntriesCache));
}

QWidget *HistoryPage::tabPlaceholder(
    const QString &modeId,
    const QString &message
) const
{
    auto *page = new QWidget;
    page->setProperty("historyMode", modeId);
    page->setProperty("historyLoaded", false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    if (!message.trimmed().isEmpty()) {
        auto *label = new QLabel(message);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QStringLiteral("color: #667085; padding: 40px;"));
        layout->addWidget(label, 1);
    }
    return page;
}
