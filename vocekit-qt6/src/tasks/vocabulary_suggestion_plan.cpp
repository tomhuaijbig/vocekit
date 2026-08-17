#include "vocabulary_suggestion_plan.h"

#include "../storage/vocabulary_store.h"

#include <QStringList>

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

} // namespace

VocabularySuggestionPromptPlan buildVocabularySuggestionPromptPlan(
    const VocabularySuggestionInput &input)
{
    VocabularySuggestionPromptPlan plan;
    const QString source = input.sourceText.trimmed();
    const QString edited = input.editedText.trimmed();
    if (source.isEmpty() && edited.isEmpty()) {
        plan.errorMessage = text("没有可生成词条的文字。");
        return plan;
    }

    plan.valid = true;
    plan.fallbackText = edited.isEmpty() ? source : edited;
    plan.scopeId = normalizeVocabularyScope(input.scopeId);

    QStringList userParts;
    userParts << text(
        "任务：生成一个词库词条，用于后续自动修正识别文本或模型输出。"
    );
    userParts << text("默认作用范围：") + plan.scopeId;
    if (edited.isEmpty()) {
        userParts << text("用户提供的文字：\n") + source;
        userParts << text(
            "请从中找出最像专有名词、固定译名、易错词或需要统一写法的一个词条。"
        );
    } else {
        userParts << text("修改前或原始文字：\n") + source;
        userParts << text("修改后或标准写法：\n") + edited;
        userParts << text(
            "请优先从修改差异中生成一个最值得加入词库的词条。"
        );
    }
    if (!input.extraContext.trimmed().isEmpty()) {
        userParts << text("当前表单草稿：\n")
            + input.extraContext.trimmed();
    }
    userParts << text(
        "严格要求：只输出一个 JSON 对象；不要输出解释；source 和 target "
        "必须是短词、短语或固定名称，不要是一整段长句；如果已经有明确标准写法，"
        "target 必须使用它。"
    );
    plan.userPrompt = userParts.join(text("\n\n"));
    return plan;
}
