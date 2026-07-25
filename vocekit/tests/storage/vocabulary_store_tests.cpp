#include "../../src/storage/vocabulary_store.h"

#include <QtTest>

#include <QDir>
#include <QTemporaryDir>

namespace {

VocabularyEntry entry(
    const QString &id,
    const QString &source,
    const QString &target,
    const QString &scope = QStringLiteral("__global"),
    const QString &aliases = QString()
)
{
    VocabularyEntry item;
    item.id = id;
    item.source = source;
    item.target = target;
    item.scopeId = scope;
    item.matchMode = QStringLiteral("caseInsensitive");
    item.aliases = aliases;
    item.enabled = true;
    return item;
}

} // namespace

class VocabularyStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void savesAndLoadsValidEntriesOnly();
    void appendsEntryWithValidation();
    void appliesScopedCorrections();
    void buildsLimitedRelevantPromptBlock();
    void parsesAndEscapesCsvLines();
};

void VocabularyStoreTests::savesAndLoadsValidEntriesOnly()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyStore store(QDir(temp.path()).filePath(QStringLiteral("entries.json")));

    QVector<VocabularyEntry> entries;
    entries.append(entry(QStringLiteral("vocab_1"), QStringLiteral("deepseep"), QStringLiteral("DeepSeek")));
    entries.append(entry(QString(), QStringLiteral("bad"), QStringLiteral("Bad")));
    QVERIFY(store.saveEntries(entries));

    const QVector<VocabularyEntry> loaded = store.loadEntries();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, QStringLiteral("vocab_1"));
    QCOMPARE(loaded.first().source, QStringLiteral("deepseep"));
    QCOMPARE(loaded.first().target, QStringLiteral("DeepSeek"));
}

void VocabularyStoreTests::appendsEntryWithValidation()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyStore store(QDir(temp.path()).filePath(QStringLiteral("entries.json")));

    VocabularyEntry item = entry(
        QString(),
        QStringLiteral("deepseep"),
        QStringLiteral("DeepSeek")
    );
    QString error;
    QVERIFY(store.appendEntry(&item, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(item.id, QStringLiteral("vocab_1"));

    const QVector<VocabularyEntry> loaded = store.loadEntries();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, QStringLiteral("vocab_1"));

    VocabularyEntry invalid = entry(
        QString(),
        QStringLiteral("DeepSeek"),
        QStringLiteral("DeepSeek")
    );
    QVERIFY(!store.appendEntry(&invalid, &error));
    QVERIFY(error.contains(QStringLiteral("无修正效果")));
    QCOMPARE(store.loadEntries().size(), 1);
}

void VocabularyStoreTests::appliesScopedCorrections()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyStore store(QDir(temp.path()).filePath(QStringLiteral("entries.json")));
    QVector<VocabularyEntry> entries;
    entries.append(entry(
        QStringLiteral("vocab_1"),
        QStringLiteral("deepseep"),
        QStringLiteral("DeepSeek"),
        QStringLiteral("__global")
    ));
    entries.append(entry(
        QStringLiteral("vocab_2"),
        QStringLiteral("qt"),
        QStringLiteral("Qt"),
        QStringLiteral("translate")
    ));
    QVERIFY(store.saveEntries(entries));

    QCOMPARE(
        store.applyEntries(QStringLiteral("deepseep 和 qt"), QStringLiteral("dictate"), true),
        QStringLiteral("DeepSeek 和 qt")
    );
    QCOMPARE(
        store.applyEntries(QStringLiteral("deepseep 和 qt"), QStringLiteral("translate"), true),
        QStringLiteral("DeepSeek 和 Qt")
    );
}

void VocabularyStoreTests::buildsLimitedRelevantPromptBlock()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyStore store(QDir(temp.path()).filePath(QStringLiteral("entries.json")));
    QVector<VocabularyEntry> entries;
    entries.append(entry(
        QStringLiteral("vocab_1"),
        QStringLiteral("deepseep"),
        QStringLiteral("DeepSeek"),
        QStringLiteral("__global"),
        QStringLiteral("deep seek")
    ));
    entries.append(entry(
        QStringLiteral("vocab_2"),
        QStringLiteral("open ai"),
        QStringLiteral("OpenAI"),
        QStringLiteral("__global")
    ));
    entries.append(entry(
        QStringLiteral("vocab_3"),
        QStringLiteral("unrelated"),
        QStringLiteral("Unrelated"),
        QStringLiteral("__global")
    ));
    QVERIFY(store.saveEntries(entries));

    const QString block = store.promptBlock(
        QStringLiteral("dictate"),
        true,
        QStringLiteral("请把 deepseep 和 open ai 这两个词写对。"),
        1
    );

    QVERIFY(block.contains(QStringLiteral("deepseep")));
    QVERIFY(block.contains(QStringLiteral("DeepSeek")));
    QVERIFY(!block.contains(QStringLiteral("open ai")));
    QVERIFY(!block.contains(QStringLiteral("unrelated")));
}

void VocabularyStoreTests::parsesAndEscapesCsvLines()
{
    const QString escaped = VocabularyStore::csvEscape(QStringLiteral("a,b\"c"));
    QCOMPARE(escaped, QStringLiteral("\"a,b\"\"c\""));

    const QStringList parsed = VocabularyStore::parseCsvLine(QStringLiteral("\"a,b\"\"c\", second"));
    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed.at(0), QStringLiteral("a,b\"c"));
    QCOMPARE(parsed.at(1), QStringLiteral("second"));
}

QTEST_MAIN(VocabularyStoreTests)

#include "vocabulary_store_tests.moc"
