#include "../../src/file_utils.h"
#include "../../src/domain/voice_history_recorder.h"
#include "../../src/domain/voice_history_request_builder.h"
#include "../../src/storage/history_record_service.h"
#include "../../src/storage/history_store.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool pathIsDirectory(const QString &path)
{
    return QFileInfo(path).isDir();
}

QByteArray readFileBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

ExecutionId executionId(const QString &value)
{
    ExecutionId result;
    result.value = value;
    return result;
}

QJsonObject makeHistoryJson(
    const QString &time,
    const QString &input,
    const QString &output,
    const QString &error = QString()
)
{
    QJsonObject item;
    item.insert(QStringLiteral("modeId"), QStringLiteral("dictate"));
    item.insert(QStringLiteral("mode"), tr8("听写"));
    item.insert(QStringLiteral("time"), time);
    item.insert(QStringLiteral("input"), input);
    item.insert(QStringLiteral("output"), output);
    item.insert(QStringLiteral("error"), error);
    item.insert(QStringLiteral("audio"), QString());
    item.insert(QStringLiteral("model"), QStringLiteral("deepseek-v4-flash"));
    item.insert(QStringLiteral("elapsedMs"), 1200.0);
    item.insert(QStringLiteral("speechElapsedMs"), 300.0);
    item.insert(QStringLiteral("modelElapsedMs"), 900.0);
    item.insert(QStringLiteral("promptVersion"), QStringLiteral("dictate-v1"));
    return item;
}

bool writeDetailFile(
    const HistoryStore &store,
    const QString &date,
    const QString &fileName,
    const QJsonObject &item,
    QString *path
)
{
    const QString detailDir =
        store.modeDateSubDirectory(tr8("听写"), date, HistoryStore::detailSubFolderName());
    QDir().mkpath(detailDir);
    if (path) {
        *path = QDir(detailDir).filePath(fileName);
    }
    const QByteArray json = QJsonDocument(item).toJson(QJsonDocument::Indented);
    return path && writeBytesAtomically(*path, json);
}

} // namespace

class HistoryStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void createsStructuredDirectories();
    void appendsRecordFilesAndIndex();
    void scansDetailsAndWritesIndex();
    void rebuildsWhenIndexOnlyHasStaleFiles();
    void appendIndexEntryReplacesExistingFile();
    void updateRetriedSegmentSynchronizesMirrorsAndIndex();
    void relatedFilesAreDeduplicated();
    void removeEntryFilesDeletesExistingFiles();
    void voiceHistoryBuilderNormalizesControllerState();
    void voiceHistoryRecorderSavesAndReportsLogFields();
    void recordServiceBuildsMetadataAndAppendsRecord();
    void recordServiceSavesOcrRecord();
    void recordServiceUpdatesFavoriteAndIndex();
    void recordServiceRemovesEntriesAndRebuildsIndex();
    void recordServiceRejectsEntryOutsideRoot();
    void recordServiceUpdatesFlowEditedTextEverywhere();
    void recordServiceAllowsLegacyEmptyFlowReferences();
    void recordServiceRejectsFlowEditOutsideRoot();
    void recordServiceRejectsMissingFlowDetail();
    void recordServiceRejectsFlowEditForWrongRun();
    void recordServiceRejectsUnsafeFlowEditReference_data();
    void recordServiceRejectsUnsafeFlowEditReference();
    void recordServiceRejectsMissingFlowEditReference();
    void recordServiceRejectsMismatchedFlowMirror();
    void flowMetadataPreservesFullHashAndSafeTrace();

private:
    HistoryAppendResult appendFlowRecord(
        const QString &recordDirectory,
        const QString &runId
    );
};

HistoryAppendResult HistoryStoreTests::appendFlowRecord(
    const QString &recordDirectory,
    const QString &runId
)
{
    HistoryAppendRequest request;
    request.modeId = QStringLiteral("custom_flow");
    request.modeTitle = QStringLiteral("Flow");
    request.item.insert(QStringLiteral("input"), QStringLiteral("canonical input"));
    request.item.insert(QStringLiteral("output"), QStringLiteral("original output"));
    request.item.insert(QStringLiteral("error"), QString());
    request.item.insert(QStringLiteral("flowRunId"), runId);
    return HistoryStore(recordDirectory).appendRecord(request);
}

void HistoryStoreTests::createsStructuredDirectories()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    store.ensureModeDateStructure(tr8("听写"), QStringLiteral("2026-06-24"));

    QVERIFY(pathIsDirectory(store.backupDirectory()));
    QVERIFY(pathIsDirectory(QDir(store.rootPath()).filePath(HistoryStore::allAudioFolderName())));
    QVERIFY(pathIsDirectory(QDir(store.rootPath()).filePath(HistoryStore::allTextFolderName())));
    QVERIFY(pathIsDirectory(QDir(store.rootPath()).filePath(HistoryStore::allDetailFolderName())));
    QVERIFY(pathIsDirectory(store.modeDateSubDirectory(
        tr8("听写"),
        QStringLiteral("2026-06-24"),
        HistoryStore::textSubFolderName()
    )));
    QVERIFY(pathIsDirectory(store.modeDateSubDirectory(
        tr8("听写"),
        QStringLiteral("2026-06-24"),
        HistoryStore::audioSubFolderName()
    )));
    QVERIFY(pathIsDirectory(store.modeDateSubDirectory(
        tr8("听写"),
        QStringLiteral("2026-06-24"),
        HistoryStore::detailSubFolderName()
    )));
}

void HistoryStoreTests::appendsRecordFilesAndIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    const QString audioPath = QDir(temp.path()).filePath(QStringLiteral("source.wav"));
    QVERIFY(writeBytesAtomically(audioPath, QByteArray("sample audio")));

    HistoryAppendRequest request;
    request.modeId = QStringLiteral("translate");
    request.modeTitle = tr8("翻译");
    request.sourceAudioPath = audioPath;
    request.item.insert(QStringLiteral("input"), QStringLiteral("hello"));
    request.item.insert(QStringLiteral("output"), tr8("你好"));
    request.item.insert(QStringLiteral("error"), QString());
    request.item.insert(QStringLiteral("model"), QStringLiteral("deepseek-v4-flash"));
    request.item.insert(QStringLiteral("elapsedMs"), 42.0);

    const HistoryAppendResult result = store.appendRecord(request);

    QVERIFY(result.ok);
    QCOMPARE(result.item.value(QStringLiteral("modeId")).toString(), QStringLiteral("translate"));
    QCOMPARE(result.item.value(QStringLiteral("mode")).toString(), tr8("翻译"));
    QCOMPARE(result.item.value(QStringLiteral("input")).toString(), QStringLiteral("hello"));
    QCOMPARE(result.item.value(QStringLiteral("output")).toString(), tr8("你好"));
    QCOMPARE(result.item.value(QStringLiteral("audio")).toString(), audioPath);
    QCOMPARE(result.item.value(QStringLiteral("allAudioFile")).toString(), result.allAudioPath);
    QVERIFY(QFileInfo::exists(result.modeTextPath));
    QVERIFY(QFileInfo::exists(result.modeDetailPath));
    QVERIFY(QFileInfo::exists(result.allAudioPath));
    QVERIFY(QFileInfo::exists(result.allTextPath));
    QVERIFY(QFileInfo::exists(result.allDetailPath));
    QVERIFY(readTextFile(result.modeTextPath).contains(tr8("你好")));

    const QVector<HistoryEntry> entries = store.loadEntries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().modeId, QStringLiteral("translate"));
    QCOMPARE(entries.first().mode, tr8("翻译"));
    QCOMPARE(entries.first().output, tr8("你好"));
    QCOMPARE(entries.first().filePath, result.modeDetailPath);
}

void HistoryStoreTests::scansDetailsAndWritesIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    store.ensureModeDateStructure(tr8("听写"), QStringLiteral("2026-06-24"));
    QString detailPath;
    QVERIFY(writeDetailFile(
        store,
        QStringLiteral("2026-06-24"),
        QStringLiteral("one.json"),
        makeHistoryJson(QStringLiteral("2026-06-24T10:00:00"), tr8("原始输入"), tr8("模型输出")),
        &detailPath
    ));

    const QVector<HistoryEntry> entries = store.loadEntries();

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().mode, tr8("听写"));
    QCOMPARE(entries.first().input, tr8("原始输入"));
    QCOMPARE(entries.first().output, tr8("模型输出"));
    QCOMPARE(entries.first().filePath, detailPath);
    QVERIFY(QFileInfo::exists(store.indexPath()));

    QVector<HistoryEntry> indexed;
    QVERIFY(store.readIndex(&indexed));
    QCOMPARE(indexed.size(), 1);
    QCOMPARE(indexed.first().filePath, detailPath);
}

void HistoryStoreTests::rebuildsWhenIndexOnlyHasStaleFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    store.ensureModeDateStructure(tr8("听写"), QStringLiteral("2026-06-24"));
    QString detailPath;
    QVERIFY(writeDetailFile(
        store,
        QStringLiteral("2026-06-24"),
        QStringLiteral("actual.json"),
        makeHistoryJson(QStringLiteral("2026-06-24T11:00:00"), tr8("新的输入"), tr8("新的输出")),
        &detailPath
    ));

    HistoryEntry stale;
    stale.modeId = QStringLiteral("dictate");
    stale.mode = tr8("听写");
    stale.time = QStringLiteral("2026-06-24T09:00:00");
    stale.filePath = QDir(temp.path()).filePath(QStringLiteral("missing.json"));

    QJsonObject index;
    QJsonArray items;
    items.append(HistoryStore::entryToIndexObject(stale));
    index.insert(QStringLiteral("entries"), items);
    QVERIFY(writeBytesAtomically(
        store.indexPath(),
        QJsonDocument(index).toJson(QJsonDocument::Indented)
    ));

    const QVector<HistoryEntry> entries = store.loadEntries();

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().filePath, detailPath);
    QCOMPARE(entries.first().input, tr8("新的输入"));
}

void HistoryStoreTests::appendIndexEntryReplacesExistingFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    store.ensureModeDateStructure(tr8("听写"), QStringLiteral("2026-06-24"));
    QString detailPath;
    QVERIFY(writeDetailFile(
        store,
        QStringLiteral("2026-06-24"),
        QStringLiteral("replace.json"),
        makeHistoryJson(QStringLiteral("2026-06-24T12:00:00"), tr8("输入"), tr8("旧输出")),
        &detailPath
    ));

    HistoryEntry first = HistoryStore::entryFromJsonObject(
        makeHistoryJson(QStringLiteral("2026-06-24T12:00:00"), tr8("输入"), tr8("旧输出")),
        detailPath
    );
    HistoryEntry second = first;
    second.output = tr8("新输出");
    second.favorite = true;

    store.appendIndexEntry(first);
    store.appendIndexEntry(second);

    QVector<HistoryEntry> indexed;
    QVERIFY(store.readIndex(&indexed));
    QCOMPARE(indexed.size(), 1);
    QCOMPARE(indexed.first().output, tr8("新输出"));
    QVERIFY(indexed.first().favorite);
}

void HistoryStoreTests::updateRetriedSegmentSynchronizesMirrorsAndIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    QJsonObject item = makeHistoryJson(
        QStringLiteral("2026-06-24T12:00:00"),
        tr8("语音输入：\nfirst\n[第 2 段识别失败]"),
        QStringLiteral("output"),
        tr8("所有录音分段识别失败")
    );
    item.insert(QStringLiteral("longRecording"), true);
    item.insert(QStringLiteral("speechElapsedMs"), 10.0);
    item.insert(QStringLiteral("segmentCount"), 2);
    item.insert(QStringLiteral("failedSegmentCount"), 1);

    QJsonArray failedSegments;
    failedSegments.append(2);
    item.insert(QStringLiteral("failedSegments"), failedSegments);

    QJsonArray segments;
    QJsonObject firstSegment;
    firstSegment.insert(QStringLiteral("index"), 1);
    firstSegment.insert(QStringLiteral("audio"), QStringLiteral("one.wav"));
    firstSegment.insert(QStringLiteral("text"), QStringLiteral("first"));
    firstSegment.insert(QStringLiteral("error"), QString());
    firstSegment.insert(QStringLiteral("recognitionElapsedMs"), 10.0);
    firstSegment.insert(QStringLiteral("attempts"), 1);
    segments.append(firstSegment);

    QJsonObject secondSegment;
    secondSegment.insert(QStringLiteral("index"), 2);
    secondSegment.insert(QStringLiteral("audio"), QStringLiteral("two.wav"));
    secondSegment.insert(QStringLiteral("text"), QString());
    secondSegment.insert(QStringLiteral("error"), QStringLiteral("failed"));
    secondSegment.insert(QStringLiteral("recognitionElapsedMs"), -1.0);
    secondSegment.insert(QStringLiteral("attempts"), 1);
    segments.append(secondSegment);
    item.insert(QStringLiteral("segments"), segments);

    HistoryAppendRequest request;
    request.modeId = QStringLiteral("dictate");
    request.modeTitle = tr8("听写");
    request.item = item;
    const HistoryAppendResult appendResult = store.appendRecord(request);
    QVERIFY(appendResult.ok);

    HistoryEntry entry = store.loadEntries().first();
    HistorySegmentRetryResult retry;
    retry.index = 2;
    retry.text = QStringLiteral("second retry");
    retry.elapsedMs = 25;

    QVERIFY(store.updateRetriedSegment(&entry, retry));

    QCOMPARE(entry.speechElapsedMs, 35LL);
    QVERIFY(entry.error.isEmpty());
    QVERIFY(entry.input.contains(QStringLiteral("first")));
    QVERIFY(entry.input.contains(QStringLiteral("second retry")));

    QJsonObject mainDetail;
    QVERIFY(readJsonObjectFile(entry.filePath, &mainDetail));
    QJsonObject allDetail;
    QVERIFY(readJsonObjectFile(entry.allDetailFile, &allDetail));

    for (const QJsonObject detail : QVector<QJsonObject>() << mainDetail << allDetail) {
        QCOMPARE(detail.value(QStringLiteral("failedSegmentCount")).toInt(), 0);
        QCOMPARE(detail.value(QStringLiteral("failedSegments")).toArray().size(), 0);
        QCOMPARE(
            static_cast<qint64>(detail.value(QStringLiteral("speechElapsedMs")).toDouble()),
            35LL
        );
        const QJsonArray savedSegments =
            detail.value(QStringLiteral("segments")).toArray();
        QCOMPARE(savedSegments.size(), 2);
        const QJsonObject retried =
            savedSegments.at(1).toObject();
        QCOMPARE(retried.value(QStringLiteral("text")).toString(), QStringLiteral("second retry"));
        QCOMPARE(retried.value(QStringLiteral("error")).toString(), QString());
        QCOMPARE(retried.value(QStringLiteral("attempts")).toInt(), 2);
    }

    QVERIFY(readTextFile(entry.textFile).contains(QStringLiteral("second retry")));
    QVERIFY(readTextFile(entry.allTextFile).contains(QStringLiteral("second retry")));

    QVector<HistoryEntry> indexed;
    QVERIFY(store.readIndex(&indexed));
    QCOMPARE(indexed.size(), 1);
    QVERIFY(indexed.first().input.contains(QStringLiteral("second retry")));
    QVERIFY(indexed.first().error.isEmpty());
}

void HistoryStoreTests::relatedFilesAreDeduplicated()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString audioPath = QDir(temp.path()).filePath(QStringLiteral("same.wav"));
    const QString detailPath = QDir(temp.path()).filePath(QStringLiteral("detail.json"));

    HistoryEntry entry;
    entry.audio = audioPath;
    entry.allAudioFile = audioPath;
    entry.textFile = QStringLiteral("  ");
    entry.filePath = detailPath;
    entry.allDetailFile = detailPath;

    const QStringList files = HistoryStore::relatedFilesForEntry(entry);

    QCOMPARE(files.size(), 2);
    QVERIFY(files.contains(audioPath));
    QVERIFY(files.contains(detailPath));
}

void HistoryStoreTests::removeEntryFilesDeletesExistingFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString audioPath = QDir(temp.path()).filePath(QStringLiteral("audio.wav"));
    const QString textPath = QDir(temp.path()).filePath(QStringLiteral("record.txt"));
    const QString detailPath = QDir(temp.path()).filePath(QStringLiteral("detail.json"));
    QVERIFY(writeBytesAtomically(audioPath, QByteArray("audio")));
    QVERIFY(writeBytesAtomically(textPath, QByteArray("text")));
    QVERIFY(writeBytesAtomically(detailPath, QByteArray("{}")));

    HistoryEntry entry;
    entry.audio = audioPath;
    entry.textFile = textPath;
    entry.filePath = detailPath;
    entry.allDetailFile = QDir(temp.path()).filePath(QStringLiteral("missing.json"));

    QVERIFY(HistoryStore::removeEntryFiles(entry));
    QVERIFY(!QFileInfo::exists(audioPath));
    QVERIFY(!QFileInfo::exists(textPath));
    QVERIFY(!QFileInfo::exists(detailPath));
}

void HistoryStoreTests::voiceHistoryBuilderNormalizesControllerState()
{
    VoiceHistoryBuildRequest request;
    request.recordDirectory = QStringLiteral("records");
    request.modeId = QStringLiteral("dictate");
    request.modeTitle = tr8("鍚啓");
    request.input = QStringLiteral("raw");
    request.output = QStringLiteral("polished");
    request.model = QStringLiteral("deepseek-v4-flash");
    request.usedModel = false;
    request.promptVersion = QStringLiteral("dictate-v1");
    request.elapsedMs = 321;
    request.runContext.modeId = QStringLiteral("translate");
    request.runContext.screenshotInput = true;
    request.actionHadRecording = true;
    request.recordingTriggerMode = QStringLiteral("hotkey");

    const HistoryRecordSaveRequest saveRequest =
        VoiceHistoryRequestBuilder::build(request);

    QCOMPARE(saveRequest.recordDirectory, QStringLiteral("records"));
    QCOMPARE(saveRequest.modeId, QStringLiteral("dictate"));
    QCOMPARE(saveRequest.metadata.input, QStringLiteral("raw"));
    QCOMPARE(saveRequest.metadata.output, QStringLiteral("polished"));
    QVERIFY(saveRequest.metadata.model.isEmpty());
    QVERIFY(saveRequest.metadata.promptVersion.isEmpty());
    QCOMPARE(saveRequest.metadata.elapsedMs, 321LL);
    QVERIFY(saveRequest.metadata.actionHadRecording);
    QCOMPARE(
        saveRequest.metadata.recordingTriggerMode,
        QStringLiteral("hotkey")
    );
    QVERIFY(!saveRequest.metadata.runContext.screenshotInput);
}

void HistoryStoreTests::voiceHistoryRecorderSavesAndReportsLogFields()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VoiceHistorySaveRequest request;
    request.recordDirectory = temp.path();
    request.modeId = QStringLiteral("ask");
    request.modeTitle = tr8("问答");
    request.input = QStringLiteral("source");
    request.output = QStringLiteral("answer");
    request.error = QString();
    request.model = QStringLiteral("deepseek-v4-pro");
    request.usedModel = true;
    request.elapsedMs = 456;
    request.speechElapsedMs = 120;
    request.modelElapsedMs = 300;
    request.promptVersion = QStringLiteral("ask-v1");
    request.runContext.modeId = QStringLiteral("ask");
    request.runContext.selectedText = QStringLiteral("source");
    request.actionHadRecording = true;
    request.recordingTriggerMode = QStringLiteral("hotkey");

    const VoiceHistorySaveResult result = VoiceHistoryRecorder::save(request);

    QVERIFY(result.saved.ok);
    QCOMPARE(result.logAction, tr8("保存"));
    QVERIFY(result.logDetail.contains(QStringLiteral("ask")));
    QVERIFY(QFileInfo::exists(result.saved.modeDetailPath));
    QCOMPARE(result.saved.item.value(QStringLiteral("modeId")).toString(), QStringLiteral("ask"));
    QCOMPARE(result.saved.item.value(QStringLiteral("model")).toString(), QStringLiteral("deepseek-v4-pro"));
    QCOMPARE(result.saved.item.value(QStringLiteral("promptVersion")).toString(), QStringLiteral("ask-v1"));
    QCOMPARE(static_cast<int>(result.saved.item.value(QStringLiteral("elapsedMs")).toDouble()), 456);
}

void HistoryStoreTests::recordServiceBuildsMetadataAndAppendsRecord()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    HistoryRecordSaveRequest request;
    request.recordDirectory = temp.path();
    request.modeId = QStringLiteral("translate");
    request.modeTitle = tr8("翻译");
    request.metadata.input = QStringLiteral("hello");
    request.metadata.output = tr8("你好");
    request.metadata.model = QStringLiteral("deepseek-v4-flash");
    request.metadata.elapsedMs = 88;
    request.metadata.runContext.modeId = QStringLiteral("translate");
    request.metadata.runContext.selectedText = QStringLiteral("hello");

    const HistoryAppendResult result = HistoryRecordService::save(request);

    QVERIFY(result.ok);
    QVERIFY(QFileInfo::exists(result.modeDetailPath));
    QVERIFY(QFileInfo::exists(result.modeTextPath));
    QCOMPARE(result.item.value(QStringLiteral("modeId")).toString(), QStringLiteral("translate"));
    QCOMPARE(result.item.value(QStringLiteral("mode")).toString(), tr8("翻译"));
    QCOMPARE(result.item.value(QStringLiteral("input")).toString(), QStringLiteral("hello"));
    QCOMPARE(result.item.value(QStringLiteral("output")).toString(), tr8("你好"));
    QCOMPARE(result.item.value(QStringLiteral("inputSource")).toString(), QStringLiteral("text"));
    QCOMPARE(static_cast<int>(result.item.value(QStringLiteral("elapsedMs")).toDouble()), 88);

    const QVector<HistoryEntry> entries = HistoryStore(temp.path()).loadEntries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().modeId, QStringLiteral("translate"));
    QCOMPARE(entries.first().output, tr8("你好"));
}

void HistoryStoreTests::recordServiceSavesOcrRecord()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    OcrPageHistoryMetadataRequest metadata;
    metadata.result.ok = true;
    metadata.result.engine = OcrEngine::RapidOcr;
    metadata.result.text = QStringLiteral("OCR result");
    metadata.result.elapsedMs = 37;
    metadata.imagePath = QDir(temp.path()).filePath(QStringLiteral("capture.png"));
    metadata.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");

    const HistoryAppendResult result = HistoryRecordService(temp.path()).saveOcr(
        QStringLiteral("OCR"),
        metadata
    );

    QVERIFY(result.ok);
    QCOMPARE(result.item.value(QStringLiteral("modeId")).toString(), QStringLiteral("ocr"));
    QCOMPARE(result.item.value(QStringLiteral("input")).toString(), QStringLiteral("OCR result"));
    QCOMPARE(result.item.value(QStringLiteral("ocrEngine")).toString(), QStringLiteral("RapidOCR"));
    QCOMPARE(result.item.value(QStringLiteral("ocrLanguages")).toArray().size(), 2);
}

void HistoryStoreTests::recordServiceUpdatesFavoriteAndIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    HistoryRecordSaveRequest request;
    request.recordDirectory = temp.path();
    request.modeId = QStringLiteral("dictate");
    request.modeTitle = QStringLiteral("Dictate");
    request.metadata.input = QStringLiteral("source");
    request.metadata.output = QStringLiteral("result");
    const HistoryAppendResult saved = HistoryRecordService::save(request);
    QVERIFY(saved.ok);

    const HistoryFavoriteUpdateResult updated =
        HistoryRecordService(temp.path()).updateFavorite(
            saved.modeDetailPath,
            true,
            QStringLiteral("important")
        );

    QVERIFY(updated.ok);
    QJsonObject mainDetail;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &mainDetail));
    QJsonObject mirrorDetail;
    QVERIFY(readJsonObjectFile(saved.allDetailPath, &mirrorDetail));
    QVERIFY(mainDetail.value(QStringLiteral("favorite")).toBool());
    QCOMPARE(
        mainDetail.value(QStringLiteral("favoriteFolder")).toString(),
        QStringLiteral("important")
    );
    QCOMPARE(mainDetail, mirrorDetail);

    const QVector<HistoryEntry> entries = HistoryStore(temp.path()).loadEntries();
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.first().favorite);
    QCOMPARE(entries.first().favoriteFolder, QStringLiteral("important"));
}

void HistoryStoreTests::recordServiceRemovesEntriesAndRebuildsIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    HistoryRecordSaveRequest request;
    request.recordDirectory = temp.path();
    request.modeId = QStringLiteral("dictate");
    request.modeTitle = QStringLiteral("Dictate");
    request.metadata.input = QStringLiteral("source");
    request.metadata.output = QStringLiteral("result");
    const HistoryAppendResult saved = HistoryRecordService::save(request);
    QVERIFY(saved.ok);

    const HistoryDeleteResult removed =
        HistoryRecordService(temp.path()).removeEntries(
            QStringList() << saved.modeDetailPath << saved.modeDetailPath
        );

    QVERIFY(removed.ok);
    QCOMPARE(removed.requestedCount, 1);
    QCOMPARE(removed.removedCount, 1);
    QVERIFY(removed.failedPaths.isEmpty());
    QVERIFY(!QFileInfo::exists(saved.modeTextPath));
    QVERIFY(!QFileInfo::exists(saved.modeDetailPath));
    QVERIFY(!QFileInfo::exists(saved.allTextPath));
    QVERIFY(!QFileInfo::exists(saved.allDetailPath));
    QCOMPARE(HistoryStore(temp.path()).loadEntries().size(), 0);
}

void HistoryStoreTests::recordServiceRejectsEntryOutsideRoot()
{
    QTemporaryDir records;
    QTemporaryDir outside;
    QVERIFY(records.isValid());
    QVERIFY(outside.isValid());

    const QString detailPath =
        QDir(outside.path()).filePath(QStringLiteral("outside.json"));
    QVERIFY(writeBytesAtomically(
        detailPath,
        QJsonDocument(makeHistoryJson(
            QStringLiteral("2026-07-24T10:00:00"),
            QStringLiteral("outside"),
            QStringLiteral("keep")
        )).toJson(QJsonDocument::Indented)
    ));

    const HistoryDeleteResult removed =
        HistoryRecordService(records.path()).removeEntries(
            QStringList() << detailPath
        );

    QVERIFY(!removed.ok);
    QCOMPARE(removed.requestedCount, 1);
    QCOMPARE(removed.removedCount, 0);
    QCOMPARE(removed.failedPaths, QStringList() << detailPath);
    QVERIFY(QFileInfo::exists(detailPath));
}

void HistoryStoreTests::recordServiceUpdatesFlowEditedTextEverywhere()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("run-42"));
    QVERIFY(saved.ok);

    OperationError error;
    QVERIFY(HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("run-42")),
        saved.modeDetailPath,
        QStringLiteral("edited result"),
        &error
    ));
    QVERIFY(error.isEmpty());

    QJsonObject modeDetail;
    QJsonObject allDetail;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &modeDetail));
    QVERIFY(readJsonObjectFile(saved.allDetailPath, &allDetail));
    QCOMPARE(
        modeDetail.value(QStringLiteral("input")).toString(),
        QStringLiteral("canonical input")
    );
    QCOMPARE(
        modeDetail.value(QStringLiteral("output")).toString(),
        QStringLiteral("edited result")
    );
    QCOMPARE(modeDetail, allDetail);
    QVERIFY(readTextFile(saved.modeTextPath).contains(QStringLiteral("edited result")));
    QVERIFY(readTextFile(saved.allTextPath).contains(QStringLiteral("edited result")));
    QVERIFY(!readTextFile(saved.modeTextPath).contains(QStringLiteral("original output")));
    QVERIFY(!readTextFile(saved.allTextPath).contains(QStringLiteral("original output")));

    QVector<HistoryEntry> indexed;
    QVERIFY(HistoryStore(records.path()).readIndex(&indexed));
    QCOMPARE(indexed.size(), 1);
    QCOMPARE(indexed.first().filePath, saved.modeDetailPath);
    QCOMPARE(indexed.first().output, QStringLiteral("edited result"));
}

void HistoryStoreTests::recordServiceAllowsLegacyEmptyFlowReferences()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("legacy-run"));
    QVERIFY(saved.ok);

    QJsonObject detail;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &detail));
    detail.insert(QStringLiteral("allDetailFile"), QString());
    detail.insert(QStringLiteral("textFile"), QString());
    detail.insert(QStringLiteral("allTextFile"), QString());
    QVERIFY(writeBytesAtomically(
        saved.modeDetailPath,
        QJsonDocument(detail).toJson(QJsonDocument::Indented)
    ));

    OperationError error;
    QVERIFY(HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("legacy-run")),
        saved.modeDetailPath,
        QStringLiteral("legacy edit"),
        &error
    ));

    QJsonObject updated;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &updated));
    QCOMPARE(
        updated.value(QStringLiteral("output")).toString(),
        QStringLiteral("legacy edit")
    );

    QVector<HistoryEntry> indexed;
    QVERIFY(HistoryStore(records.path()).readIndex(&indexed));
    QCOMPARE(indexed.size(), 1);
    QCOMPARE(indexed.first().output, QStringLiteral("legacy edit"));
}

void HistoryStoreTests::recordServiceRejectsFlowEditOutsideRoot()
{
    QTemporaryDir records;
    QTemporaryDir outside;
    QVERIFY(records.isValid());
    QVERIFY(outside.isValid());

    QJsonObject detail;
    detail.insert(QStringLiteral("flowRunId"), QStringLiteral("outside-run"));
    detail.insert(QStringLiteral("output"), QStringLiteral("sentinel"));
    const QString outsidePath =
        QDir(outside.path()).filePath(QStringLiteral("outside.json"));
    QVERIFY(writeBytesAtomically(
        outsidePath,
        QJsonDocument(detail).toJson(QJsonDocument::Indented)
    ));
    const QByteArray before = readFileBytes(outsidePath);

    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("outside-run")),
        outsidePath,
        QStringLiteral("must not write"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QCOMPARE(readFileBytes(outsidePath), before);
    QVERIFY(!QFileInfo::exists(HistoryStore(records.path()).indexPath()));
}

void HistoryStoreTests::recordServiceRejectsMissingFlowDetail()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const QString missingPath =
        QDir(records.path()).filePath(QStringLiteral("missing.json"));
    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("missing-run")),
        missingPath,
        QStringLiteral("must not create"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QVERIFY(!QFileInfo::exists(missingPath));
    QVERIFY(!QFileInfo::exists(HistoryStore(records.path()).indexPath()));
}

void HistoryStoreTests::recordServiceRejectsFlowEditForWrongRun()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("actual-run"));
    QVERIFY(saved.ok);
    const QByteArray modeBefore = readFileBytes(saved.modeDetailPath);
    const QByteArray mirrorBefore = readFileBytes(saved.allDetailPath);
    const QByteArray textBefore = readFileBytes(saved.modeTextPath);

    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("different-run")),
        saved.modeDetailPath,
        QStringLiteral("must not write"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QCOMPARE(readFileBytes(saved.modeDetailPath), modeBefore);
    QCOMPARE(readFileBytes(saved.allDetailPath), mirrorBefore);
    QCOMPARE(readFileBytes(saved.modeTextPath), textBefore);
}

void HistoryStoreTests::recordServiceRejectsUnsafeFlowEditReference_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("fileName");

    QTest::newRow("all detail") << QStringLiteral("allDetailFile")
                                << QStringLiteral("sentinel.json");
    QTest::newRow("mode text") << QStringLiteral("textFile")
                              << QStringLiteral("sentinel.txt");
    QTest::newRow("all text") << QStringLiteral("allTextFile")
                             << QStringLiteral("sentinel.txt");
}

void HistoryStoreTests::recordServiceRejectsUnsafeFlowEditReference()
{
    QFETCH(QString, field);
    QFETCH(QString, fileName);

    QTemporaryDir records;
    QTemporaryDir outside;
    QVERIFY(records.isValid());
    QVERIFY(outside.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("run-ref"));
    QVERIFY(saved.ok);

    const QString outsidePath = QDir(outside.path()).filePath(fileName);
    QVERIFY(writeBytesAtomically(outsidePath, QByteArray("outside sentinel")));
    const QByteArray outsideBefore = readFileBytes(outsidePath);

    QJsonObject detail;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &detail));
    detail.insert(field, outsidePath);
    QVERIFY(writeBytesAtomically(
        saved.modeDetailPath,
        QJsonDocument(detail).toJson(QJsonDocument::Indented)
    ));
    const QByteArray modeBefore = readFileBytes(saved.modeDetailPath);
    const QByteArray mirrorBefore = readFileBytes(saved.allDetailPath);

    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("run-ref")),
        saved.modeDetailPath,
        QStringLiteral("must not write"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QCOMPARE(readFileBytes(outsidePath), outsideBefore);
    QCOMPARE(readFileBytes(saved.modeDetailPath), modeBefore);
    QCOMPARE(readFileBytes(saved.allDetailPath), mirrorBefore);
}

void HistoryStoreTests::recordServiceRejectsMissingFlowEditReference()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("run-missing-ref"));
    QVERIFY(saved.ok);

    QJsonObject detail;
    QVERIFY(readJsonObjectFile(saved.modeDetailPath, &detail));
    detail.insert(
        QStringLiteral("textFile"),
        QDir(records.path()).filePath(QStringLiteral("missing.txt"))
    );
    QVERIFY(writeBytesAtomically(
        saved.modeDetailPath,
        QJsonDocument(detail).toJson(QJsonDocument::Indented)
    ));
    const QByteArray modeBefore = readFileBytes(saved.modeDetailPath);

    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("run-missing-ref")),
        saved.modeDetailPath,
        QStringLiteral("must not write"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QCOMPARE(readFileBytes(saved.modeDetailPath), modeBefore);
}

void HistoryStoreTests::recordServiceRejectsMismatchedFlowMirror()
{
    QTemporaryDir records;
    QVERIFY(records.isValid());

    const HistoryAppendResult saved =
        appendFlowRecord(records.path(), QStringLiteral("run-main"));
    QVERIFY(saved.ok);

    QJsonObject mirror;
    QVERIFY(readJsonObjectFile(saved.allDetailPath, &mirror));
    mirror.insert(QStringLiteral("flowRunId"), QStringLiteral("another-run"));
    QVERIFY(writeBytesAtomically(
        saved.allDetailPath,
        QJsonDocument(mirror).toJson(QJsonDocument::Indented)
    ));
    const QByteArray mainBefore = readFileBytes(saved.modeDetailPath);
    const QByteArray mirrorBefore = readFileBytes(saved.allDetailPath);

    OperationError error;
    QVERIFY(!HistoryRecordService(records.path()).updateFlowEditedText(
        executionId(QStringLiteral("run-main")),
        saved.modeDetailPath,
        QStringLiteral("must not write"),
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_history_save_failed"));
    QCOMPARE(readFileBytes(saved.modeDetailPath), mainBefore);
    QCOMPARE(readFileBytes(saved.allDetailPath), mirrorBefore);
}

void HistoryStoreTests::flowMetadataPreservesFullHashAndSafeTrace()
{
    HistoryRecordMetadataRequest request;
    request.input = QStringLiteral("canonical input");
    request.output = QStringLiteral("final output");
    request.flowRunId = QStringLiteral("run-64");
    request.flowPublishedRevision = 3;
    request.flowPublishedHash = QString(64, QLatin1Char('a'));
    request.flowTrigger = QStringLiteral("mainHotkey");
    HistoryFlowNodeTrace trace;
    trace.nodeId = QStringLiteral("model");
    trace.nodeType = QStringLiteral("model");
    trace.state = QStringLiteral("succeeded");
    trace.elapsedMs = 18;
    trace.modelId = QStringLiteral("model-a");
    trace.promptVersion = QStringLiteral("prompt-v1");
    request.flowNodeTraces << trace;

    const QJsonObject item =
        HistoryRecordBuilder::buildMetadata(request);
    QCOMPARE(
        item.value(QStringLiteral("flowPublishedRevision"))
            .toInt(),
        3
    );
    QCOMPARE(
        item.value(QStringLiteral("flowPublishedHash"))
            .toString(),
        QString(64, QLatin1Char('a'))
    );
    QCOMPARE(
        item.value(QStringLiteral("flowNodeTraces"))
            .toArray().size(),
        1
    );
    const QByteArray serialized =
        QJsonDocument(item).toJson(QJsonDocument::Compact);
    QVERIFY(!serialized.contains("data:image"));
    QVERIFY(!serialized.contains("screenshot"));
}

QTEST_MAIN(HistoryStoreTests)

#include "history_store_tests.moc"
