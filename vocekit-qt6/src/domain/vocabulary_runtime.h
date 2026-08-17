#ifndef VOCEKIT_VOCABULARY_RUNTIME_H
#define VOCEKIT_VOCABULARY_RUNTIME_H

#include "../config/app_settings_data.h"

#include <QString>

// 词库运行策略：判断本次功能是否启用词库，并把相关词条注入模型提示或应用到文本。
bool isVocabularyEnabledForRun(
    const AppSettingsData &settings,
    bool hasVoiceInput
);
QString applyVocabularyPreCorrectionForRun(
    const AppSettingsData &settings,
    const QString &text,
    const QString &modeId,
    bool hasVoiceInput
);
QString applyVocabularyPostCorrectionForRun(
    const AppSettingsData &settings,
    const QString &text,
    const QString &modeId,
    bool hasVoiceInput
);
QString addVocabularyPromptBlockForRun(
    const AppSettingsData &settings,
    const QString &modeId,
    const QString &userText,
    bool hasVoiceInput
);

#endif // VOCEKIT_VOCABULARY_RUNTIME_H
