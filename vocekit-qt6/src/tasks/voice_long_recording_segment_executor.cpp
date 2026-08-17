#include "voice_long_recording_segment_executor.h"

namespace {

const int kMaximumRecognitionAttempts = 2;

QString cancelledError()
{
    return QString::fromUtf8(
        "\xE9\x95\xBF\xE5\xBD\x95\xE9\x9F\xB3\xE5\x88\x86\xE6\xAE\xB5\xE8\xAF\x86\xE5\x88\xAB\xE5\xB7\xB2\xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"
    );
}

} // namespace

VoiceLongRecordingSegmentResult VoiceLongRecordingSegmentExecutor::run(
    const VoiceLongRecordingSegmentRequest &request,
    const VoiceSpeechRecognitionHandlers &handlers
)
{
    VoiceLongRecordingSegmentResult result;
    result.index = request.speech.index;
    result.executionId = request.cancellation.executionId();

    while (result.attempts < kMaximumRecognitionAttempts) {
        if (request.cancellation.isCancellationRequested()) {
            result.cancelled = true;
            result.error = cancelledError();
            return result;
        }

        const VoiceSpeechRecognitionResult attempt =
            VoiceSpeechRecognitionExecutor::run(request.speech, handlers);
        result.attemptResults.append(attempt);
        ++result.attempts;
        if (attempt.elapsedMs >= 0) {
            result.elapsedMs += attempt.elapsedMs;
        }

        result.text = attempt.text;
        result.error = attempt.error;
        result.errorCode = attempt.errorCode;
        if (attempt.ok) {
            result.ok = true;
            result.error.clear();
            result.errorCode.clear();
            return result;
        }
    }

    return result;
}

void VoiceLongRecordingSegmentExecutor::apply(
    const VoiceLongRecordingSegmentResult &result,
    SegmentedRecordingState *state
)
{
    if (!state || result.cancelled || result.index <= 0) {
        return;
    }

    for (int attempt = 0; attempt < result.attempts; ++attempt) {
        state->markAttemptStarted(result.index);
    }
    state->recordResult(
        result.index,
        result.text,
        result.error,
        result.elapsedMs
    );
}
