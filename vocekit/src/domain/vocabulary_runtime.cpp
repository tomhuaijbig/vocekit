#include "vocabulary_runtime.h"

#include "../storage/vocabulary_store.h"

bool isVocabularyEnabledForRun(
    const AppSettingsData &settings,
    bool hasVoiceInput)
{
    if (!settings.vocabularyEnabled) {
        return false;
    }
    return !settings.vocabularyOnlyForVoiceInput || hasVoiceInput;
}

QString applyVocabularyPreCorrectionForRun(
    const AppSettingsData &settings,
    const QString &text,
    const QString &modeId,
    bool hasVoiceInput
)
{
    return applyVocabularyEntries(
        text,
        modeId,
        isVocabularyEnabledForRun(settings, hasVoiceInput)
    );
}

QString applyVocabularyPostCorrectionForRun(
    const AppSettingsData &settings,
    const QString &text,
    const QString &modeId,
    bool hasVoiceInput
)
{
    return applyVocabularyEntries(
        text,
        modeId,
        isVocabularyEnabledForRun(settings, hasVoiceInput)
    );
}

QString addVocabularyPromptBlockForRun(
    const AppSettingsData &settings,
    const QString &modeId,
    const QString &userText,
    bool hasVoiceInput
)
{
    const QString block = vocabularyPromptBlock(
        modeId,
        isVocabularyEnabledForRun(settings, hasVoiceInput),
        userText,
        settings.vocabularyPromptEntryLimit
    );
    if (block.trimmed().isEmpty()) {
        return userText;
    }
    return block + QStringLiteral("\n\n") + userText;
}
