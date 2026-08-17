#ifndef VOCEKIT_VOICE_LONG_RECORDING_COMPLETION_EXECUTOR_H
#define VOCEKIT_VOICE_LONG_RECORDING_COMPLETION_EXECUTOR_H

#include "voice_long_recording_result_builder.h"

#include <functional>

struct VoiceLongRecordingCompletionRequest
{
    const SegmentedRecordingState *state = nullptr;
    QMap<int, QByteArray> segmentPcm;
    QString audioDirectory;
    QString fileBase;
};

struct VoiceLongRecordingCompletionHandlers
{
    std::function<QString(
        const QByteArray &completePcm,
        const QString &audioDirectory,
        const QString &fileBase,
        QString *error
    )> saveCompleteAudio;
};

struct VoiceLongRecordingCompletionResult
{
    bool ok = false;
    VoiceLongRecordingBuildResult build;
    QString audioPath;
    QString audioSaveError;
    QString error;
};

// 长录音完成执行器：合并分段文本与音频，并统一判断整体识别结果。
class VoiceLongRecordingCompletionExecutor
{
public:
    static VoiceLongRecordingCompletionResult run(
        const VoiceLongRecordingCompletionRequest &request,
        const VoiceLongRecordingCompletionHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_LONG_RECORDING_COMPLETION_EXECUTOR_H
