#ifndef VOCEKIT_VOICE_RUN_EXECUTOR_H
#define VOCEKIT_VOICE_RUN_EXECUTOR_H

#include "voice_run_planner.h"
#include "../tasks/voice_model_processing_task.h"

#include <functional>

using VoiceRunDeltaCallback = std::function<void(const QString &)>;

struct VoiceRunExecutionRequest
{
    VoiceRunContext context;
    QString modelOverride;
    QString extraInstruction;
    VoiceRunDeltaCallback onDelta;
    CancellationToken cancellation;
};

struct VoiceRunExecutionHandlers
{
    std::function<VoiceModelRuntimeSettings(
        const QString &modeId
    )> runtimeSettings;
    VocabularyPromptBlockBuilder vocabularyPromptBlockBuilder;
    std::function<VoiceModelProcessingResult(
        const VoiceModelProcessingRequest &request
    )> processModelRequest;
    std::function<void(
        qint64 durationMs,
        const QString &promptVersion
    )> modelResultRecorded;
    std::function<void(
        const VoiceModelProcessingResult &result
    )> modelDetailsRecorded;
};

// 模型执行管线：把功能上下文转换为统一模型任务，并记录模型阶段结果。
// 网络实现仍由注入的任务回调负责，执行器本身不依赖界面对象。
class VoiceRunExecutor
{
public:
    static QString run(
        const VoiceRunExecutionRequest &request,
        const VoiceRunExecutionHandlers &handlers,
        QString *error
    );
};

#endif // VOCEKIT_VOICE_RUN_EXECUTOR_H
