#include "voice_run_executor.h"

namespace {

QString missingHandlerError()
{
    return QString::fromUtf8("功能执行器未配置。");
}

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString primaryTextForPlan(const VoiceRunModelPlan &plan)
{
    switch (plan.operation) {
    case VoiceRunOperation::Dictate:
    case VoiceRunOperation::Translate:
        return plan.primaryText;
    case VoiceRunOperation::Ask:
        return plan.question;
    case VoiceRunOperation::Custom:
        return plan.customVoiceText;
    }
    return QString();
}

VoiceModelProcessingRequest buildModelRequest(
    const VoiceRunExecutionRequest &request,
    const VoiceRunModelPlan &plan,
    const VoiceRunExecutionHandlers &handlers
)
{
    VoiceModelProcessingRequest modelRequest;
    modelRequest.modeId = request.context.modeId;
    modelRequest.runtime = handlers.runtimeSettings
        ? handlers.runtimeSettings(request.context.modeId)
        : VoiceModelRuntimeSettings();
    modelRequest.primaryText = primaryTextForPlan(plan);
    modelRequest.selectedText = request.context.selectedText;
    modelRequest.voiceInstruction = plan.voiceInstruction;
    modelRequest.modelOverride = request.modelOverride;
    modelRequest.extraInstruction = plan.extraInstruction;
    modelRequest.hasVoiceInput = plan.hasVoiceInput;
    modelRequest.vocabularyPromptBlockBuilder =
        handlers.vocabularyPromptBlockBuilder;
    modelRequest.onDelta = request.onDelta;
    modelRequest.cancellation = request.cancellation;
    return modelRequest;
}

} // namespace

QString VoiceRunExecutor::run(
    const VoiceRunExecutionRequest &request,
    const VoiceRunExecutionHandlers &handlers,
    QString *error
)
{
    if (error) {
        error->clear();
    }
    if (!handlers.processModelRequest) {
        setError(error, missingHandlerError());
        return QString();
    }

    const VoiceRunModelPlan plan =
        VoiceRunPlanner::plan(request.context, request.extraInstruction);
    const VoiceModelProcessingRequest modelRequest =
        buildModelRequest(request, plan, handlers);
    const VoiceModelProcessingResult result =
        handlers.processModelRequest(modelRequest);

    if (handlers.modelResultRecorded) {
        handlers.modelResultRecorded(
            result.durationMs,
            result.promptVersion
        );
    }
    if (handlers.modelDetailsRecorded) {
        handlers.modelDetailsRecorded(result);
    }
    if (!result.errorMessage.isEmpty()) {
        setError(error, result.errorMessage);
    }
    return result.text;
}
