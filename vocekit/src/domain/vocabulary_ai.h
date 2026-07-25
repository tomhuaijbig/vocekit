#ifndef VOCEKIT_VOCABULARY_AI_H
#define VOCEKIT_VOCABULARY_AI_H

#include "app_legacy_types.h"

#include <QString>

// 词库 AI 填充辅助：把表单上下文整理给模型，并把模型 JSON 回复解析成可保存词条。
QString compactPromptField(const QString &label, const QString &value);
QString mergeVocabularyAliasText(const QString &current, const QString &generated);
VocabularySuggestion vocabularySuggestionFromModelReply(
    const QString &reply,
    const QString &fallbackText,
    const QString &scopeId
);

#endif // VOCEKIT_VOCABULARY_AI_H
