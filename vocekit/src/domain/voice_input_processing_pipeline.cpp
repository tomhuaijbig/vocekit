#include "voice_input_processing_pipeline.h"

namespace {

VoiceRunContext buildContext(
    const VoiceInputProcessingRequest &request,
    const QString &correctedText
)
{
    VoiceRunContext context;
    context.modeId = request.modeId;
    context.selectedText = request.selectedText;
    context.networkPolicies = request.networkPolicies;
    context.textOnly = request.kind == VoiceInputProcessingKind::TextOnly;
    if (context.textOnly) {
        context.textOnlyInput = correctedText;
    } else {
        context.voiceText = correctedText;
    }
    return context;
}

} // namespace

VoiceInputProcessingResult VoiceInputProcessingPipeline::run(
    const VoiceInputProcessingRequest &request,
    const VoiceInputProcessingHandlers &handlers
)
{
    VoiceInputProcessingResult result;
    const bool hasVoiceInput =
        request.kind == VoiceInputProcessingKind::Voice;
    result.correctedText = handlers.preCorrect
        ? handlers.preCorrect(
            request.inputText,
            request.modeId,
            request.sourceLabel,
            hasVoiceInput
        )
        : request.inputText;
    result.context = buildContext(request, result.correctedText);
    if (handlers.enrichContext) {
        handlers.enrichContext(&result.context);
    }

    if (handlers.shouldStream
        && handlers.stream
        && handlers.shouldStream(result.context)) {
        handlers.stream(result.context);
        result.streamed = true;
        result.ok = true;
        return result;
    }

    if (handlers.beforeCompletion) {
        handlers.beforeCompletion(result.context);
    }
    VoiceResultCompletionRequest completionRequest;
    completionRequest.context = result.context;
    completionRequest.failureStage = request.failureStage;
    result.completion = VoiceResultCompletionExecutor::run(
        completionRequest,
        handlers.completion
    );
    result.ok = result.completion.ok;
    return result;
}
