#include "voice_long_recording_completion_executor.h"

namespace {

QString missingStateError()
{
    return QString::fromUtf8("长录音识别状态未配置。");
}

} // namespace

VoiceLongRecordingCompletionResult
VoiceLongRecordingCompletionExecutor::run(
    const VoiceLongRecordingCompletionRequest &request,
    const VoiceLongRecordingCompletionHandlers &handlers
)
{
    VoiceLongRecordingCompletionResult result;
    if (!request.state) {
        result.error = missingStateError();
        return result;
    }

    result.build = VoiceLongRecordingResultBuilder::build(
        *request.state,
        request.segmentPcm
    );
    if (!result.build.completePcm.isEmpty()
        && handlers.saveCompleteAudio) {
        result.audioPath = handlers.saveCompleteAudio(
            result.build.completePcm,
            request.audioDirectory,
            request.fileBase,
            &result.audioSaveError
        );
    }

    if (result.build.successfulSegmentCount <= 0) {
        result.error = result.build.noSuccessfulSegmentError;
        return result;
    }
    result.ok = true;
    return result;
}
