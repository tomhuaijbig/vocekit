#include "voice_recording_completion_executor.h"

namespace {

QString missingStopHandlerError()
{
    return QString::fromUtf8("录音停止执行器未配置。");
}

} // namespace

VoiceRecordingCompletionResult VoiceRecordingCompletionExecutor::run(
    const VoiceRecordingCompletionRequest &request,
    const VoiceRecordingCompletionHandlers &handlers
)
{
    VoiceRecordingCompletionResult result;
    if (!handlers.stopRecording) {
        result.error = missingStopHandlerError();
        return result;
    }

    result.recording = handlers.stopRecording();
    VoiceSpeechRecognitionRequest speechRequest;
    speechRequest.modeId = request.modeId;
    speechRequest.audioData = result.recording.pcm;
    speechRequest.provider = request.provider;
    speechRequest.networkPolicy = request.networkPolicy;
    speechRequest.useSystemProxy = request.useSystemProxy;
    result.speech = VoiceSpeechRecognitionExecutor::run(
        speechRequest,
        handlers.recognition
    );
    result.ok = result.speech.ok;
    result.error = result.speech.error;
    return result;
}
