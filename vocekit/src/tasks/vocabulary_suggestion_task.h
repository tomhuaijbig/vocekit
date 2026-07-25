#ifndef VOCEKIT_VOCABULARY_SUGGESTION_TASK_H
#define VOCEKIT_VOCABULARY_SUGGESTION_TASK_H

#include "../domain/app_legacy_types.h"
#include "vocabulary_suggestion_plan.h"

#include <QString>

struct VocabularySuggestionTaskRequest
{
    VocabularySuggestionInput input;
    QString modelId = QStringLiteral("deepseek-v4-flash");
    QString systemPrompt;
    bool useSystemProxy = false;
};

// 通过已配置的大模型生成一个词库建议，不直接读取全局设置。
VocabularySuggestion requestVocabularySuggestion(
    const VocabularySuggestionTaskRequest &request,
    QString *error
);

#endif // VOCEKIT_VOCABULARY_SUGGESTION_TASK_H
