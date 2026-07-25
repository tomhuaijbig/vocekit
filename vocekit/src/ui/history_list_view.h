#ifndef VOCEKIT_HISTORY_LIST_VIEW_H
#define VOCEKIT_HISTORY_LIST_VIEW_H

#include "../domain/history_filter.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QListWidget;

struct HistoryListViewOptions
{
    QString modeId = QStringLiteral("__all");
    QString searchText;
    QVector<CustomFunctionDef> customFunctions;
    QStringList favoriteFolders;
    int initialLoadCount = 30;
    int loadMoreCount = 30;
    int maxRows = 0;
};

struct HistoryListViewCallbacks
{
    std::function<QWidget *(const HistoryEntry &, QListWidget *)> createRow;
    std::function<void()> createFavoriteFolder;
};

// 历史列表视图构建器：负责分类筛选、收藏夹视图和“加载更多”分页。
// 具体每条记录如何显示由 createRow 回调决定。
QWidget *createHistoryListView(
    const QVector<HistoryEntry> &entries,
    const HistoryListViewOptions &options,
    const HistoryListViewCallbacks &callbacks,
    QWidget *parent = nullptr
);

#endif // VOCEKIT_HISTORY_LIST_VIEW_H
