#ifndef VOCEKIT_VOICE_INPUT_PROCESSING_PIPELINE_H
#define VOCEKIT_VOICE_INPUT_PROCESSING_PIPELINE_H

#include "voice_result_completion_executor.h"

#include <functional>

enum class VoiceInputProcessingKind
{
    Voice,
    TextOnly
};

struct VoiceInputProcessingRequest
{
    QString modeId;
    QString selectedText;
    QString inputText;
    QString sourceLabel;
    VoiceInputProcessingKind kind = VoiceInputProcessingKind::Voice;
    FunctionNetworkPolicies networkPolicies;
    QString failureStage;
};

struct VoiceInputProcessingHandlers
{
    std::function<QString(
        const QString &text,
        const QString &modeId,
        const QString &sourceLabel,
        bool hasVoiceInput
    )> preCorrect;
    std::function<void(VoiceRunContext *context)> enrichContext;
    std::function<bool(const VoiceRunContext &context)> shouldStream;
    std::function<void(const VoiceRunContext &context)> stream;
    std::function<void(const VoiceRunContext &context)> beforeCompletion;
    VoiceResultCompletionHandlers completion;
};

struct VoiceInputProcessingResult
{
    bool ok = false;
    bool streamed = false;
    QString correctedText;
    VoiceRunContext context;
    VoiceResultCompletionResult completion;
};

// 输入处理管线：统一词库修正、上下文构造、流式分流和模型完成流程。
class VoiceInputProcessingPipeline
{
public:
    static VoiceInputProcessingResult run(
        const VoiceInputProcessingRequest &request,
        const VoiceInputProcessingHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_INPUT_PROCESSING_PIPELINE_H
