#include "screenshot_text_action_task.h"

#include "model_request_task.h"

namespace {

QString s(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

OcrAiTaskResult runScreenshotTextActionTask(
    const ScreenshotTextActionTaskRequest &request
)
{
    OcrAiTaskResult result;
    if (!isModelProviderAvailableForTask(request.model)) {
        result.error = s(
            "当前模型缺少接口配置，请先在“设置 -> 接口”中填写密钥。"
        );
        return result;
    }

    ModelRequestTaskRequest modelRequest;
    modelRequest.modelId = request.model;
    modelRequest.systemPrompt = request.systemPrompt;
    modelRequest.userPrompt = request.sourceText;
    modelRequest.useSystemProxy = request.useSystemProxy;
    modelRequest.networkPolicy = QStringLiteral("inherit");
    modelRequest.cancellation = request.cancellation;
    const ModelRequestTaskResult modelResult =
        runModelProviderRequestTask(modelRequest);
    result.text = modelResult.text.trimmed();
    result.error = modelResult.errorMessage;
    if (result.text.isEmpty() && result.error.isEmpty()) {
        result.error = s("模型没有返回可用内容。");
    }
    return result;
}
