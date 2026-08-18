#ifndef VOCEKIT_VOICE_MODEL_PROCESSING_TASK_H
#define VOCEKIT_VOICE_MODEL_PROCESSING_TASK_H

#include "../providers/provider_types.h"
#include "cancellation_token.h"
#include "voice_model_runtime_settings.h"

#include <QString>

#include <functional>

using VocabularyPromptBlockBuilder = std::function<QString(
    const QString &modeId,
    const QString &userText,
    bool hasVoiceInput
)>;

struct VoiceModelProcessingRequest
{
    VoiceModelRuntimeSettings runtime;
    QString modeId;
    QString primaryText;
    QString selectedText;
    QString voiceInstruction;
    QString modelOverride;
    QString extraInstruction;
    bool hasVoiceInput = false;
    VocabularyPromptBlockBuilder vocabularyPromptBlockBuilder;
    ModelDeltaCallback onDelta;
    CancellationToken cancellation;
};

struct VoiceModelProcessingResult
{
    QString text;
    QByteArray rawResponse;
    QString errorMessage;
    QString promptVersion;
    qint64 durationMs = -1;
    ModelRequestTelemetry telemetry;
    bool modelSkipped = false;
    bool cancelled = false;
};

// 将听写、翻译、问答和自定义功能的大模型阶段集中在任务层。
// UI 只需要传入上下文和回调，不再直接拼装每一种提示词请求。
VoiceModelProcessingResult processVoiceModelRequest(
    const VoiceModelProcessingRequest &request
);

#endif // VOCEKIT_VOICE_MODEL_PROCESSING_TASK_H
