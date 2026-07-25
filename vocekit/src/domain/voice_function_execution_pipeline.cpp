#include "voice_function_execution_pipeline.h"

VoiceFunctionExecutionPipeline::VoiceFunctionExecutionPipeline()
{
}

VoiceFunctionExecutionPipeline::VoiceFunctionExecutionPipeline(
    const VoiceFunctionExecutionAccess &access
)
    : m_access(access)
{
}

void VoiceFunctionExecutionPipeline::setAccess(
    const VoiceFunctionExecutionAccess &access
)
{
    m_access = access;
}

VoiceFunctionExecutionResult VoiceFunctionExecutionPipeline::execute(
    const VoiceFunctionExecutionRequest &request
) const
{
    VoiceInputProcessingRequest inputRequest;
    inputRequest.modeId = request.modeId;
    inputRequest.selectedText =
        m_access.selectedText ? m_access.selectedText() : QString();
    inputRequest.inputText = request.inputText;
    inputRequest.sourceLabel = request.sourceLabel;
    inputRequest.kind = request.kind;
    inputRequest.networkPolicies = m_access.networkPoliciesFor
        ? m_access.networkPoliciesFor(request.modeId)
        : FunctionNetworkPolicies();
    inputRequest.failureStage = request.failureStage;

    VoiceInputProcessingHandlers inputHandlers;
    inputHandlers.preCorrect = m_access.preCorrect;
    inputHandlers.enrichContext = m_access.enrichContext;
    inputHandlers.shouldStream = m_access.shouldStream;
    inputHandlers.stream = m_access.stream;
    inputHandlers.beforeCompletion = [this, request](
        const VoiceRunContext &context
    ) {
        if (m_access.processingStarted) {
            m_access.processingStarted(
                request.processingStatus,
                context
            );
        }
    };
    if (m_access.completionHandlers) {
        inputHandlers.completion = m_access.completionHandlers();
    }

    VoiceFunctionExecutionResult result;
    result.processing = VoiceInputProcessingPipeline::run(
        inputRequest,
        inputHandlers
    );

    if (m_access.contextUpdated) {
        m_access.contextUpdated(result.processing.context);
    }

    if (result.processing.streamed) {
        result.disposition =
            VoiceFunctionExecutionDisposition::Streamed;
        return result;
    }

    if (!result.processing.completion.ok) {
        result.disposition =
            VoiceFunctionExecutionDisposition::Failed;
        if (m_access.failed) {
            m_access.failed(
                result.processing.context,
                result.processing.completion
            );
        }
        return result;
    }

    result.disposition =
        VoiceFunctionExecutionDisposition::Completed;
    if (m_access.completed) {
        m_access.completed(
            result.processing.context,
            result.processing.completion.finalOutput
        );
    }
    return result;
}
