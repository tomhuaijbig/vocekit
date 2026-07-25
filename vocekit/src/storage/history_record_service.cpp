#include "history_record_service.h"

#include <QSet>

HistoryRecordService::HistoryRecordService(const QString &recordDirectory)
    : m_store(recordDirectory)
{
}

HistoryAppendResult HistoryRecordService::save(
    const HistoryRecordSaveRequest &request
)
{
    HistoryAppendRequest appendRequest;
    appendRequest.modeId = request.modeId;
    appendRequest.modeTitle = request.modeTitle;
    appendRequest.sourceAudioPath = request.sourceAudioPath;
    appendRequest.draft = request.draft;
    appendRequest.item = HistoryRecordBuilder::buildMetadata(request.metadata);

    return HistoryRecordService(request.recordDirectory)
        .m_store.appendRecord(appendRequest);
}

HistoryAppendResult HistoryRecordService::saveOcr(
    const QString &modeTitle,
    const OcrPageHistoryMetadataRequest &metadata
) const
{
    HistoryAppendRequest request;
    request.modeId = QStringLiteral("ocr");
    request.modeTitle = modeTitle;
    request.item = HistoryRecordBuilder::buildOcrPageMetadata(metadata);
    return m_store.appendRecord(request);
}

HistoryFavoriteUpdateResult HistoryRecordService::updateFavorite(
    const QString &filePath,
    bool favorite,
    const QString &folder
) const
{
    HistoryFavoriteUpdateResult result;
    if (!isManagedDetailPath(filePath)) {
        result.error = QStringLiteral("History detail path is outside the record directory.");
        return result;
    }

    result = updateHistoryFavoriteFiles(filePath, favorite, folder);
    if (result.ok) {
        m_store.rebuildIndex();
    }
    return result;
}

HistoryDeleteResult HistoryRecordService::removeEntry(
    const HistoryEntry &entry
) const
{
    return removeEntries(QStringList() << entry.filePath);
}

HistoryDeleteResult HistoryRecordService::removeEntries(
    const QStringList &filePaths
) const
{
    HistoryDeleteResult result;
    QSet<QString> seen;

    for (const QString &filePath : filePaths) {
        const QString trimmed = filePath.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QString normalized = HistoryStore::normalizedPathForCompare(trimmed);
        if (seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);
        ++result.requestedCount;

        if (!isManagedDetailPath(trimmed)) {
            result.failedPaths.append(trimmed);
            continue;
        }

        const HistoryEntry entry = HistoryStore::entryFromFile(trimmed);
        if (entry.filePath.trimmed().isEmpty()
            || !HistoryStore::removeEntryFiles(entry)) {
            result.failedPaths.append(trimmed);
            continue;
        }
        ++result.removedCount;
    }

    if (result.requestedCount > 0) {
        m_store.rebuildIndex();
    }
    result.ok = result.requestedCount > 0
        && result.removedCount == result.requestedCount
        && result.failedPaths.isEmpty();
    return result;
}

bool HistoryRecordService::updateRetriedSegment(
    HistoryEntry *entry,
    const HistorySegmentRetryResult &result
) const
{
    if (!entry || !isManagedDetailPath(entry->filePath)) {
        return false;
    }
    return m_store.updateRetriedSegment(entry, result);
}

void HistoryRecordService::rebuildIndex() const
{
    m_store.rebuildIndex();
}

bool HistoryRecordService::isManagedDetailPath(const QString &filePath) const
{
    const QString trimmed = filePath.trimmed();
    return !trimmed.isEmpty()
        && HistoryStore::pathIsSameOrInside(trimmed, m_store.rootPath());
}
