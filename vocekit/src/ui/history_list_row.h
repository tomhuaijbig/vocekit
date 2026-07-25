#ifndef VOCEKIT_HISTORY_LIST_ROW_H
#define VOCEKIT_HISTORY_LIST_ROW_H

#include "../domain/history_types.h"

#include <QStringList>
#include <QWidget>

#include <functional>

class QListWidget;

struct HistoryRowCallbacks
{
    std::function<QStringList()> favoriteFolders;
    std::function<QString(const HistoryEntry &)> titleText;
    std::function<QString(const HistoryEntry &)> previewText;
    std::function<int(const HistoryEntry &, int)> rowHeight;
    std::function<bool(const HistoryEntry &)> isSelected;
    std::function<void(const HistoryEntry &, bool)> setSelected;
    std::function<void(const HistoryEntry &)> openDetail;
    std::function<void(const HistoryEntry &)> playAudio;
    std::function<void(const HistoryEntry &)> copyContent;
    std::function<void(const HistoryEntry &)> toggleFavorite;
    std::function<QString()> createFavoriteFolder;
    std::function<void(const QString &, const QString &)> setFavoriteFolder;
    std::function<void(const HistoryEntry &)> deleteEntry;
};

// 历史列表行构建器：集中维护行高、标题、预览、批量选择和右侧操作菜单。
// 上层页面只提供回调，不再直接拼装每一行的控件树。
QWidget *createHistoryRowWidget(
    const HistoryEntry &entry,
    QListWidget *list,
    bool batchMode,
    const HistoryRowCallbacks &callbacks,
    QWidget *parent = nullptr
);

#endif // VOCEKIT_HISTORY_LIST_ROW_H
