#ifndef VOCEKIT_VOCABULARY_SUGGESTION_PLAN_H
#define VOCEKIT_VOCABULARY_SUGGESTION_PLAN_H

#include <QString>

struct VocabularySuggestionInput
{
    QString sourceText;
    QString scopeId;
    QString editedText;
    QString extraContext;
};

struct VocabularySuggestionPromptPlan
{
    bool valid = false;
    QString errorMessage;
    QString fallbackText;
    QString scopeId;
    QString userPrompt;
};

// 把词库表单输入整理成稳定的模型提示内容，不读取界面或全局设置。
VocabularySuggestionPromptPlan buildVocabularySuggestionPromptPlan(
    const VocabularySuggestionInput &input
);

#endif // VOCEKIT_VOCABULARY_SUGGESTION_PLAN_H
