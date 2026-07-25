#ifndef VOCEKIT_VOICE_RESULT_RERUN_EXECUTOR_H
#define VOCEKIT_VOICE_RESULT_RERUN_EXECUTOR_H

#include "voice_run_context.h"
#include "voice_run_executor.h"

#include <functional>

struct VoiceResultRerunRequest
{
    VoiceRunContext context;
    QString modelOverride;
    QString extraInstruction;
    QString defaultModel;
    qint64 elapsedMs = -1;
    VoiceRunDeltaCallback onDelta;
};

struct VoiceResultRerunHandlers
{
    std::function<QString(
        const VoiceRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction,
        QString *error,
        const VoiceRunDeltaCallback &onDelta
    )> runContext;

    std::function<QString(
        const VoiceRunContext &context,
        const QString &output
    )> finalizeOutput;
    std::function<bool()> wasCancelled;
};

struct VoiceResultRerunResult
{
    bool ok = false;
    QString rawOutput;
    QString finalOutput;
    QString finalModel;
    QString error;
    QString logAction;
    QString logDetail;
    bool cancelled = false;
};

class VoiceResultRerunExecutor
{
public:
    static VoiceResultRerunResult run(
        const VoiceResultRerunRequest &request,
        const VoiceResultRerunHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_RESULT_RERUN_EXECUTOR_H
