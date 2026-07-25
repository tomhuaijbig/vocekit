#include "../../src/domain/vocabulary_candidates.h"

#include <QtTest>

namespace {

HistoryEntry historyEntry(
    const QString &modeId,
    const QString &mode,
    const QString &input,
    const QString &output
)
{
    HistoryEntry entry;
    entry.modeId = modeId;
    entry.mode = mode;
    entry.input = input;
    entry.output = output;
    return entry;
}

VocabularyEntry vocabularyEntry(
    const QString &source,
    const QString &target,
    const QString &scopeId = QStringLiteral("__global")
)
{
    VocabularyEntry entry;
    entry.source = source;
    entry.target = target;
    entry.scopeId = scopeId;
    entry.matchMode = QStringLiteral("caseInsensitive");
    entry.enabled = true;
    return entry;
}

} // namespace

class VocabularyCandidatesTests : public QObject
{
    Q_OBJECT

private slots:
    void recommendsCaseCorrectionsAndCountsRepeatedTokens();
    void skipsExistingCorrections();
    void mapsCustomFunctionHistoryToCustomScope();
    void respectsHistoryAndCandidateLimits();
};

void VocabularyCandidatesTests::recommendsCaseCorrectionsAndCountsRepeatedTokens()
{
    VocabularyCandidateRequest request;
    request.history = {
        historyEntry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("DeepSeek"), QStringLiteral("DeepSeek")),
        historyEntry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("deepseek"), QStringLiteral("plain lowercase"))
    };

    const QVector<VocabularyCandidate> candidates = buildVocabularyCandidates(request);
    QCOMPARE(candidates.size(), 1);
    QCOMPARE(candidates.first().entry.source, QStringLiteral("deepseek"));
    QCOMPARE(candidates.first().entry.target, QStringLiteral("DeepSeek"));
    QCOMPARE(candidates.first().entry.scopeId, QStringLiteral("dictate"));
    QCOMPARE(candidates.first().score, 2);
    QVERIFY(candidates.first().reason.contains(QStringLiteral("历史记录")));
}

void VocabularyCandidatesTests::skipsExistingCorrections()
{
    VocabularyCandidateRequest request;
    request.existingEntries = {
        vocabularyEntry(QStringLiteral("openai"), QStringLiteral("OpenAI"), QStringLiteral("translate"))
    };
    request.history = {
        historyEntry(QStringLiteral("translate"), QStringLiteral("翻译"), QStringLiteral("OpenAI"), QString())
    };

    const QVector<VocabularyCandidate> candidates = buildVocabularyCandidates(request);
    QVERIFY(candidates.isEmpty());
}

void VocabularyCandidatesTests::mapsCustomFunctionHistoryToCustomScope()
{
    CustomFunctionDef custom;
    custom.id = QStringLiteral("custom_polish");
    custom.name = QStringLiteral("论文润色");

    VocabularyCandidateRequest request;
    request.customFunctions = {custom};
    request.history = {
        historyEntry(QString(), QStringLiteral("论文润色"), QStringLiteral("VoceKit"), QString())
    };

    const QVector<VocabularyCandidate> candidates = buildVocabularyCandidates(request);
    QCOMPARE(candidates.size(), 1);
    QCOMPARE(candidates.first().entry.scopeId, QStringLiteral("custom_polish"));
    QCOMPARE(candidates.first().entry.source, QStringLiteral("vocekit"));
    QCOMPARE(candidates.first().entry.target, QStringLiteral("VoceKit"));
}

void VocabularyCandidatesTests::respectsHistoryAndCandidateLimits()
{
    VocabularyCandidateRequest request;
    request.maxHistoryRecords = 1;
    request.maxCandidates = 1;
    request.history = {
        historyEntry(QStringLiteral("ask"), QStringLiteral("问答"), QStringLiteral("ZedAlpha ZedAlpha"), QString()),
        historyEntry(QStringLiteral("ask"), QStringLiteral("问答"), QStringLiteral("BetaName"), QString())
    };

    const QVector<VocabularyCandidate> candidates = buildVocabularyCandidates(request);
    QCOMPARE(candidates.size(), 1);
    QCOMPARE(candidates.first().entry.target, QStringLiteral("ZedAlpha"));
    QCOMPARE(candidates.first().score, 2);
}

QTEST_MAIN(VocabularyCandidatesTests)

#include "vocabulary_candidates_tests.moc"
