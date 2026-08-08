#include "history_store.h"

#include "../file_utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegExp>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

QString hsTr8(const char *text)
{
    return QString::fromUtf8(text);
}

qint64 jsonInt64(const QJsonObject &object, const QString &key, qint64 fallback = -1)
{
    return static_cast<qint64>(object.value(key).toDouble(fallback));
}

QString normalizedRecordRoot(QString recordDirectory)
{
    recordDirectory = recordDirectory.trimmed();
    if (recordDirectory.isEmpty()) {
        recordDirectory = QDir::current().filePath(QStringLiteral("records"));
    }
    return QDir::cleanPath(recordDirectory);
}

QString mergedSegmentText(const QVector<RecordingSegment> &segments)
{
    QStringList parts;
    for (const RecordingSegment &segment : segments) {
        if (!segment.text.trimmed().isEmpty()) {
            parts << segment.text.trimmed();
        } else {
            parts << hsTr8("[第 %1 段识别失败]").arg(segment.index);
        }
    }
    return parts.join(QStringLiteral("\n"));
}

QJsonArray segmentItemsFromEntry(
    const HistoryEntry &entry,
    int *failedSegmentCount,
    QJsonArray *failedSegments
)
{
    if (failedSegmentCount) {
        *failedSegmentCount = 0;
    }
    if (failedSegments) {
        *failedSegments = QJsonArray();
    }

    QJsonArray segmentItems;
    for (const RecordingSegment &segment : entry.segments) {
        if (segment.text.trimmed().isEmpty()) {
            if (failedSegmentCount) {
                ++(*failedSegmentCount);
            }
            if (failedSegments) {
                failedSegments->append(segment.index);
            }
        }

        QJsonObject segmentItem;
        segmentItem.insert(QStringLiteral("index"), segment.index);
        segmentItem.insert(QStringLiteral("audio"), segment.wavPath);
        segmentItem.insert(QStringLiteral("text"), segment.text);
        segmentItem.insert(QStringLiteral("error"), segment.error);
        segmentItem.insert(
            QStringLiteral("recognitionElapsedMs"),
            static_cast<double>(segment.recognitionElapsedMs)
        );
        segmentItem.insert(QStringLiteral("attempts"), segment.attempts);
        segmentItems.append(segmentItem);
    }
    return segmentItems;
}

} // namespace

HistoryStore::HistoryStore(const QString &recordDirectory)
    : m_recordDirectory(normalizedRecordRoot(recordDirectory))
{
}

QString HistoryStore::rootPath() const
{
    return m_recordDirectory;
}

QString HistoryStore::backupDirectory() const
{
    return QDir(rootPath()).filePath(backupFolderName());
}

QString HistoryStore::allDateDirectory(const QString &folderName, const QString &date) const
{
    return QDir(QDir(rootPath()).filePath(folderName)).filePath(date);
}

QString HistoryStore::modeDirectory(const QString &modeTitle) const
{
    return QDir(rootPath()).filePath(safeFileNamePart(modeTitle, hsTr8("未命名功能")));
}

QString HistoryStore::modeDateDirectory(const QString &modeTitle, const QString &date) const
{
    return QDir(modeDirectory(modeTitle)).filePath(date);
}

QString HistoryStore::modeDateSubDirectory(
    const QString &modeTitle,
    const QString &date,
    const QString &subFolder
) const
{
    return QDir(modeDateDirectory(modeTitle, date)).filePath(subFolder);
}

QString HistoryStore::indexPath() const
{
    return QDir(rootPath()).filePath(QStringLiteral("history_index.json"));
}

void HistoryStore::ensureRootStructure() const
{
    QDir root(rootPath());
    root.mkpath(QStringLiteral("."));
    root.mkpath(backupFolderName());
    root.mkpath(allAudioFolderName());
    root.mkpath(allTextFolderName());
    root.mkpath(allDetailFolderName());
}

void HistoryStore::ensureModeDateStructure(const QString &modeTitle, const QString &date) const
{
    ensureRootStructure();
    QDir().mkpath(modeDateSubDirectory(modeTitle, date, textSubFolderName()));
    QDir().mkpath(modeDateSubDirectory(modeTitle, date, audioSubFolderName()));
    QDir().mkpath(modeDateSubDirectory(modeTitle, date, detailSubFolderName()));
    QDir().mkpath(allDateDirectory(allAudioFolderName(), date));
    QDir().mkpath(allDateDirectory(allTextFolderName(), date));
    QDir().mkpath(allDateDirectory(allDetailFolderName(), date));
}

QVector<HistoryEntry> HistoryStore::loadEntries() const
{
    QVector<HistoryEntry> entries;
    if (readIndex(&entries)) {
        return entries;
    }
    entries = scanEntries();
    writeIndex(entries);
    return entries;
}

QVector<HistoryEntry> HistoryStore::scanEntries() const
{
    QVector<HistoryEntry> entries;
    QDir root(rootPath());
    if (!root.exists()) {
        return entries;
    }

    const QFileInfoList modeDirs =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &modeDirInfo : modeDirs) {
        if (isReservedRootFolder(modeDirInfo.fileName()) || isDateFolderName(modeDirInfo.fileName())) {
            continue;
        }
        QDir modeDir(modeDirInfo.absoluteFilePath());
        const QFileInfoList dayDirs =
            modeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QFileInfo &dayInfo : dayDirs) {
            if (!isDateFolderName(dayInfo.fileName())) {
                continue;
            }
            QDir detailDir(QDir(dayInfo.absoluteFilePath()).filePath(detailSubFolderName()));
            const QFileInfoList files =
                detailDir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Time);
            for (const QFileInfo &fileInfo : files) {
                QJsonObject item;
                if (!readJsonObjectFile(fileInfo.absoluteFilePath(), &item)) {
                    continue;
                }
                entries.append(entryFromJsonObject(item, fileInfo.absoluteFilePath()));
            }
        }
    }

    sortEntries(&entries);
    return entries;
}

bool HistoryStore::readIndex(QVector<HistoryEntry> *entries) const
{
    if (!entries) {
        return false;
    }
    entries->clear();

    QJsonObject root;
    if (!readJsonObjectFile(indexPath(), &root)) {
        return false;
    }
    if (!root.contains(QStringLiteral("entries")) || !root.value(QStringLiteral("entries")).isArray()) {
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("entries")).toArray();
    if (items.isEmpty() && rootHasDetailJsonFiles()) {
        return false;
    }
    for (const QJsonValue &value : items) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString filePath = object.value(QStringLiteral("sourceFile")).toString();
        if (filePath.trimmed().isEmpty()
            || !QFileInfo::exists(filePath)
            || !pathIsSameOrInside(filePath, rootPath())) {
            continue;
        }
        entries->append(entryFromJsonObject(object, filePath));
    }
    if (!items.isEmpty() && entries->isEmpty()) {
        return false;
    }
    sortEntries(entries);
    return true;
}

bool HistoryStore::writeIndex(const QVector<HistoryEntry> &entries) const
{
    QJsonArray items;
    for (const HistoryEntry &entry : entries) {
        if (!entry.filePath.trimmed().isEmpty() && QFileInfo::exists(entry.filePath)) {
            items.append(entryToIndexObject(entry));
        }
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("recordCount"), items.size());
    root.insert(QStringLiteral("entries"), items);
    return writeBytesAtomically(indexPath(), QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void HistoryStore::rebuildIndex() const
{
    writeIndex(scanEntries());
}

void HistoryStore::appendIndexEntry(const HistoryEntry &entry) const
{
    QVector<HistoryEntry> entries;
    if (!readIndex(&entries)) {
        entries = scanEntries();
    }
    auto existing = std::find_if(entries.begin(), entries.end(), [&entry](const HistoryEntry &item) {
        return item.filePath == entry.filePath;
    });
    if (existing != entries.end()) {
        *existing = entry;
    } else {
        entries.append(entry);
    }
    sortEntries(&entries);
    writeIndex(entries);
}

HistoryAppendResult HistoryStore::appendRecord(const HistoryAppendRequest &request) const
{
    HistoryAppendResult result;
    const QDateTime savedAt = QDateTime::currentDateTime();
    const QString date = savedAt.date().toString(QStringLiteral("yyyy-MM-dd"));
    const QString modeId = request.modeId.trimmed().isEmpty()
        ? request.item.value(QStringLiteral("modeId")).toString(QStringLiteral("mode"))
        : request.modeId.trimmed();
    const QString modeTitle = request.modeTitle.trimmed().isEmpty()
        ? request.item.value(QStringLiteral("mode")).toString(hsTr8("未命名功能"))
        : request.modeTitle.trimmed();

    ensureModeDateStructure(modeTitle, date);
    const QString fileBase = savedAt.toString(QStringLiteral("HHmmss_zzz_"))
        + safeFileNamePart(modeId, QStringLiteral("mode"))
        + (request.draft ? QStringLiteral("_draft") : QString());

    result.modeTextPath = QDir(modeDateSubDirectory(modeTitle, date, textSubFolderName()))
        .filePath(fileBase + QStringLiteral(".txt"));
    result.modeDetailPath = QDir(modeDateSubDirectory(modeTitle, date, detailSubFolderName()))
        .filePath(fileBase + QStringLiteral(".json"));
    result.allTextPath = QDir(allDateDirectory(allTextFolderName(), date))
        .filePath(fileBase + QStringLiteral(".txt"));
    result.allDetailPath = QDir(allDateDirectory(allDetailFolderName(), date))
        .filePath(fileBase + QStringLiteral(".json"));

    if (!request.sourceAudioPath.trimmed().isEmpty()
        && QFileInfo::exists(request.sourceAudioPath)) {
        const QString allAudioDir = allDateDirectory(allAudioFolderName(), date);
        QDir().mkpath(allAudioDir);
        result.allAudioPath = uniqueFilePath(
            QDir(allAudioDir).filePath(QFileInfo(request.sourceAudioPath).fileName())
        );
        if (!QFile::copy(request.sourceAudioPath, result.allAudioPath)) {
            result.allAudioPath.clear();
        }
    }

    result.item = request.item;
    result.item.insert(QStringLiteral("modeId"), modeId);
    result.item.insert(QStringLiteral("mode"), modeTitle);
    result.item.insert(QStringLiteral("time"), savedAt.toString(Qt::ISODate));
    result.item.insert(QStringLiteral("audio"), request.sourceAudioPath);
    result.item.insert(QStringLiteral("textFile"), result.modeTextPath);
    result.item.insert(QStringLiteral("allAudioFile"), result.allAudioPath);
    result.item.insert(QStringLiteral("allTextFile"), result.allTextPath);
    result.item.insert(QStringLiteral("allDetailFile"), result.allDetailPath);
    if (!result.item.contains(QStringLiteral("favorite"))) {
        result.item.insert(QStringLiteral("favorite"), false);
    }
    result.item.insert(QStringLiteral("draft"), request.draft);

    const QString readableText = textFromJsonObject(result.item);
    const bool modeTextOk = writeTextFile(result.modeTextPath, readableText);
    const bool allTextOk = writeTextFile(result.allTextPath, readableText);
    const QByteArray json = QJsonDocument(result.item).toJson(QJsonDocument::Indented);
    const bool modeDetailOk = writeBytesAtomically(result.modeDetailPath, json);
    const bool allDetailOk = writeBytesAtomically(result.allDetailPath, json);
    result.ok = modeTextOk && allTextOk && modeDetailOk && allDetailOk;

    if (modeDetailOk) {
        appendIndexEntry(entryFromJsonObject(result.item, result.modeDetailPath));
    }
    return result;
}

bool HistoryStore::updateRetriedSegment(
    HistoryEntry *entry,
    const HistorySegmentRetryResult &result
) const
{
    if (!entry || result.index <= 0) {
        return false;
    }

    bool found = false;
    qint64 speechElapsedMs = 0;
    for (RecordingSegment &segment : entry->segments) {
        if (segment.index == result.index) {
            segment.text = result.text.trimmed();
            segment.error = segment.text.isEmpty() ? result.error.trimmed() : QString();
            segment.recognitionElapsedMs = result.elapsedMs;
            ++segment.attempts;
            found = true;
        }
        if (segment.recognitionElapsedMs >= 0) {
            speechElapsedMs += segment.recognitionElapsedMs;
        }
    }
    if (!found) {
        return false;
    }

    int failedSegmentCount = 0;
    QJsonArray failedSegments;
    const QJsonArray segmentItems =
        segmentItemsFromEntry(*entry, &failedSegmentCount, &failedSegments);

    entry->speechElapsedMs = speechElapsedMs;
    const QString mergedText = mergedSegmentText(entry->segments);
    const QString marker = hsTr8("语音输入：\n");
    const int markerIndex = entry->input.indexOf(marker);
    entry->input = markerIndex >= 0
        ? entry->input.left(markerIndex + marker.size()) + mergedText
        : mergedText;
    if (failedSegmentCount < entry->segments.size()
        && entry->error.contains(hsTr8("所有录音分段"))) {
        entry->error.clear();
    }

    QJsonObject item;
    if (!readJsonObjectFile(entry->filePath, &item)) {
        return false;
    }
    item.insert(QStringLiteral("input"), entry->input);
    item.insert(QStringLiteral("error"), entry->error);
    item.insert(
        QStringLiteral("speechElapsedMs"),
        static_cast<double>(entry->speechElapsedMs)
    );
    item.insert(QStringLiteral("failedSegmentCount"), failedSegmentCount);
    item.insert(QStringLiteral("failedSegments"), failedSegments);
    item.insert(QStringLiteral("segments"), segmentItems);

    const QByteArray json = QJsonDocument(item).toJson(QJsonDocument::Indented);
    QStringList detailPaths;
    detailPaths << entry->filePath << entry->allDetailFile;
    detailPaths.removeDuplicates();
    for (const QString &path : detailPaths) {
        if (!path.trimmed().isEmpty() && !writeBytesAtomically(path, json)) {
            return false;
        }
    }

    const QString readableText = textFromJsonObject(item);
    QStringList textPaths;
    textPaths << entry->textFile << entry->allTextFile;
    textPaths.removeDuplicates();
    for (const QString &path : textPaths) {
        if (!path.trimmed().isEmpty() && !writeTextFile(path, readableText)) {
            return false;
        }
    }

    appendIndexEntry(*entry);
    return true;
}

HistoryEntry HistoryStore::entryFromFile(const QString &filePath)
{
    QJsonObject item;
    if (!readJsonObjectFile(filePath, &item)) {
        return HistoryEntry();
    }
    return entryFromJsonObject(item, filePath);
}

QStringList HistoryStore::relatedFilesForEntry(const HistoryEntry &entry)
{
    QStringList files;
    QSet<QString> seen;
    const QStringList candidates = QStringList()
        << entry.audio
        << entry.allAudioFile
        << entry.textFile
        << entry.allTextFile
        << entry.filePath
        << entry.allDetailFile;
    for (const QString &path : candidates) {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QString normalized = normalizedPathForCompare(trimmed);
        if (seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);
        files.append(trimmed);
    }
    return files;
}

bool HistoryStore::removeEntryFiles(const HistoryEntry &entry)
{
    bool ok = true;
    const QStringList files = relatedFilesForEntry(entry);
    for (const QString &path : files) {
        if (QFileInfo::exists(path)) {
            ok = QFile::remove(path) && ok;
        }
    }
    return ok;
}

QString HistoryStore::safeFileNamePart(const QString &text, const QString &fallback)
{
    QString safe = text.trimmed();
    safe.replace(QRegExp(QStringLiteral("[\\\\/:*?\"<>|\\r\\n\\t]+")), QStringLiteral("_"));
    safe.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral(" "));
    safe = safe.trimmed();
    if (safe.isEmpty()) {
        safe = fallback;
    }
    if (safe.size() > 72) {
        safe = safe.left(72).trimmed();
    }
    return safe;
}

QString HistoryStore::backupFolderName() { return hsTr8("备份文件"); }
QString HistoryStore::allAudioFolderName() { return hsTr8("总录音文件"); }
QString HistoryStore::allTextFolderName() { return hsTr8("总文本文件"); }
QString HistoryStore::allDetailFolderName() { return hsTr8("总详细记录文件"); }
QString HistoryStore::textSubFolderName() { return hsTr8("文本记录"); }
QString HistoryStore::audioSubFolderName() { return hsTr8("录音记录"); }
QString HistoryStore::detailSubFolderName() { return hsTr8("详细记录"); }

bool HistoryStore::isDateFolderName(const QString &name)
{
    return QRegExp(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$")).exactMatch(name);
}

bool HistoryStore::isReservedRootFolder(const QString &name)
{
    return name == backupFolderName()
        || name == allAudioFolderName()
        || name == allTextFolderName()
        || name == allDetailFolderName();
}

QString HistoryStore::normalizedPathForCompare(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath())
        .replace(QChar('\\'), QChar('/'))
        .toLower();
}

bool HistoryStore::pathIsSameOrInside(const QString &candidate, const QString &parent)
{
    const QString childPath = normalizedPathForCompare(candidate);
    const QString parentPath = normalizedPathForCompare(parent);
    return childPath == parentPath || childPath.startsWith(parentPath + QStringLiteral("/"));
}

HistoryEntry HistoryStore::entryFromJsonObject(const QJsonObject &item, const QString &filePath)
{
    HistoryEntry entry;
    entry.modeId = item.value(QStringLiteral("modeId")).toString();
    entry.mode = item.value(QStringLiteral("mode")).toString();
    entry.time = item.value(QStringLiteral("time")).toString();
    entry.input = item.value(QStringLiteral("input")).toString();
    entry.output = item.value(QStringLiteral("output")).toString();
    entry.error = item.value(QStringLiteral("error")).toString();
    entry.audio = item.value(QStringLiteral("audio")).toString();
    entry.textFile = item.value(QStringLiteral("textFile")).toString();
    entry.allAudioFile = item.value(QStringLiteral("allAudioFile")).toString();
    entry.allTextFile = item.value(QStringLiteral("allTextFile")).toString();
    entry.allDetailFile = item.value(QStringLiteral("allDetailFile")).toString();
    entry.model = item.value(QStringLiteral("model")).toString();
    entry.elapsedMs = jsonInt64(item, QStringLiteral("elapsedMs"));
    entry.speechElapsedMs = jsonInt64(item, QStringLiteral("speechElapsedMs"));
    entry.modelElapsedMs = jsonInt64(item, QStringLiteral("modelElapsedMs"));
    entry.recordingTriggerMode = item.value(QStringLiteral("recordingTriggerMode")).toString();
    entry.longRecording = item.value(QStringLiteral("longRecording")).toBool(false);

    const QJsonArray recordingSegments = item.value(QStringLiteral("segments")).toArray();
    for (const QJsonValue &value : recordingSegments) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject segmentItem = value.toObject();
        RecordingSegment segment;
        segment.index = segmentItem.value(QStringLiteral("index")).toInt();
        segment.wavPath = segmentItem.value(QStringLiteral("audio")).toString();
        segment.text = segmentItem.value(QStringLiteral("text")).toString();
        segment.error = segmentItem.value(QStringLiteral("error")).toString();
        segment.recognitionElapsedMs = jsonInt64(segmentItem, QStringLiteral("recognitionElapsedMs"));
        segment.attempts = segmentItem.value(QStringLiteral("attempts")).toInt();
        if (segment.index > 0) {
            entry.segments.append(segment);
        }
    }

    entry.ocrEngine = item.value(QStringLiteral("ocrEngine")).toString();
    const QJsonArray ocrLanguages = item.value(QStringLiteral("ocrLanguages")).toArray();
    for (const QJsonValue &language : ocrLanguages) {
        if (language.isString()) {
            entry.ocrLanguages.append(language.toString());
        }
    }
    entry.ocrElapsedMs = jsonInt64(item, QStringLiteral("ocrElapsedMs"));
    entry.ocrUsedFallback = item.value(QStringLiteral("ocrUsedFallback")).toBool(false);
    entry.imageFileName = item.value(QStringLiteral("imageFileName")).toString();
    entry.promptVersion = item.value(QStringLiteral("promptVersion")).toString();
    entry.favorite = item.value(QStringLiteral("favorite")).toBool(false);
    entry.favoriteFolder = item.value(QStringLiteral("favoriteFolder")).toString();
    entry.draft = item.value(QStringLiteral("draft")).toBool(false);
    entry.flowRunId =
        item.value(QStringLiteral("flowRunId")).toString();
    entry.flowPublishedRevision = item.value(
        QStringLiteral("flowPublishedRevision")
    ).toInt(0);
    entry.flowPublishedHash = item.value(
        QStringLiteral("flowPublishedHash")
    ).toString();
    entry.flowTrigger =
        item.value(QStringLiteral("flowTrigger")).toString();
    entry.flowFailedNodeId = item.value(
        QStringLiteral("flowFailedNodeId")
    ).toString();
    entry.flowFailedNodeType = item.value(
        QStringLiteral("flowFailedNodeType")
    ).toString();
    entry.flowCancelled = item.value(
        QStringLiteral("flowCancelled")
    ).toBool(false);
    const QJsonArray flowTraces = item.value(
        QStringLiteral("flowNodeTraces")
    ).toArray();
    for (const QJsonValue &value : flowTraces) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject traceItem = value.toObject();
        HistoryFlowNodeTrace trace;
        trace.nodeId =
            traceItem.value(QStringLiteral("nodeId")).toString();
        trace.nodeType =
            traceItem.value(QStringLiteral("nodeType")).toString();
        trace.state =
            traceItem.value(QStringLiteral("state")).toString();
        trace.elapsedMs = jsonInt64(
            traceItem,
            QStringLiteral("elapsedMs")
        );
        trace.errorCode = traceItem.value(
            QStringLiteral("errorCode")
        ).toString();
        trace.modelId =
            traceItem.value(QStringLiteral("modelId")).toString();
        trace.promptVersion = traceItem.value(
            QStringLiteral("promptVersion")
        ).toString();
        entry.flowNodeTraces.append(trace);
    }
    entry.filePath = filePath;
    return entry;
}

QJsonObject HistoryStore::entryToIndexObject(const HistoryEntry &entry)
{
    QJsonObject item;
    item.insert(QStringLiteral("modeId"), entry.modeId);
    item.insert(QStringLiteral("mode"), entry.mode);
    item.insert(QStringLiteral("time"), entry.time);
    item.insert(QStringLiteral("input"), entry.input);
    item.insert(QStringLiteral("output"), entry.output);
    item.insert(QStringLiteral("error"), entry.error);
    item.insert(QStringLiteral("audio"), entry.audio);
    item.insert(QStringLiteral("textFile"), entry.textFile);
    item.insert(QStringLiteral("allAudioFile"), entry.allAudioFile);
    item.insert(QStringLiteral("allTextFile"), entry.allTextFile);
    item.insert(QStringLiteral("allDetailFile"), entry.allDetailFile);
    item.insert(QStringLiteral("model"), entry.model);
    item.insert(QStringLiteral("elapsedMs"), static_cast<double>(entry.elapsedMs));
    item.insert(QStringLiteral("speechElapsedMs"), static_cast<double>(entry.speechElapsedMs));
    item.insert(QStringLiteral("modelElapsedMs"), static_cast<double>(entry.modelElapsedMs));
    item.insert(QStringLiteral("recordingTriggerMode"), entry.recordingTriggerMode);
    item.insert(QStringLiteral("longRecording"), entry.longRecording);

    QJsonArray recordingSegments;
    for (const RecordingSegment &segment : entry.segments) {
        QJsonObject segmentItem;
        segmentItem.insert(QStringLiteral("index"), segment.index);
        segmentItem.insert(QStringLiteral("audio"), segment.wavPath);
        segmentItem.insert(QStringLiteral("text"), segment.text);
        segmentItem.insert(QStringLiteral("error"), segment.error);
        segmentItem.insert(
            QStringLiteral("recognitionElapsedMs"),
            static_cast<double>(segment.recognitionElapsedMs)
        );
        segmentItem.insert(QStringLiteral("attempts"), segment.attempts);
        recordingSegments.append(segmentItem);
    }
    item.insert(QStringLiteral("segments"), recordingSegments);
    item.insert(QStringLiteral("ocrEngine"), entry.ocrEngine);
    item.insert(QStringLiteral("ocrLanguages"), QJsonArray::fromStringList(entry.ocrLanguages));
    item.insert(QStringLiteral("ocrElapsedMs"), static_cast<double>(entry.ocrElapsedMs));
    item.insert(QStringLiteral("ocrUsedFallback"), entry.ocrUsedFallback);
    item.insert(QStringLiteral("imageFileName"), entry.imageFileName);
    item.insert(QStringLiteral("promptVersion"), entry.promptVersion);
    item.insert(QStringLiteral("favorite"), entry.favorite);
    item.insert(QStringLiteral("favoriteFolder"), entry.favoriteFolder);
    item.insert(QStringLiteral("draft"), entry.draft);
    if (!entry.flowRunId.trimmed().isEmpty()) {
        item.insert(QStringLiteral("flowRunId"), entry.flowRunId);
        item.insert(
            QStringLiteral("flowPublishedRevision"),
            entry.flowPublishedRevision
        );
        item.insert(
            QStringLiteral("flowPublishedHash"),
            entry.flowPublishedHash
        );
        item.insert(
            QStringLiteral("flowTrigger"),
            entry.flowTrigger
        );
        item.insert(
            QStringLiteral("flowFailedNodeId"),
            entry.flowFailedNodeId
        );
        item.insert(
            QStringLiteral("flowFailedNodeType"),
            entry.flowFailedNodeType
        );
        item.insert(
            QStringLiteral("flowCancelled"),
            entry.flowCancelled
        );
        QJsonArray traces;
        for (const HistoryFlowNodeTrace &trace :
             entry.flowNodeTraces) {
            QJsonObject traceItem;
            traceItem.insert(
                QStringLiteral("nodeId"),
                trace.nodeId
            );
            traceItem.insert(
                QStringLiteral("nodeType"),
                trace.nodeType
            );
            traceItem.insert(
                QStringLiteral("state"),
                trace.state
            );
            traceItem.insert(
                QStringLiteral("elapsedMs"),
                static_cast<double>(trace.elapsedMs)
            );
            traceItem.insert(
                QStringLiteral("errorCode"),
                trace.errorCode
            );
            traceItem.insert(
                QStringLiteral("modelId"),
                trace.modelId
            );
            traceItem.insert(
                QStringLiteral("promptVersion"),
                trace.promptVersion
            );
            traces.append(traceItem);
        }
        item.insert(QStringLiteral("flowNodeTraces"), traces);
    }
    item.insert(QStringLiteral("sourceFile"), entry.filePath);
    return item;
}

QString HistoryStore::textFromJsonObject(const QJsonObject &item)
{
    const QString timeText =
        QString(item.value(QStringLiteral("time")).toString()).replace(QStringLiteral("T"), QStringLiteral(" "));
    const auto elapsedText = [&item](const char *key) {
        const qint64 value = jsonInt64(item, QString::fromLatin1(key));
        return value < 0 ? hsTr8("未记录") : QString::number(value) + hsTr8(" 毫秒");
    };

    QStringList parts;
    parts << hsTr8("功能：") + item.value(QStringLiteral("mode")).toString();
    parts << hsTr8("时间：") + timeText;
    parts << hsTr8("总耗时：") + elapsedText("elapsedMs");
    parts << hsTr8("语音识别耗时：") + elapsedText("speechElapsedMs");
    parts << hsTr8("模型耗时：") + elapsedText("modelElapsedMs");
    if (item.value(QStringLiteral("modeId")).toString() == QStringLiteral("ocr")) {
        parts << hsTr8("图片识别引擎：") + item.value(QStringLiteral("ocrEngine")).toString();
        parts << hsTr8("图片识别耗时：") + elapsedText("ocrElapsedMs");
        parts << hsTr8("自动回退：")
            + (item.value(QStringLiteral("ocrUsedFallback")).toBool(false) ? hsTr8("是") : hsTr8("否"));
        parts << hsTr8("图片文件名：") + item.value(QStringLiteral("imageFileName")).toString();
    }
    if (item.value(QStringLiteral("longRecording")).toBool(false)) {
        parts << hsTr8("录音方式：")
            + (item.value(QStringLiteral("recordingTriggerMode")).toString() == QStringLiteral("hold")
                ? hsTr8("按住说话")
                : hsTr8("切换开始和结束"));
        parts << hsTr8("录音分段：")
            + hsTr8("%1 段，%2 段失败")
                .arg(item.value(QStringLiteral("segmentCount")).toInt())
                .arg(item.value(QStringLiteral("failedSegmentCount")).toInt());
    }
    parts << hsTr8("提示词版本：")
        + (item.value(QStringLiteral("promptVersion")).toString().trimmed().isEmpty()
            ? hsTr8("未记录")
            : item.value(QStringLiteral("promptVersion")).toString());
    if (!item.value(QStringLiteral("flowRunId"))
             .toString().trimmed().isEmpty()) {
        parts << hsTr8("流程运行：")
            + item.value(QStringLiteral("flowRunId")).toString();
        parts << hsTr8("发布版本：")
            + QString::number(item.value(
                QStringLiteral("flowPublishedRevision")
            ).toInt());
        parts << hsTr8("发布哈希：")
            + item.value(
                QStringLiteral("flowPublishedHash")
            ).toString();
        parts << hsTr8("触发入口：")
            + item.value(QStringLiteral("flowTrigger")).toString();
    }
    parts << hsTr8("录音：")
        + (item.value(QStringLiteral("audio")).toString().trimmed().isEmpty()
            ? hsTr8("本次没有录音")
            : item.value(QStringLiteral("audio")).toString());
    parts << hsTr8("输入内容：\n")
        + (item.value(QStringLiteral("input")).toString().trimmed().isEmpty()
            ? hsTr8("无")
            : item.value(QStringLiteral("input")).toString());
    parts << hsTr8("模型输出：\n")
        + (item.value(QStringLiteral("output")).toString().trimmed().isEmpty()
            ? hsTr8("无")
            : item.value(QStringLiteral("output")).toString());
    parts << hsTr8("错误：\n")
        + (item.value(QStringLiteral("error")).toString().trimmed().isEmpty()
            ? hsTr8("无")
            : item.value(QStringLiteral("error")).toString());
    return parts.join(QStringLiteral("\n\n"));
}

void HistoryStore::sortEntries(QVector<HistoryEntry> *entries)
{
    if (!entries) {
        return;
    }
    std::sort(entries->begin(), entries->end(), [](const HistoryEntry &a, const HistoryEntry &b) {
        return a.time > b.time;
    });
}

bool HistoryStore::rootHasDetailJsonFiles() const
{
    QDir root(rootPath());
    if (!root.exists()) {
        return false;
    }

    const QFileInfoList modeDirs =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    return std::any_of(modeDirs.begin(), modeDirs.end(), [](const QFileInfo &modeDirInfo) {
        if (isReservedRootFolder(modeDirInfo.fileName()) || isDateFolderName(modeDirInfo.fileName())) {
            return false;
        }
        QDir modeDir(modeDirInfo.absoluteFilePath());
        const QFileInfoList dayDirs =
            modeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        return std::any_of(dayDirs.begin(), dayDirs.end(), [](const QFileInfo &dayInfo) {
            if (!isDateFolderName(dayInfo.fileName())) {
                return false;
            }
            QDir detailDir(QDir(dayInfo.absoluteFilePath()).filePath(detailSubFolderName()));
            return !detailDir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files).isEmpty();
        });
    });
}
