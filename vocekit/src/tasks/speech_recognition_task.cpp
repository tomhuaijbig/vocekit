#include "speech_recognition_task.h"

#include "../providers/built_in_provider_factory.h"
#include "../providers/provider_configuration.h"
#include "../providers/speech_provider.h"
#include "cancellation_token.h"

#include <QElapsedTimer>
#include <QSharedPointer>

SpeechRecognitionTaskResult runSpeechRecognitionTask(
    const SpeechRecognitionTaskRequest &request
)
{
    SpeechRecognitionTaskResult result;
    result.index = request.index;

    QElapsedTimer timer;
    timer.start();

    if (!request.recognize) {
        result.error = QString::fromUtf8("语音识别任务没有配置执行函数。");
    } else {
        result.text = request.recognize(&result.error);
    }
    result.elapsedMs = timer.elapsed();
    return result;
}

QString speechRecognitionProviderConfigurationError(
    const QString &provider
)
{
    return speechProviderConfigurationErrorFromStore(provider);
}

SpeechRecognitionTaskResult runSpeechRecognitionProviderTask(
    const SpeechRecognitionProviderTaskRequest &request
)
{
    SpeechRecognitionTaskResult result;
    result.index = request.index;

    QElapsedTimer timer;
    timer.start();

    const QSharedPointer<ISpeechProvider> provider =
        createBuiltInSpeechProvider(
            request.provider,
            request.useSystemProxy
        );

    SpeechRecognitionRequest providerRequest;
    providerRequest.audioData = request.audioData;
    providerRequest.audioFormat = request.audioFormat;
    providerRequest.sampleRate = request.sampleRate;
    providerRequest.network.globalUseSystemProxy = request.useSystemProxy;
    providerRequest.network.networkPolicy = request.networkPolicy;

    CancellationSource ownedCancellation;
    const CancellationToken cancellation =
        request.cancellation.isValid()
            ? request.cancellation
            : ownedCancellation.token();
    providerRequest.executionId = cancellation.executionId();

    const SpeechRecognitionResult providerResult =
        provider->recognize(providerRequest, cancellation);

    result.text = providerResult.text;
    result.error = providerResult.error.message;
    result.elapsedMs = providerResult.durationMs >= 0
        ? providerResult.durationMs
        : timer.elapsed();
    return result;
}
