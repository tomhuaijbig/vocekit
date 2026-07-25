#include "vocabulary_suggestion_task.h"

#include "../domain/vocabulary_ai.h"
#include "model_request_task.h"

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

} // namespace

VocabularySuggestion requestVocabularySuggestion(
    const VocabularySuggestionTaskRequest &request,
    QString *error
)
{
    VocabularySuggestion suggestion;
    const VocabularySuggestionPromptPlan plan =
        buildVocabularySuggestionPromptPlan(request.input);
    if (!plan.valid) {
        if (error) {
            *error = plan.errorMessage;
        }
        return suggestion;
    }

    const QString model = request.modelId.trimmed();
    if (!isModelProviderAvailableForTask(model)) {
        if (error) {
            *error = text("缺少 DeepSeek 密钥。请先在“设置 -> 接口”中填写 DeepSeek API Key，或把词库加入方式改成“不使用 AI”。");
        }
        return suggestion;
    }

    ModelRequestTaskRequest modelRequest;
    modelRequest.modelId = model;
    modelRequest.systemPrompt = request.systemPrompt;
    modelRequest.userPrompt = plan.userPrompt;
    modelRequest.useSystemProxy = request.useSystemProxy;
    modelRequest.networkPolicy = QStringLiteral("inherit");
    const ModelRequestTaskResult modelResult =
        runModelProviderRequestTask(modelRequest);
    const QString reply = modelResult.text;
    if (reply.trimmed().isEmpty()) {
        if (error) {
            *error = modelResult.errorMessage.isEmpty()
                ? text("模型没有返回可用词条。")
                : modelResult.errorMessage;
        }
        return suggestion;
    }

    suggestion = vocabularySuggestionFromModelReply(
        reply,
        plan.fallbackText,
        plan.scopeId
    );
    if (!suggestion.valid && error) {
        *error = text("模型没有生成可产生修正效果的词条。原词/错词和标准写法不能完全一样且别名为空；请补充一个常见错词或别名。");
    }
    return suggestion;
}
