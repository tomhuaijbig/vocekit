#include "history_segment_retry_task.h"

#include "../recording/segmented_recording.h"
#include "speech_recognition_task.h"

#include <QFile>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString historySegmentRetryConfigurationError(
    const QString &speechProvider
)
{
    return speechRecognitionProviderConfigurationError(speechProvider);
}

HistorySegmentRetryTaskPreflight prepareHistorySegmentRetryTask(
    const HistorySegmentRetryTaskRequest &request
)
{
    HistorySegmentRetryTaskPreflight preflight;

    const QString configurationError =
        historySegmentRetryConfigurationError(request.speechProvider);
    if (!configurationError.isEmpty()) {
        preflight.error = configurationError;
        return preflight;
    }

    if (!request.pcm.isEmpty()) {
        preflight.ok = true;
        preflight.pcm = request.pcm;
        return preflight;
    }

    QFile wavFile(request.segment.wavPath);
    if (!wavFile.open(QIODevice::ReadOnly)) {
        preflight.error = tr8("无法读取这一段录音文件。");
        return preflight;
    }

    QString wavError;
    preflight.pcm = pcm16FromWavData(wavFile.readAll(), &wavError);
    if (preflight.pcm.isEmpty()) {
        preflight.error = wavError;
        return preflight;
    }

    preflight.ok = true;
    return preflight;
}

HistorySegmentRetryResult runHistorySegmentRetryTask(
    const HistorySegmentRetryTaskRequest &request
)
{
    HistorySegmentRetryResult result;
    result.index = request.segment.index;

    HistorySegmentRetryTaskPreflight preflight =
        prepareHistorySegmentRetryTask(request);
    if (!preflight.ok) {
        result.error = preflight.error;
        return result;
    }

    SpeechRecognitionProviderTaskRequest speechRequest;
    speechRequest.index = request.segment.index;
    speechRequest.audioData = preflight.pcm;
    speechRequest.audioFormat = QStringLiteral("pcm");
    speechRequest.sampleRate = 16000;
    speechRequest.provider = request.speechProvider;
    speechRequest.networkPolicy = request.networkPolicy;
    speechRequest.useSystemProxy = request.useSystemProxy;

    const SpeechRecognitionTaskResult speechResult =
        runSpeechRecognitionProviderTask(speechRequest);
    result.index = speechResult.index;
    result.text = speechResult.text;
    result.error = speechResult.error;
    result.elapsedMs = speechResult.elapsedMs;
    return result;
}
