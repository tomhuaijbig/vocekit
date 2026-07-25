#include "history_export.h"

#include "../domain/history_text.h"
#include "../file_utils.h"
#include "history_store.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegExp>
#include <QStringList>

namespace {

QString heTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString defaultDetailText(const HistoryEntry &entry)
{
    if (!entry.output.trimmed().isEmpty()) {
        return entry.output.trimmed();
    }
    if (!entry.input.trimmed().isEmpty()) {
        return entry.input.trimmed();
    }
    return entry.error.trimmed();
}

QJsonObject defaultEntryObject(const HistoryEntry &entry)
{
    return historyEntryExportObject(entry);
}

QString historyExportAudioFileName(const HistoryEntry &entry, int index, const QString &sourceAudio)
{
    const QFileInfo audioInfo(sourceAudio);
    const QString timePart = historyDisplayTimeText(entry.time).remove(QRegExp(QStringLiteral("[^0-9]")));
    const QString suffix = audioInfo.suffix().isEmpty() ? QStringLiteral("wav") : audioInfo.suffix();
    return HistoryStore::safeFileNamePart(
        timePart + QStringLiteral("_") + entry.mode + QStringLiteral("_") + QString::number(index + 1),
        QStringLiteral("audio")
    ) + QStringLiteral(".") + suffix;
}

} // namespace

QString buildHistoryTextExportContent(
    const QVector<HistoryEntry> &entries,
    const HistoryExportTextProvider &detailTextProvider
)
{
    QStringList blocks;
    blocks.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const HistoryEntry &entry = entries.at(i);
        const QString detail = detailTextProvider ? detailTextProvider(entry) : defaultDetailText(entry);
        blocks << (heTr8("第 ") + QString::number(i + 1) + heTr8(" 条\n") + detail);
    }
    return blocks.join(QStringLiteral("\n\n==============================\n\n"));
}

QJsonObject buildHistoryDetailsExportObject(
    const QVector<HistoryEntry> &entries,
    const HistoryExportOptions &options,
    const HistoryExportJsonProvider &entryObjectProvider
)
{
    QJsonArray records;
    for (const HistoryEntry &entry : entries) {
        records.append(entryObjectProvider ? entryObjectProvider(entry) : defaultEntryObject(entry));
    }

    const QDateTime exportedAt = options.exportedAt.isValid()
        ? options.exportedAt
        : QDateTime::currentDateTime();

    QJsonObject root;
    root.insert(QStringLiteral("exportedAt"), exportedAt.toString(Qt::ISODate));
    root.insert(QStringLiteral("recordCount"), entries.size());
    root.insert(QStringLiteral("filterMode"), options.filterMode);
    root.insert(QStringLiteral("searchText"), options.searchText);
    root.insert(QStringLiteral("selectionOnly"), options.selectionOnly);
    root.insert(QStringLiteral("records"), records);
    return root;
}

bool writeHistoryTextExportFile(
    const QVector<HistoryEntry> &entries,
    const QString &path,
    const HistoryExportTextProvider &detailTextProvider
)
{
    return writeTextFile(path, buildHistoryTextExportContent(entries, detailTextProvider));
}

bool writeHistoryDetailsExportFile(
    const QVector<HistoryEntry> &entries,
    const QString &path,
    const HistoryExportOptions &options,
    const HistoryExportJsonProvider &entryObjectProvider
)
{
    const QJsonObject root = buildHistoryDetailsExportObject(entries, options, entryObjectProvider);
    return writeBytesAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

HistoryAudioExportResult exportHistoryAudioFilesToDirectory(
    const QVector<HistoryEntry> &entries,
    const QString &targetPath
)
{
    HistoryAudioExportResult result;

    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        result.ok = false;
        result.error = heTr8("无法创建录音导出目录。");
        return result;
    }

    QString copyError;
    for (int i = 0; i < entries.size(); ++i) {
        const HistoryEntry entry = entries.at(i);
        QString sourceAudio = entry.audio.trimmed();
        if ((sourceAudio.isEmpty() || !QFileInfo::exists(sourceAudio)) && QFileInfo::exists(entry.allAudioFile)) {
            sourceAudio = entry.allAudioFile;
        }
        if (sourceAudio.isEmpty() || !QFileInfo::exists(sourceAudio)) {
            continue;
        }

        const QString fileName = historyExportAudioFileName(entry, i, sourceAudio);
        if (!copyFileToPath(sourceAudio, target.filePath(fileName), true, &copyError, &result.exported)) {
            result.ok = false;
            result.error = copyError.isEmpty() ? heTr8("无法复制录音文件。") : copyError;
            return result;
        }
    }

    return result;
}
