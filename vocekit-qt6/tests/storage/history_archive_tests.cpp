#include "../../src/file_utils.h"
#include "../../src/storage/history_archive.h"
#include "../../src/storage/history_store.h"

#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool writeText(const QString &path, const QByteArray &data = QByteArray("data"))
{
    return writeBytesAtomically(path, data);
}

QJsonObject makeHistoryJson()
{
    QJsonObject item;
    item.insert(QStringLiteral("modeId"), QStringLiteral("dictate"));
    item.insert(QStringLiteral("mode"), tr8("听写"));
    item.insert(QStringLiteral("time"), QStringLiteral("2026-07-03T12:34:56"));
    item.insert(QStringLiteral("input"), QStringLiteral("hello"));
    item.insert(QStringLiteral("output"), tr8("你好"));
    item.insert(QStringLiteral("error"), QString());
    return item;
}

} // namespace

class HistoryArchiveTests : public QObject
{
    Q_OBJECT

private slots:
    void backupCopiesFilesButSkipsBackupFolder();
    void importRejectsRecordDirectoryParent();
    void importCopiesFilesAndRebuildsIndex();
    void importReportsEmptySource();
};

void HistoryArchiveTests::backupCopiesFilesButSkipsBackupFolder()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryStore store(temp.path());
    store.ensureRootStructure();
    const QString sourceFile = QDir(store.rootPath()).filePath(QStringLiteral("root.txt"));
    QVERIFY(writeText(sourceFile));

    const QString nestedBackupFile = QDir(store.backupDirectory()).filePath(QStringLiteral("old.txt"));
    QVERIFY(writeText(nestedBackupFile));

    const HistoryBackupResult result = backupHistoryRecordsToDirectory(
        store.rootPath(),
        QStringLiteral("20260703_120000")
    );

    QVERIFY(result.ok);
    QCOMPARE(result.fileCount, 1);
    QVERIFY(QFileInfo::exists(QDir(result.targetPath).filePath(QStringLiteral("root.txt"))));
    QVERIFY(!QFileInfo::exists(QDir(result.targetPath).filePath(HistoryStore::backupFolderName() + QStringLiteral("/old.txt"))));
}

void HistoryArchiveTests::importRejectsRecordDirectoryParent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString recordPath = QDir(temp.path()).filePath(QStringLiteral("records"));
    QDir().mkpath(recordPath);

    const HistoryImportResult result = importHistoryRecordsFromDirectory(temp.path(), recordPath);

    QVERIFY(!result.ok);
    QCOMPARE(result.status, HistoryImportStatus::UnsafeSource);
    QVERIFY(result.error.contains(tr8("不能从当前历史记录保存目录")));
}

void HistoryArchiveTests::importCopiesFilesAndRebuildsIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourcePath = QDir(temp.path()).filePath(QStringLiteral("source"));
    const HistoryStore sourceStore(sourcePath);
    const QString detailDir = sourceStore.modeDateSubDirectory(
        tr8("听写"),
        QStringLiteral("2026-07-03"),
        HistoryStore::detailSubFolderName()
    );
    QDir().mkpath(detailDir);
    QVERIFY(writeText(
        QDir(detailDir).filePath(QStringLiteral("entry.json")),
        QJsonDocument(makeHistoryJson()).toJson(QJsonDocument::Indented)
    ));

    const QString recordPath = QDir(temp.path()).filePath(QStringLiteral("target-records"));
    const HistoryImportResult result = importHistoryRecordsFromDirectory(sourcePath, recordPath);

    QVERIFY(result.ok);
    QCOMPARE(result.status, HistoryImportStatus::Success);
    QCOMPARE(result.fileCount, 1);
    QVERIFY(QFileInfo::exists(HistoryStore(recordPath).indexPath()));
    QCOMPARE(HistoryStore(recordPath).loadEntries().size(), 1);
}

void HistoryArchiveTests::importReportsEmptySource()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourcePath = QDir(temp.path()).filePath(QStringLiteral("empty-source"));
    const QString recordPath = QDir(temp.path()).filePath(QStringLiteral("target-records"));
    QDir().mkpath(sourcePath);

    const HistoryImportResult result = importHistoryRecordsFromDirectory(sourcePath, recordPath);

    QVERIFY(!result.ok);
    QCOMPARE(result.status, HistoryImportStatus::EmptySource);
    QCOMPARE(result.fileCount, 0);
}

QTEST_MAIN(HistoryArchiveTests)

#include "history_archive_tests.moc"
