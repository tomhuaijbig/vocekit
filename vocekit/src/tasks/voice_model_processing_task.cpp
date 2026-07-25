#include "voice_model_processing_task.h"

#include "model_request_task.h"

namespace {

QString s(const char *text)
{
    return QString::fromUtf8(text);
}

QString trimmedOrFallback(const QString &value, const QString &fallback)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QString withVocabularyBlock(
    const VoiceModelProcessingRequest &request,
    const QString &modeId,
    const QString &userText
)
{
    if (!request.vocabularyPromptBlockBuilder) {
        return userText;
    }
    return request.vocabularyPromptBlockBuilder(
        modeId,
        userText,
        request.hasVoiceInput
    );
}

void appendExtraInstruction(QString *userText, const QString &extraInstruction)
{
    if (!userText) {
        return;
    }
    const QString trimmed = extraInstruction.trimmed();
    if (!trimmed.isEmpty()) {
        *userText += s("\n\n继续追问或补充要求：\n") + trimmed;
    }
}

VoiceModelProcessingResult skippedResult(const QString &text)
{
    VoiceModelProcessingResult result;
    result.text = text;
    result.durationMs = -1;
    result.modelSkipped = true;
    return result;
}

VoiceModelProcessingResult runModel(
    const VoiceModelProcessingRequest &request,
    const QString &model,
    const QString &prompt,
    const QString &userText
)
{
    VoiceModelProcessingResult result;

    ModelRequestTaskRequest taskRequest;
    taskRequest.modelId = model;
    taskRequest.systemPrompt = prompt;
    taskRequest.userPrompt = userText;
    taskRequest.stream = static_cast<bool>(request.onDelta);
    taskRequest.useSystemProxy = request.runtime.useSystemProxy;
    taskRequest.networkPolicy = QStringLiteral("inherit");
    taskRequest.cancellation = request.cancellation;

    const ModelRequestTaskResult taskResult =
        runModelProviderRequestTask(taskRequest, request.onDelta);
    result.text = taskResult.text;
    result.errorMessage = taskResult.errorMessage;
    result.promptVersion = taskResult.promptVersion;
    result.durationMs = taskResult.durationMs;
    result.cancelled = taskResult.cancelled;
    return result;
}

VoiceModelProcessingResult processDictate(
    const VoiceModelProcessingRequest &request
)
{
    if (!request.runtime.dictatePolishEnabled
        && request.modelOverride.trimmed().isEmpty()
        && request.extraInstruction.trimmed().isEmpty()) {
        return skippedResult(request.primaryText);
    }

    const QString model = trimmedOrFallback(
        request.modelOverride,
        request.runtime.defaultModel
    );
    if (!isModelProviderAvailableForTask(model)) {
        return skippedResult(request.primaryText);
    }

    const QString prompt = trimmedOrFallback(
        request.runtime.systemPrompt,
        defaultVoiceModelSystemPrompt(QStringLiteral("dictate"))
    );
    QString user = withVocabularyBlock(
        request,
        QStringLiteral("dictate"),
        s("识别文本：\n") + request.primaryText
    );
    if (!request.extraInstruction.trimmed().isEmpty()) {
        user += s("\n\n补充要求：\n") + request.extraInstruction.trimmed();
    }
    return runModel(request, model, prompt, user);
}

VoiceModelProcessingResult processTranslate(
    const VoiceModelProcessingRequest &request
)
{
    const QString prompt = trimmedOrFallback(
        request.runtime.systemPrompt,
        defaultVoiceModelSystemPrompt(QStringLiteral("translate"))
    );
    QString user = withVocabularyBlock(
        request,
        QStringLiteral("translate"),
        s("目标语言：简体中文\n待翻译内容：\n") + request.primaryText
    );
    if (!request.voiceInstruction.trimmed().isEmpty()) {
        user += s("\n\n语音补充要求：\n") + request.voiceInstruction;
    }
    appendExtraInstruction(&user, request.extraInstruction);

    const QString model = trimmedOrFallback(
        request.modelOverride,
        request.runtime.defaultModel
    );
    return runModel(request, model, prompt, user);
}

VoiceModelProcessingResult processAsk(
    const VoiceModelProcessingRequest &request
)
{
    const QString prompt = trimmedOrFallback(
        request.runtime.systemPrompt,
        defaultVoiceModelSystemPrompt(QStringLiteral("ask"))
    );
    const QString question = request.primaryText.trimmed().isEmpty()
        ? s("请直接分析或处理选中的文字。")
        : request.primaryText;
    QString user = withVocabularyBlock(
        request,
        QStringLiteral("ask"),
        s("选中文本：\n")
            + (request.selectedText.isEmpty() ? s("（无）") : request.selectedText)
            + s("\n\n用户问题：\n")
            + question
    );
    appendExtraInstruction(&user, request.extraInstruction);

    const QString model = trimmedOrFallback(
        request.modelOverride,
        request.runtime.defaultModel
    );
    return runModel(request, model, prompt, user);
}

VoiceModelProcessingResult processCustom(
    const VoiceModelProcessingRequest &request
)
{
    const QString functionId = request.modeId.trimmed();
    const QString prompt = trimmedOrFallback(
        request.runtime.systemPrompt,
        defaultVoiceModelSystemPrompt(functionId)
    );
    const QString instruction = request.primaryText.trimmed().isEmpty()
        ? s("请直接按照提示词处理选中的文字。")
        : request.primaryText;
    QString user = withVocabularyBlock(
        request,
        functionId,
        s("选中文本：\n")
            + (request.selectedText.isEmpty() ? s("（无）") : request.selectedText)
            + s("\n\n用户要求：\n")
            + instruction
    );
    appendExtraInstruction(&user, request.extraInstruction);

    const QString model = trimmedOrFallback(
        request.modelOverride,
        request.runtime.defaultModel
    );
    return runModel(request, model, prompt, user);
}

} // namespace

VoiceModelProcessingResult processVoiceModelRequest(
    const VoiceModelProcessingRequest &request
)
{
    if (request.modeId == QStringLiteral("dictate")) {
        return processDictate(request);
    }
    if (request.modeId == QStringLiteral("translate")) {
        return processTranslate(request);
    }
    if (request.modeId == QStringLiteral("ask")) {
        return processAsk(request);
    }
    return processCustom(request);
}
