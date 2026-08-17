#ifndef VOCEKIT_HISTORY_DIRECTORY_MENU_H
#define VOCEKIT_HISTORY_DIRECTORY_MENU_H

#include <functional>

#include <QString>
#include <QMenu>

// 历史目录菜单：统一“当前保存目录、备份、当天录音/文本/详情、默认目录”的入口。
QMenu *recordDirectoryOpenMenu(
    QWidget *parent,
    QObject *receiver,
    const std::function<QString()> &recordDirectoryProvider
);

#endif // VOCEKIT_HISTORY_DIRECTORY_MENU_H
