#include "history_record_service.h"

#include "../file_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSet>

namespace {

QString historyServiceText(const char *text)
{
    return QString::fromUtf8(text);
}

void setFlowHistoryError(OperationError *error, const QString &message)
{
    if (!error) {
        return;
    }
    error->code = QStringLiteral("flow_history_save_failed");
    error->message = message;
    error->detail.clear();
    error->retryable = false;
}

QString canonicalPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(
        canonical.isEmpty() ? info.absoluteFilePath() : canonical
    );
}

bool hasExpectedSuffix(const QString &path, const QString &suffix)
{
    return suffix.isEmpty()
        || QFileInfo(path).suffix().compare(suffix, Qt::CaseInsensitive) == 0;
}

bool resolveManagedExistingFile(
    const QString &requestedPath,
    const QString &rootPath,
    const QString &expectedSuffix,
    QString *resolvedPath
)
{
    const QString trimmed = requestedPath.trimmed();
    if (trimmed.isEmpty() || !QFileInfo(trimmed).isAbsolute()) {
        return false;
    }

    const QFileInfo info(trimmed);
    if (!info.exists() || !info.isFile()
        || !hasExpectedSuffix(trimmed, expectedSuffix)) {
        return false;
    }

    const QString absolutePath = QDir::cleanPath(info.absoluteFilePath());
    const QString absoluteRoot =
        QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    if (!HistoryStore::pathIsSameOrInside(absolutePath, absoluteRoot)) {
        return false;
    }

    const QString canonicalFile = canonicalPath(absolutePath);
    const QString canonicalRoot = canonicalPath(absoluteRoot);
    if (!HistoryStore::pathIsSameOrInside(canonicalFile, canonicalRoot)) {
        return false;
    }

    if (resolvedPath) {
        *resolvedPath = absolutePath;
    }
    return true;
}

bool readExistingBytes(const QString &path, QByteArray *bytes)
{
    if (!bytes) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    *bytes = file.readAll();
    return file.error() == QFile::NoError;
}

bool referencePath(
    const QJsonObject &item,
    const QString &key,
    QString *path
)
{
    if (!path) {
        return false;
    }
    path->clear();
    if (!item.contains(key) || item.value(key).isNull()
        || item.value(key).isUndefined()) {
        return true;
    }
    if (!item.value(key).isString()) {
        return false;
    }
    *path = item.value(key).toString().trimmed();
    return true;
}

struct PendingHistoryWrite
{
    QString path;
    QByteArray before;
    QByteArray after;
};

bool addPendingWrite(
    QVector<PendingHistoryWrite> *writes,
    const QString &path,
    const QByteArray &after
)
{
    if (!writes) {
        return false;
    }
    const QString normalized = HistoryStore::normalizedPathForCompare(path);
    for (const PendingHistoryWrite &write : *writes) {
        if (HistoryStore::normalizedPathForCompare(write.path) == normalized) {
            return write.after == after;
        }
    }

    PendingHistoryWrite write;
    write.path = path;
    write.after = after;
    if (!readExistingBytes(path, &write.before)) {
        return false;
    }
    writes->append(write);
    return true;
}

void restoreWrites(
    const QVector<PendingHistoryWrite> &writes,
    int writtenCount
)
{
    for (int index = writtenCount - 1; index >= 0; --index) {
        writeBytesAtomically(writes.at(index).path, writes.at(index).before);
    }
}

} // namespace

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

bool HistoryRecordService::updateFlowEditedText(
    const ExecutionId &runId,
    const QString &detailPath,
    const QString &editedText,
    OperationError *error
) const
{
    if (error) {
        *error = OperationError();
    }
    if (!runId.isValid()) {
        setFlowHistoryError(
            error,
            historyServiceText("流程运行编号无效，无法更新历史记录。")
        );
        return false;
    }

    QString managedDetailPath;
    if (!resolveManagedExistingFile(
            detailPath,
            m_store.rootPath(),
            QStringLiteral("json"),
            &managedDetailPath)) {
        setFlowHistoryError(
            error,
            historyServiceText("历史详情不存在或不在本次运行冻结的历史目录内。")
        );
        return false;
    }

    QJsonObject item;
    if (!readJsonObjectFile(managedDetailPath, &item)) {
        setFlowHistoryError(
            error,
            historyServiceText("历史详情文件已损坏，无法安全更新。")
        );
        return false;
    }
    if (!item.value(QStringLiteral("flowRunId")).isString()
        || item.value(QStringLiteral("flowRunId")).toString() != runId.value) {
        setFlowHistoryError(
            error,
            historyServiceText("历史记录与本次流程运行编号不一致，已拒绝更新。")
        );
        return false;
    }

    QString allDetailReference;
    QString textReference;
    QString allTextReference;
    if (!referencePath(
            item,
            QStringLiteral("allDetailFile"),
            &allDetailReference)
        || !referencePath(
            item,
            QStringLiteral("textFile"),
            &textReference)
        || !referencePath(
            item,
            QStringLiteral("allTextFile"),
            &allTextReference)) {
        setFlowHistoryError(
            error,
            historyServiceText("历史详情中的关联文件路径格式无效。")
        );
        return false;
    }

    QString managedAllDetailPath;
    QString managedTextPath;
    QString managedAllTextPath;
    if ((!allDetailReference.isEmpty()
         && !resolveManagedExistingFile(
             allDetailReference,
             m_store.rootPath(),
             QStringLiteral("json"),
             &managedAllDetailPath))
        || (!textReference.isEmpty()
            && !resolveManagedExistingFile(
                textReference,
                m_store.rootPath(),
                QStringLiteral("txt"),
                &managedTextPath))
        || (!allTextReference.isEmpty()
            && !resolveManagedExistingFile(
                allTextReference,
                m_store.rootPath(),
                QStringLiteral("txt"),
                &managedAllTextPath))) {
        setFlowHistoryError(
            error,
            historyServiceText("历史详情引用了缺失或越出受管目录的关联文件。")
        );
        return false;
    }

    if (!managedAllDetailPath.isEmpty()) {
        QJsonObject mirror;
        if (!readJsonObjectFile(managedAllDetailPath, &mirror)
            || !mirror.value(QStringLiteral("flowRunId")).isString()
            || mirror.value(QStringLiteral("flowRunId")).toString()
                != runId.value) {
            setFlowHistoryError(
                error,
                historyServiceText("历史镜像与本次流程运行编号不一致，已拒绝更新。")
            );
            return false;
        }
    }

    QJsonObject updatedItem = item;
    updatedItem.insert(QStringLiteral("output"), editedText);
    const QByteArray detailBytes =
        QJsonDocument(updatedItem).toJson(QJsonDocument::Indented);
    const QByteArray textBytes =
        HistoryStore::textFromJsonObject(updatedItem).toUtf8();

    QVector<PendingHistoryWrite> writes;
    if (!addPendingWrite(&writes, managedDetailPath, detailBytes)
        || (!managedAllDetailPath.isEmpty()
            && !addPendingWrite(
                &writes,
                managedAllDetailPath,
                detailBytes))
        || (!managedTextPath.isEmpty()
            && !addPendingWrite(&writes, managedTextPath, textBytes))
        || (!managedAllTextPath.isEmpty()
            && !addPendingWrite(&writes, managedAllTextPath, textBytes))) {
        setFlowHistoryError(
            error,
            historyServiceText("历史关联文件无法读取，未执行编辑更新。")
        );
        return false;
    }

    QVector<HistoryEntry> entries;
    if (!m_store.readIndex(&entries)) {
        entries = m_store.scanEntries();
    }
    const QString normalizedDetail =
        HistoryStore::normalizedPathForCompare(managedDetailPath);
    const HistoryEntry updatedEntry = HistoryStore::entryFromJsonObject(
        updatedItem,
        managedDetailPath
    );
    bool replaced = false;
    for (HistoryEntry &entry : entries) {
        if (HistoryStore::normalizedPathForCompare(entry.filePath)
            == normalizedDetail) {
            entry = updatedEntry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        entries.append(updatedEntry);
    }
    HistoryStore::sortEntries(&entries);

    int writtenCount = 0;
    for (const PendingHistoryWrite &write : writes) {
        if (!writeBytesAtomically(write.path, write.after)) {
            restoreWrites(writes, writtenCount);
            setFlowHistoryError(
                error,
                historyServiceText("历史关联文件写入失败，编辑内容未保存。")
            );
            return false;
        }
        ++writtenCount;
    }

    if (!m_store.writeIndex(entries)) {
        restoreWrites(writes, writtenCount);
        setFlowHistoryError(
            error,
            historyServiceText("历史索引写入失败，编辑内容未保存。")
        );
        return false;
    }
    return true;
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
