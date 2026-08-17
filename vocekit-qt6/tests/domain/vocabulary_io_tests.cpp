#include "../../src/domain/vocabulary_io.h"

#include <QtTest>

namespace {

VocabularyScopeOption customScope()
{
    return {QStringLiteral("custom_1"), QStringLiteral("自定义功能 1")};
}

VocabularyEntry entry(
    const QString &source,
    const QString &target,
    const QString &scope = QStringLiteral("__global")
)
{
    VocabularyEntry item;
    item.id = QStringLiteral("vocab_test");
    item.source = source;
    item.target = target;
    item.scopeId = scope;
    item.matchMode = QStringLiteral("caseInsensitive");
    item.enabled = true;
    return item;
}

QVector<VocabularyScopeOption> scopes()
{
    QVector<VocabularyScopeOption> options = builtinVocabularyScopeOptions();
    options.append(customScope());
    return options;
}

} // namespace

class VocabularyIoTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizesBuiltInAndCustomScopes();
    void parsesCsvWithChineseHeaders();
    void parsesJsonRecordsRoot();
    void exportsReadableTextWithScopeTitle();
    void matchesSearchByScopeAndMatchTitle();
};

void VocabularyIoTests::normalizesBuiltInAndCustomScopes()
{
    QCOMPARE(
        normalizeVocabularyImportScope(QStringLiteral("全部"), scopes()),
        QStringLiteral("__global")
    );
    QCOMPARE(
        normalizeVocabularyImportScope(QStringLiteral("翻译"), scopes()),
        QStringLiteral("translate")
    );
    QCOMPARE(
        normalizeVocabularyImportScope(QStringLiteral("自定义功能 1"), scopes()),
        QStringLiteral("custom_1")
    );
}

void VocabularyIoTests::parsesCsvWithChineseHeaders()
{
    const QString csv = QStringLiteral(
        "原词/错词,标准写法,别名,作用范围,匹配方式,备注,启用\n"
        "deepseep,DeepSeek,deep seek,自定义功能 1,精确匹配,项目名,否\n"
    );

    const QVector<VocabularyEntry> entries = parseVocabularyCsvImport(csv, scopes());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().source, QStringLiteral("deepseep"));
    QCOMPARE(entries.first().target, QStringLiteral("DeepSeek"));
    QCOMPARE(entries.first().scopeId, QStringLiteral("custom_1"));
    QCOMPARE(entries.first().matchMode, QStringLiteral("exact"));
    QCOMPARE(entries.first().enabled, false);
}

void VocabularyIoTests::parsesJsonRecordsRoot()
{
    const QByteArray json = QByteArrayLiteral(
        "{ \"records\": ["
        "{\"id\":\"vocab_1\",\"source\":\"qt\",\"target\":\"Qt\",\"scopeId\":\"听写\",\"matchMode\":\"contains\"}"
        "] }"
    );

    const QVector<VocabularyEntry> entries = parseVocabularyJsonImport(json, scopes());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().scopeId, QStringLiteral("dictate"));
    QCOMPARE(entries.first().matchMode, QStringLiteral("contains"));
}

void VocabularyIoTests::exportsReadableTextWithScopeTitle()
{
    VocabularyEntry item = entry(
        QStringLiteral("deepseep"),
        QStringLiteral("DeepSeek"),
        QStringLiteral("custom_1")
    );
    item.aliases = QStringLiteral("deep seek");

    const QString text = vocabularyPlainExportText(QVector<VocabularyEntry>() << item, scopes());
    QVERIFY(text.contains(QStringLiteral("deepseep -> DeepSeek")));
    QVERIFY(text.contains(QStringLiteral("自定义功能 1")));
    QVERIFY(text.contains(QStringLiteral("忽略大小写")));
}

void VocabularyIoTests::matchesSearchByScopeAndMatchTitle()
{
    VocabularyEntry item = entry(
        QStringLiteral("deepseep"),
        QStringLiteral("DeepSeek"),
        QStringLiteral("custom_1")
    );
    item.matchMode = QStringLiteral("regex");

    QVERIFY(vocabularyEntryMatchesSearch(item, QStringLiteral("自定义功能"), scopes()));
    QVERIFY(vocabularyEntryMatchesSearch(item, QStringLiteral("正则"), scopes()));
    QVERIFY(!vocabularyEntryMatchesSearch(item, QStringLiteral("完全无关"), scopes()));
}

QTEST_MAIN(VocabularyIoTests)

#include "vocabulary_io_tests.moc"
