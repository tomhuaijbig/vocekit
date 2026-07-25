#ifndef VOCEKIT_VOICE_FUNCTION_EXECUTION_PIPELINE_H
#define VOCEKIT_VOICE_FUNCTION_EXECUTION_PIPELINE_H

#include "voice_input_processing_pipeline.h"

#include <functional>

struct VoiceFunctionExecutionRequest
{
    QString modeId;
    QString inputText;
    VoiceInputProcessingKind kind = VoiceInputProcessingKind::Voice;
    QString sourceLabel;
    QString failureStage;
    QString processingStatus;
};

struct VoiceFunctionExecutionAccess
{
    std::function<QString()> selectedText;
    std::function<FunctionNetworkPolicies(const QString &)> networkPoliciesFor;
    std::function<QString(
        const QString &text,
        const QString &modeId,
        const QString &sourceLabel,
        bool hasVoiceInput
    )> preCorrect;
    std::function<void(VoiceRunContext *context)> enrichContext;
    std::function<bool(const VoiceRunContext &context)> shouldStream;
    std::function<void(const VoiceRunContext &context)> stream;
    std::function<void(
        const QString &status,
        const VoiceRunContext &context
    )> processingStarted;
    std::function<VoiceResultCompletionHandlers()> completionHandlers;
    std::function<void(const VoiceRunContext &context)> contextUpdated;
    std::function<void(
        const VoiceRunContext &context,
        const VoiceResultCompletionResult &completion
    )> failed;
    std::function<void(
        const VoiceRunContext &context,
        const QString &finalOutput
    )> completed;
};

enum class VoiceFunctionExecutionDisposition
{
    Completed,
    Streamed,
    Failed
};

struct VoiceFunctionExecutionResult
{
    VoiceFunctionExecutionDisposition disposition =
        VoiceFunctionExecutionDisposition::Failed;
    VoiceInputProcessingResult processing;
};

// 功能执行管线统一输入请求、上下文准备、流式分流以及最终成功或失败出口。
class VoiceFunctionExecutionPipeline
{
public:
    VoiceFunctionExecutionPipeline();
    explicit VoiceFunctionExecutionPipeline(
        const VoiceFunctionExecutionAccess &access
    );

    void setAccess(const VoiceFunctionExecutionAccess &access);
    VoiceFunctionExecutionResult execute(
        const VoiceFunctionExecutionRequest &request
    ) const;

private:
    VoiceFunctionExecutionAccess m_access;
};

#endif // VOCEKIT_VOICE_FUNCTION_EXECUTION_PIPELINE_H
