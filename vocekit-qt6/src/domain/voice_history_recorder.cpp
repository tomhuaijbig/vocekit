#include "voice_history_recorder.h"

#include "voice_history_request_builder.h"

#include "../storage/history_record_service.h"

VoiceHistorySaveResult VoiceHistoryRecorder::save(
    const VoiceHistorySaveRequest &request
)
{
    VoiceHistoryBuildRequest buildRequest;
    buildRequest.recordDirectory = request.recordDirectory;
    buildRequest.modeId = request.modeId;
    buildRequest.modeTitle = request.modeTitle;
    buildRequest.sourceAudioPath = request.sourceAudioPath;
    buildRequest.input = request.input;
    buildRequest.output = request.output;
    buildRequest.error = request.error;
    buildRequest.draft = request.draft;
    buildRequest.model = request.model;
    buildRequest.usedModel = request.usedModel;
    buildRequest.elapsedMs = request.elapsedMs;
    buildRequest.speechElapsedMs = request.speechElapsedMs;
    buildRequest.modelElapsedMs = request.modelElapsedMs;
    buildRequest.promptVersion = request.promptVersion;
    buildRequest.runContext = request.runContext;
    buildRequest.actionHadRecording = request.actionHadRecording;
    buildRequest.recordingTriggerMode = request.recordingTriggerMode;
    buildRequest.longRecording = request.longRecording;
    buildRequest.recordingSegments = request.recordingSegments;

    VoiceHistorySaveResult result;
    result.saved = HistoryRecordService::save(
        VoiceHistoryRequestBuilder::build(buildRequest)
    );
    result.logAction = request.error.trimmed().isEmpty()
        ? QString::fromUtf8("保存")
        : QString::fromUtf8("保存错误记录");
    result.logDetail =
        QString::fromUtf8("功能=") + request.modeId
        + QString::fromUtf8("，草稿=") + (request.draft ? QString::fromUtf8("是") : QString::fromUtf8("否"))
        + QString::fromUtf8("，输入字数=") + QString::number(request.input.size())
        + QString::fromUtf8("，输出字数=") + QString::number(request.output.size())
        + (!request.error.trimmed().isEmpty()
            ? QString::fromUtf8("，错误=") + request.error
            : QString());
    return result;
}
