#include "voice_history_request_builder.h"

HistoryRecordSaveRequest VoiceHistoryRequestBuilder::build(
    const VoiceHistoryBuildRequest &request
)
{
    VoiceRunContext historyContext = request.runContext;
    if (historyContext.modeId != request.modeId) {
        historyContext.screenshotInput = false;
    }

    HistoryRecordSaveRequest saveRequest;
    saveRequest.recordDirectory = request.recordDirectory;
    saveRequest.modeId = request.modeId;
    saveRequest.modeTitle = request.modeTitle;
    saveRequest.sourceAudioPath = request.sourceAudioPath;
    saveRequest.draft = request.draft;
    saveRequest.metadata.input = request.input;
    saveRequest.metadata.output = request.output;
    saveRequest.metadata.error = request.error;
    saveRequest.metadata.model =
        request.usedModel ? request.model : QString();
    saveRequest.metadata.elapsedMs = request.elapsedMs;
    saveRequest.metadata.speechElapsedMs = request.speechElapsedMs;
    saveRequest.metadata.modelElapsedMs = request.modelElapsedMs;
    saveRequest.metadata.promptVersion =
        request.usedModel ? request.promptVersion : QString();
    saveRequest.metadata.runContext = historyContext;
    saveRequest.metadata.actionHadRecording = request.actionHadRecording;
    saveRequest.metadata.recordingTriggerMode =
        request.recordingTriggerMode;
    saveRequest.metadata.longRecording = request.longRecording;
    saveRequest.metadata.recordingSegments = request.recordingSegments;

    return saveRequest;
}
