#include "../../src/tasks/vocabulary_diagnostic_task.h"
#include "../../src/storage/vocabulary_store.h"

#include <QtTest>

#include <QDir>
#include <QTemporaryDir>

namespace {

VocabularyEntry testEntry()
{
    VocabularyEntry item;
    item.id = QStringLiteral("vocab_1");
    item.source = QStringLiteral("deepseep");
    item.target = QStringLiteral("DeepSeek");
    item.scopeId = QStringLiteral("__global");
    item.matchMode = QStringLiteral("caseInsensitive");
    item.enabled = true;
    return item;
}

} // namespace

class VocabularyDiagnosticTaskTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsRequestFromSettings();
    void reportsMissingStoreWithoutCrashing();
    void reportsReplacementAndPromptBlock();
};

void VocabularyDiagnosticTaskTests::buildsRequestFromSettings()
{
    AppSettingsData settings;
    settings.vocabularyEnabled = true;
    settings.vocabularyAddMode = QStringLiteral("manual");
    settings.vocabularyOnlyForVoiceInput = true;
    settings.vocabularyPromptEntryLimit = 160;

    const VocabularyDiagnosticRequest request =
        buildVocabularyDiagnosticRequest(
            settings,
            QStringLiteral("C:/data/vocabulary.json")
        );

    QCOMPARE(request.storePath, QStringLiteral("C:/data/vocabulary.json"));
    QVERIFY(request.vocabularyEnabled);
    QCOMPARE(request.vocabularyAddMode, QStringLiteral("manual"));
    QVERIFY(request.vocabularyOnlyForVoiceInput);
    QCOMPARE(request.vocabularyPromptEntryLimit, 100);
}

void VocabularyDiagnosticTaskTests::reportsMissingStoreWithoutCrashing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyDiagnosticRequest request;
    request.storePath = QDir(temp.path()).filePath(QStringLiteral("missing.json"));
    request.vocabularyEnabled = true;
    request.vocabularyAddMode = QStringLiteral("manual");
    request.vocabularyPromptEntryLimit = 16;

    const QString output = runVocabularyDiagnosticTask(request).join(QStringLiteral("\n"));
    QVERIFY(output.contains(QStringLiteral("missing.json")));
    QVERIFY(output.contains(QStringLiteral("0")));
}

void VocabularyDiagnosticTaskTests::reportsReplacementAndPromptBlock()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    VocabularyStore store(QDir(temp.path()).filePath(QStringLiteral("entries.json")));
    QVector<VocabularyEntry> entries;
    entries.append(testEntry());
    QVERIFY(store.saveEntries(entries));

    VocabularyDiagnosticRequest request;
    request.storePath = store.path();
    request.vocabularyEnabled = true;
    request.vocabularyAddMode = QStringLiteral("manual");
    request.vocabularyPromptEntryLimit = 16;

    const QString output = runVocabularyDiagnosticTask(request).join(QStringLiteral("\n"));
    QVERIFY(output.contains(QStringLiteral("deepseep")));
    QVERIFY(output.contains(QStringLiteral("DeepSeek")));
    QVERIFY(output.contains(QStringLiteral("dictate")));
}

QTEST_MAIN(VocabularyDiagnosticTaskTests)

#include "vocabulary_diagnostic_task_tests.moc"
