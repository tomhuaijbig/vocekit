#ifndef VOCEKIT_VOICE_RECORDING_COMPLETION_EXECUTOR_H
#define VOCEKIT_VOICE_RECORDING_COMPLETION_EXECUTOR_H

#include "voice_speech_recognition_executor.h"
#include "../recording/voice_recording_capture.h"

#include <functional>

struct VoiceRecordingCompletionRequest
{
    QString modeId;
    QString provider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
};

struct VoiceRecordingCompletionHandlers
{
    std::function<VoiceRecordingStopResult()> stopRecording;
    VoiceSpeechRecognitionHandlers recognition;
};

struct VoiceRecordingCompletionResult
{
    bool ok = false;
    VoiceRecordingStopResult recording;
    VoiceSpeechRecognitionResult speech;
    QString error;
};

// 录音完成执行器：停止普通录音，并把采集到的音频交给语音识别执行器。
class VoiceRecordingCompletionExecutor
{
public:
    static VoiceRecordingCompletionResult run(
        const VoiceRecordingCompletionRequest &request,
        const VoiceRecordingCompletionHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_RECORDING_COMPLETION_EXECUTOR_H
