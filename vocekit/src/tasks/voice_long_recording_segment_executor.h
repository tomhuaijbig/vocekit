#ifndef VOCEKIT_VOICE_LONG_RECORDING_SEGMENT_EXECUTOR_H
#define VOCEKIT_VOICE_LONG_RECORDING_SEGMENT_EXECUTOR_H

#include "cancellation_token.h"
#include "voice_speech_recognition_executor.h"
#include "../recording/segmented_recording.h"

#include <QVector>

// 长录音单段识别模块：集中处理重试、取消、耗时合并和状态写回。
struct VoiceLongRecordingSegmentRequest
{
    VoiceSpeechRecognitionRequest speech;
    CancellationToken cancellation;
};

struct VoiceLongRecordingSegmentResult
{
    int index = 0;
    ExecutionId executionId;
    bool ok = false;
    bool cancelled = false;
    int attempts = 0;
    QString text;
    QString error;
    qint64 elapsedMs = 0;
    QVector<VoiceSpeechRecognitionResult> attemptResults;
};

class VoiceLongRecordingSegmentExecutor
{
public:
    static VoiceLongRecordingSegmentResult run(
        const VoiceLongRecordingSegmentRequest &request,
        const VoiceSpeechRecognitionHandlers &handlers
    );

    static void apply(
        const VoiceLongRecordingSegmentResult &result,
        SegmentedRecordingState *state
    );
};

#endif // VOCEKIT_VOICE_LONG_RECORDING_SEGMENT_EXECUTOR_H
