#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/domain/vocabulary_runtime.h"

class VocabularyRuntimeTests : public QObject
{
    Q_OBJECT

private slots:
    void respectsGlobalAndVoiceOnlySettings()
    {
        AppSettingsData settings;
        settings.vocabularyEnabled = false;
        QCOMPARE(isVocabularyEnabledForRun(settings, true), false);

        settings.vocabularyEnabled = true;
        settings.vocabularyOnlyForVoiceInput = true;
        QCOMPARE(isVocabularyEnabledForRun(settings, false), false);
        QCOMPARE(isVocabularyEnabledForRun(settings, true), true);
    }

    void leavesUserTextUnchangedWhenInjectionIsDisabled()
    {
        AppSettingsData settings;
        settings.vocabularyEnabled = true;
        settings.vocabularyPromptEntryLimit = 0;

        QCOMPARE(
            addVocabularyPromptBlockForRun(
                settings,
                QStringLiteral("dictate"),
                QStringLiteral("hello"),
                true
            ),
            QStringLiteral("hello")
        );
    }
};

QTEST_MAIN(VocabularyRuntimeTests)
#include "vocabulary_runtime_tests.moc"
