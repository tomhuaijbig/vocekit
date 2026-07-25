#ifndef VOCEKIT_VOICE_RESULT_COMPLETION_EXECUTOR_H
#define VOCEKIT_VOICE_RESULT_COMPLETION_EXECUTOR_H

#include "voice_run_context.h"

#include <functional>

struct VoiceResultCompletionRequest
{
    VoiceRunContext context;
    QString failureStage;
};

struct VoiceResultCompletionHandlers
{
    std::function<QString(
        const VoiceRunContext &context,
        QString *error
    )> runContext;

    std::function<QString(
        const VoiceRunContext &context,
        const QString &output
    )> finalizeOutput;
    std::function<bool()> wasCancelled;
};

struct VoiceResultCompletionResult
{
    bool ok = false;
    QString rawOutput;
    QString finalOutput;
    QString error;
    QString logAction;
    QString logDetail;
    bool cancelled = false;
};

class VoiceResultCompletionExecutor
{
public:
    static VoiceResultCompletionResult run(
        const VoiceResultCompletionRequest &request,
        const VoiceResultCompletionHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_RESULT_COMPLETION_EXECUTOR_H
