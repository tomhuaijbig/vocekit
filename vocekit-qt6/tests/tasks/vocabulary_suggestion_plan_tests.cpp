#include <QtTest>

#include "../../src/tasks/vocabulary_suggestion_plan.h"

class VocabularySuggestionPlanTests : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingSourceAndEditedText()
    {
        const VocabularySuggestionInput input;
        const VocabularySuggestionPromptPlan plan =
            buildVocabularySuggestionPromptPlan(input);

        QCOMPARE(plan.valid, false);
        QVERIFY(plan.errorMessage.contains(QString::fromUtf8("没有")));
    }

    void prioritizesEditedTextAndKeepsContext()
    {
        VocabularySuggestionInput input;
        input.sourceText = QStringLiteral("deep seek");
        input.editedText = QStringLiteral("DeepSeek");
        input.scopeId = QStringLiteral("dictate");
        input.extraContext = QStringLiteral("product name");

        const VocabularySuggestionPromptPlan plan =
            buildVocabularySuggestionPromptPlan(input);

        QCOMPARE(plan.valid, true);
        QCOMPARE(plan.fallbackText, QStringLiteral("DeepSeek"));
        QCOMPARE(plan.scopeId, QStringLiteral("dictate"));
        QVERIFY(plan.userPrompt.contains(QStringLiteral("deep seek")));
        QVERIFY(plan.userPrompt.contains(QStringLiteral("DeepSeek")));
        QVERIFY(plan.userPrompt.contains(QStringLiteral("product name")));
    }
};

QTEST_MAIN(VocabularySuggestionPlanTests)
#include "vocabulary_suggestion_plan_tests.moc"
