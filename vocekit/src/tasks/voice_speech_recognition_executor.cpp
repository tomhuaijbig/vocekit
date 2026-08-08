#include "voice_speech_recognition_executor.h"

namespace {

QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

QString defaultNoSpeechError()
{
    return textUtf8(
        "\xE6\xB2\xA1\xE6\x9C\x89\xE8\xAF\x86\xE5\x88\xAB\xE5\x88\xB0\xE8\xAF\xAD\xE9\x9F\xB3\xE3\x80\x82"
    );
}

QString missingHandlerError()
{
    return textUtf8(
        "\xE8\xAF\xAD\xE9\x9F\xB3\xE8\xAF\x86\xE5\x88\xAB\xE6\x89\xA7\xE8\xA1\x8C\xE5\x99\xA8\xE6\x9C\xAA\xE9\x85\x8D\xE7\xBD\xAE\xE3\x80\x82"
    );
}

QString cancelledError()
{
    return textUtf8(
        "\xE8\xAF\xAD\xE9\x9F\xB3\xE8\xAF\x86\xE5\x88\xAB\xE5\xB7\xB2\xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"
    );
}

QString failureLogDetail(const QString &modeId, const QString &error)
{
    return textUtf8("\xE9\x98\xB6\xE6\xAE\xB5=\xE8\xAF\xAD\xE9\x9F\xB3\xE8\xAF\x86\xE5\x88\xAB\xEF\xBC\x8C\xE5\x8A\x9F\xE8\x83\xBD=")
        + modeId
        + textUtf8("\xEF\xBC\x8C\xE9\x94\x99\xE8\xAF\xAF=")
        + error;
}

QString successLogDetail(const QString &modeId, const QString &text)
{
    return textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=")
        + modeId
        + textUtf8("\xEF\xBC\x8C\xE5\xAD\x97\xE6\x95\xB0=")
        + QString::number(text.size());
}

} // namespace

VoiceSpeechRecognitionResult VoiceSpeechRecognitionExecutor::run(
    const VoiceSpeechRecognitionRequest &request,
    const VoiceSpeechRecognitionHandlers &handlers
)
{
    VoiceSpeechRecognitionResult result;
    result.index = request.index;

    if (request.cancellation.isCancellationRequested()) {
        result.cancelled = true;
        result.error = cancelledError();
        result.logCategory = textUtf8("\xE5\x8A\x9F\xE8\x83\xBD");
        result.logAction = textUtf8("\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail = failureLogDetail(request.modeId, result.error);
        return result;
    }

    if (!handlers.recognizeProvider) {
        result.error = missingHandlerError();
        result.logCategory = textUtf8("\xE5\x8A\x9F\xE8\x83\xBD");
        result.logAction = textUtf8("\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail = failureLogDetail(request.modeId, result.error);
        return result;
    }

    SpeechRecognitionProviderTaskRequest speechRequest;
    speechRequest.index = request.index;
    speechRequest.audioData = request.audioData;
    speechRequest.audioFormat = request.audioFormat;
    speechRequest.sampleRate = request.sampleRate;
    speechRequest.provider = request.provider;
    speechRequest.useSystemProxy = request.useSystemProxy;
    speechRequest.networkPolicy = request.networkPolicy;
    speechRequest.cancellation = request.cancellation;

    const SpeechRecognitionTaskResult speechResult =
        handlers.recognizeProvider(speechRequest);

    result.text = speechResult.text;
    result.error = speechResult.error;
    result.elapsedMs = speechResult.elapsedMs;

    if (request.cancellation.isCancellationRequested()) {
        result.cancelled = true;
        result.error = cancelledError();
        result.logCategory = textUtf8("\xE5\x8A\x9F\xE8\x83\xBD");
        result.logAction = textUtf8("\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail = failureLogDetail(request.modeId, result.error);
        return result;
    }

    if (result.text.trimmed().isEmpty()) {
        if (result.error.trimmed().isEmpty()) {
            result.emptyRecognition = true;
            result.error = defaultNoSpeechError();
        }
        result.logCategory = textUtf8("\xE5\x8A\x9F\xE8\x83\xBD");
        result.logAction = textUtf8("\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail = failureLogDetail(request.modeId, result.error);
        return result;
    }

    result.ok = true;
    result.logCategory = textUtf8("\xE8\xAF\xAD\xE9\x9F\xB3\xE8\xAF\x86\xE5\x88\xAB");
    result.logAction = textUtf8("\xE5\xBE\x97\xE5\x88\xB0\xE6\x96\x87\xE6\x9C\xAC");
    result.logDetail = successLogDetail(request.modeId, result.text);
    return result;
}
