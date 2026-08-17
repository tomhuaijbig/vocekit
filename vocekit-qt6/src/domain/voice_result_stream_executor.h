#ifndef VOCEKIT_VOICE_RESULT_STREAM_EXECUTOR_H
#define VOCEKIT_VOICE_RESULT_STREAM_EXECUTOR_H

#include "voice_run_context.h"
#include "voice_run_executor.h"

#include <functional>

struct VoiceResultStreamRequest
{
    VoiceRunContext context;
    VoiceRunDeltaCallback onDelta;
};

struct VoiceResultStreamHandlers
{
    std::function<QString(
        const VoiceRunContext &context,
        QString *error,
        const VoiceRunDeltaCallback &onDelta
    )> runContext;

    std::function<QString(
        const VoiceRunContext &context,
        const QString &output
    )> finalizeOutput;
    std::function<bool()> wasCancelled;
};

struct VoiceResultStreamResult
{
    bool ok = false;
    QString rawOutput;
    QString finalOutput;
    QString error;
    QString logAction;
    QString logDetail;
    bool cancelled = false;
};

class VoiceResultStreamExecutor
{
public:
    static VoiceResultStreamResult run(
        const VoiceResultStreamRequest &request,
        const VoiceResultStreamHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_RESULT_STREAM_EXECUTOR_H
