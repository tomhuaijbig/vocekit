#include "history_list_view.h"

#include "ui_style.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

static void clearLayoutItems(QLayout *layout)
{
    if (!layout) {
        return;
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout *childLayout = item->layout()) {
            clearLayoutItems(childLayout);
        }
        delete item;
    }
}

static QListWidget *createBaseHistoryList()
{
    auto *list = new QListWidget;
    list->setFrameShape(QFrame::NoFrame);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setUniformItemSizes(false);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setTextElideMode(Qt::ElideNone);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; padding: 10px; }"
        "QListWidget::item {"
        "  background: transparent;"
        "  border: none;"
        "  margin: 6px 0;"
        "}"
        "QListWidget::item:selected { background: transparent; }"
    ));
    return list;
}

static void appendHistoryRows(
    QListWidget *list,
    const QVector<HistoryEntry> &entries,
    int start,
    int count,
    const HistoryListViewCallbacks &callbacks
)
{
    if (!list || start < 0 || count <= 0) {
        return;
    }
    const int end = qMin(start + count, entries.size());
    for (int i = start; i < end; ++i) {
        const HistoryEntry entry = entries.at(i);
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, entry.filePath);
        auto *rowWidget = callbacks.createRow
            ? callbacks.createRow(entry, list)
            : new QLabel(entry.output.trimmed().isEmpty() ? entry.input : entry.output);
        const int rowHeight = rowWidget
            ? qMax(rowWidget->minimumHeight(), rowWidget->sizeHint().height())
            : 140;
        item->setSizeHint(QSize(0, rowHeight));
        list->addItem(item);
        list->setItemWidget(item, rowWidget);
    }
}

static void appendHistoryRowsFromIndexes(
    QListWidget *list,
    const QVector<HistoryEntry> &entries,
    const QVector<int> &indexes,
    int start,
    int count,
    const HistoryListViewCallbacks &callbacks
)
{
    if (!list || start < 0 || count <= 0) {
        return;
    }
    const int end = qMin(start + count, indexes.size());
    for (int i = start; i < end; ++i) {
        const int entryIndex = indexes.at(i);
        if (entryIndex < 0 || entryIndex >= entries.size()) {
            continue;
        }
        const HistoryEntry entry = entries.at(entryIndex);
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, entry.filePath);
        auto *rowWidget = callbacks.createRow
            ? callbacks.createRow(entry, list)
            : new QLabel(entry.output.trimmed().isEmpty() ? entry.input : entry.output);
        const int rowHeight = rowWidget
            ? qMax(rowWidget->minimumHeight(), rowWidget->sizeHint().height())
            : 140;
        item->setSizeHint(QSize(0, rowHeight));
        list->addItem(item);
        list->setItemWidget(item, rowWidget);
    }
}

static void addHistoryLoadMoreItem(
    QListWidget *list,
    const QSharedPointer<QVector<HistoryEntry>> &entries,
    int nextStart,
    int batchSize,
    const HistoryListViewCallbacks &callbacks
)
{
    if (!list || !entries || nextStart >= entries->size()) {
        return;
    }

    auto *item = new QListWidgetItem;
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setSizeHint(QSize(0, 54));
    list->addItem(item);

    auto *button = new QPushButton(tr8("加载更多"));
    button->setFixedHeight(38);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    list->setItemWidget(item, button);

    QObject::connect(button, &QPushButton::clicked, list, [list, item, entries, nextStart, batchSize, callbacks]() {
        const int row = list->row(item);
        if (row >= 0) {
            delete list->takeItem(row);
        }
        appendHistoryRows(list, *entries, nextStart, batchSize, callbacks);
        addHistoryLoadMoreItem(list, entries, nextStart + batchSize, batchSize, callbacks);
    });
}

static void addHistoryLoadMoreIndexItem(
    QListWidget *list,
    const QVector<HistoryEntry> &entries,
    const QSharedPointer<QVector<int>> &indexes,
    int nextStart,
    int batchSize,
    const HistoryListViewCallbacks &callbacks
)
{
    if (!list || !indexes || nextStart >= indexes->size()) {
        return;
    }

    auto *item = new QListWidgetItem;
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setSizeHint(QSize(0, 54));
    list->addItem(item);

    auto *button = new QPushButton(tr8("加载更多"));
    button->setFixedHeight(38);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    list->setItemWidget(item, button);

    QObject::connect(button, &QPushButton::clicked, list, [list, item, entries, indexes, nextStart, batchSize, callbacks]() {
        const int row = list->row(item);
        if (row >= 0) {
            delete list->takeItem(row);
        }
        appendHistoryRowsFromIndexes(list, entries, *indexes, nextStart, batchSize, callbacks);
        addHistoryLoadMoreIndexItem(list, entries, indexes, nextStart + batchSize, batchSize, callbacks);
    });
}

static QListWidget *createHistoryListForMode(
    const QVector<HistoryEntry> &entries,
    const HistoryListViewOptions &options,
    const HistoryListViewCallbacks &callbacks
)
{
    auto *list = createBaseHistoryList();

    if (options.modeId == QStringLiteral("__all")) {
        HistoryFilter filter;
        filter.modeId = QStringLiteral("__all");
        filter.searchText = options.searchText;
        filter.customFunctions = options.customFunctions;
        QSharedPointer<QVector<int>> indexes(
            new QVector<int>(historyEntryIndexesMatchingFilter(entries, filter))
        );

        if (indexes->isEmpty()) {
            auto *empty = new QListWidgetItem(tr8("暂无记录"));
            empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
            list->addItem(empty);
            return list;
        }

        const bool pagedAll = options.maxRows <= 0 && indexes->size() > options.initialLoadCount;
        const int initialRows = options.maxRows > 0
            ? qMin(options.maxRows, indexes->size())
            : (pagedAll ? options.initialLoadCount : indexes->size());
        appendHistoryRowsFromIndexes(list, entries, *indexes, 0, initialRows, callbacks);

        if (options.maxRows > 0 && indexes->size() > options.maxRows) {
            auto *more = new QListWidgetItem(tr8("还有更多记录，请打开左侧“历史记录”查看全部。"));
            more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
            list->addItem(more);
        } else if (pagedAll) {
            addHistoryLoadMoreIndexItem(list, entries, indexes, initialRows, options.loadMoreCount, callbacks);
        }
        return list;
    }

    HistoryFilter filter;
    filter.modeId = options.modeId;
    filter.searchText = options.searchText;
    filter.customFunctions = options.customFunctions;
    QSharedPointer<QVector<HistoryEntry>> filtered(
        new QVector<HistoryEntry>(filterHistoryEntries(entries, filter))
    );

    if (filtered->isEmpty()) {
        auto *empty = new QListWidgetItem(tr8("暂无记录"));
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        list->addItem(empty);
        return list;
    }

    const bool paged = options.maxRows <= 0 && filtered->size() > options.initialLoadCount;
    const int initialRows = options.maxRows > 0
        ? qMin(options.maxRows, filtered->size())
        : (paged ? options.initialLoadCount : filtered->size());
    appendHistoryRows(list, *filtered, 0, initialRows, callbacks);

    if (options.maxRows > 0 && filtered->size() > options.maxRows) {
        auto *more = new QListWidgetItem(tr8("还有更多记录，请打开左侧“历史记录”查看全部。"));
        more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
        list->addItem(more);
    } else if (paged) {
        addHistoryLoadMoreItem(list, filtered, initialRows, options.loadMoreCount, callbacks);
    }

    return list;
}

QWidget *createHistoryListView(
    const QVector<HistoryEntry> &entries,
    const HistoryListViewOptions &options,
    const HistoryListViewCallbacks &callbacks,
    QWidget *parent
)
{
    if (options.modeId != QStringLiteral("__favorite")) {
        return createHistoryListForMode(entries, options, callbacks);
    }

    auto *view = new QWidget(parent);
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *tools = new QHBoxLayout;
    tools->setSpacing(10);
    auto *label = new QLabel(tr8("收藏夹"));
    label->setFont(appFont(10, QFont::DemiBold));
    auto *folderBox = new QComboBox;
    folderBox->setMinimumHeight(40);
    folderBox->setMinimumWidth(220);
    folderBox->addItem(tr8("全部收藏"), QStringLiteral("__favorite"));
    for (const QString &folder : options.favoriteFolders) {
        folderBox->addItem(folder, QStringLiteral("__favorite_folder:") + folder);
    }

    auto *create = new QPushButton(tr8("新建收藏夹"));
    create->setFont(appFont(10, QFont::DemiBold));
    create->setFixedSize(126, 42);
    create->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));

    auto *listHost = new QWidget;
    auto *listLayout = new QVBoxLayout(listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    auto rebuildList = [folderBox, listLayout, entries, options, callbacks]() {
        clearLayoutItems(listLayout);
        HistoryListViewOptions folderOptions = options;
        folderOptions.modeId = folderBox->currentData().toString();
        listLayout->addWidget(createHistoryListForMode(entries, folderOptions, callbacks));
    };

    QObject::connect(
        folderBox,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        view,
        [rebuildList](int) {
            rebuildList();
        }
    );
    QObject::connect(create, &QPushButton::clicked, view, [callbacks]() {
        if (callbacks.createFavoriteFolder) {
            callbacks.createFavoriteFolder();
        }
    });

    tools->addWidget(label);
    tools->addWidget(folderBox);
    tools->addWidget(create);
    tools->addStretch();
    layout->addLayout(tools);
    layout->addWidget(listHost, 1);
    rebuildList();
    return view;
}
