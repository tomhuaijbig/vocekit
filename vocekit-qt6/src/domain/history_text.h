#ifndef VOCEKIT_HISTORY_TEXT_H
#define VOCEKIT_HISTORY_TEXT_H

#include "history_types.h"

#include <QJsonObject>
#include <QString>

QString historyElapsedDurationText(qint64 elapsedMs);
QString historyDisplayTimeText(const QString &iso);
QString historyEntryModeText(const HistoryEntry &entry);
QString historyEntryPreviewText(const HistoryEntry &entry, int maxLength = 160);
QString historyEntryTitleText(const HistoryEntry &entry);
QString historyEntryRecognizedText(const HistoryEntry &entry);
QString historyEntryDetailPlainText(
    const HistoryEntry &entry,
    const QString &modelText = QString()
);
QJsonObject historyEntryExportObject(
    const HistoryEntry &entry,
    const QString &modelText = QString(),
    bool audioExists = false
);
bool historyEntryMatchesSearchText(
    const HistoryEntry &entry,
    const QString &keyword
);
int historyTextDisplayUnits(const QString &text);
int historyEntryRowHeight(const HistoryEntry &entry, int viewportWidth);

#endif // VOCEKIT_HISTORY_TEXT_H
