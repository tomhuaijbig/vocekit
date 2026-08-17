#ifndef VOCEKIT_HISTORY_FAVORITES_H
#define VOCEKIT_HISTORY_FAVORITES_H

#include <QString>

struct HistoryFavoriteUpdateResult
{
    bool ok = false;
    bool wroteMain = false;
    bool wroteMirror = false;
    QString error;
};

HistoryFavoriteUpdateResult updateHistoryFavoriteFiles(
    const QString &filePath,
    bool favorite,
    const QString &folder = QString()
);

#endif // VOCEKIT_HISTORY_FAVORITES_H
