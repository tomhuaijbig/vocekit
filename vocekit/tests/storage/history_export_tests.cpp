#include "../../src/storage/history_export.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

HistoryEntry sampleEntry(const QString &mode, const QString &time)
{
    HistoryEntry entry;
    entry.modeId = QStringLiteral("dictate");
    entry.mode = mode;
    entry.time = time;
    entry.input = QStringLiteral("source text");
    entry.output = QStringLiteral("final text");
    entry.model = QStringLiteral("deepseek-v4-flash");
    return entry;
}

bool writeSmallFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write("audio");
    return true;
}

} // namespace

class HistoryExportTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTextExportContent();
    void buildsDetailsExportObject();
    void writesExportFiles();
    void exportsAudioFilesAndFallsBackToAllAudioFile();
};

void HistoryExportTests::buildsTextExportContent()
{
    QVector<HistoryEntry> entries;
    entries << sampleEntry(QString::fromUtf8("听写"), QStringLiteral("2026-07-03T12:34:56"));
    entries << sampleEntry(QString::fromUtf8("翻译"), QStringLiteral("2026-07-03T12:35:56"));

    const QString content = buildHistoryTextExportContent(
        entries,
        [](const HistoryEntry &entry) {
            return QStringLiteral("detail:") + entry.mode;
        }
    );

    QVERIFY(content.contains(QString::fromUtf8("第 1 条")));
    QVERIFY(content.contains(QStringLiteral("detail:") + QString::fromUtf8("听写")));
    QVERIFY(content.contains(QString::fromUtf8("==============================")));
    QVERIFY(content.contains(QString::fromUtf8("第 2 条")));
}

void HistoryExportTests::buildsDetailsExportObject()
{
    QVector<HistoryEntry> entries;
    entries << sampleEntry(QString::fromUtf8("问答"), QStringLiteral("2026-07-03T12:34:56"));

    HistoryExportOptions options;
    options.filterMode = QStringLiteral("ask");
    options.searchText = QStringLiteral("keyword");
    options.selectionOnly = true;
    options.exportedAt = QDateTime(QDate(2026, 7, 3), QTime(12, 0, 0));

    const QJsonObject root = buildHistoryDetailsExportObject(
        entries,
        options,
        [](const HistoryEntry &entry) {
            QJsonObject object;
            object.insert(QStringLiteral("mode"), entry.mode);
            object.insert(QStringLiteral("model"), entry.model);
            return object;
        }
    );

    QCOMPARE(root.value(QStringLiteral("exportedAt")).toString(), QStringLiteral("2026-07-03T12:00:00"));
    QCOMPARE(root.value(QStringLiteral("recordCount")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("filterMode")).toString(), QStringLiteral("ask"));
    QCOMPARE(root.value(QStringLiteral("searchText")).toString(), QStringLiteral("keyword"));
    QVERIFY(root.value(QStringLiteral("selectionOnly")).toBool());
    QCOMPARE(root.value(QStringLiteral("records")).toArray().size(), 1);
    QCOMPARE(root.value(QStringLiteral("records")).toArray().at(0).toObject().value(QStringLiteral("model")).toString(), QStringLiteral("deepseek-v4-flash"));
}

void HistoryExportTests::writesExportFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVector<HistoryEntry> entries;
    entries << sampleEntry(QString::fromUtf8("听写"), QStringLiteral("2026-07-03T12:34:56"));

    const QString textPath = QDir(dir.path()).filePath(QStringLiteral("history.txt"));
    QVERIFY(writeHistoryTextExportFile(
        entries,
        textPath,
        [](const HistoryEntry &) {
            return QStringLiteral("plain detail");
        }
    ));
    QVERIFY(QFileInfo::exists(textPath));

    HistoryExportOptions options;
    options.filterMode = QStringLiteral("__all");
    const QString jsonPath = QDir(dir.path()).filePath(QStringLiteral("history.json"));
    QVERIFY(writeHistoryDetailsExportFile(
        entries,
        jsonPath,
        options,
        [](const HistoryEntry &entry) {
            QJsonObject object;
            object.insert(QStringLiteral("mode"), entry.mode);
            return object;
        }
    ));
    QVERIFY(QFileInfo::exists(jsonPath));
}

void HistoryExportTests::exportsAudioFilesAndFallsBackToAllAudioFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString sourceAudio = QDir(dir.path()).filePath(QStringLiteral("source.wav"));
    const QString fallbackAudio = QDir(dir.path()).filePath(QStringLiteral("fallback.pcm"));
    QVERIFY(writeSmallFile(sourceAudio));
    QVERIFY(writeSmallFile(fallbackAudio));

    HistoryEntry first = sampleEntry(QString::fromUtf8("听写"), QStringLiteral("2026-07-03T12:34:56"));
    first.audio = sourceAudio;

    HistoryEntry second = sampleEntry(QString::fromUtf8("问答"), QStringLiteral("2026-07-03T12:35:56"));
    second.audio = QDir(dir.path()).filePath(QStringLiteral("missing.wav"));
    second.allAudioFile = fallbackAudio;

    HistoryEntry missing = sampleEntry(QString::fromUtf8("翻译"), QStringLiteral("2026-07-03T12:36:56"));
    missing.audio = QDir(dir.path()).filePath(QStringLiteral("missing-too.wav"));

    QVector<HistoryEntry> entries;
    entries << first << second << missing;

    const QString targetPath = QDir(dir.path()).filePath(QStringLiteral("export"));
    const HistoryAudioExportResult result = exportHistoryAudioFilesToDirectory(entries, targetPath);
    QVERIFY(result.ok);
    QCOMPARE(result.exported, 2);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(QDir(targetPath).entryList(QDir::Files | QDir::NoDotAndDotDot).size(), 2);
}

QTEST_MAIN(HistoryExportTests)

#include "history_export_tests.moc"
