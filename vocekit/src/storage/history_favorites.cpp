#include "history_favorites.h"

#include "../file_utils.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

QString hfTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

HistoryFavoriteUpdateResult updateHistoryFavoriteFiles(
    const QString &filePath,
    bool favorite,
    const QString &folder
)
{
    HistoryFavoriteUpdateResult result;
    const QString mainPath = filePath.trimmed();
    if (mainPath.isEmpty()) {
        result.error = hfTr8("历史记录路径为空。");
        return result;
    }

    QJsonObject item;
    if (!readJsonObjectFile(mainPath, &item)) {
        result.error = hfTr8("无法读取历史记录文件。");
        return result;
    }

    item.insert(QStringLiteral("favorite"), favorite);
    if (favorite && !folder.trimmed().isEmpty()) {
        item.insert(QStringLiteral("favoriteFolder"), folder.trimmed());
    } else if (!favorite) {
        item.remove(QStringLiteral("favoriteFolder"));
    }

    const QByteArray json = QJsonDocument(item).toJson(QJsonDocument::Indented);
    const QString mirrorPath = item.value(QStringLiteral("allDetailFile")).toString().trimmed();

    if (!writeBytesAtomically(mainPath, json)) {
        result.error = hfTr8("无法写入历史记录文件。");
        return result;
    }
    result.wroteMain = true;

    if (!mirrorPath.isEmpty() && mirrorPath != mainPath && QFileInfo::exists(mirrorPath)) {
        if (writeBytesAtomically(mirrorPath, json)) {
            result.wroteMirror = true;
        }
    }

    result.ok = true;
    return result;
}
