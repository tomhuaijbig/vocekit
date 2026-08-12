#ifndef VOCEKIT_WINDOWS_SPEECH_PROVIDER_H
#define VOCEKIT_WINDOWS_SPEECH_PROVIDER_H

#include "speech_provider.h"
#include "windows_speech_helper_client.h"

#include <QSharedPointer>

#include <functional>

class WindowsSpeechProvider : public ISpeechProvider
{
public:
    using BatchFunction = std::function<WindowsSpeechHelperResult(
        const WindowsSpeechBatchRequest &request
    )>;
    using ProbeFunction = std::function<WindowsSpeechHelperResult(
        const WindowsSpeechProbeRequest &request
    )>;

    WindowsSpeechProvider();
    WindowsSpeechProvider(
        const BatchFunction &batch,
        const ProbeFunction &probe
    );

    QString id() const override;
    ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation = CancellationToken()
    ) const override;
    SpeechRecognitionResult recognize(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) override;

private:
    BatchFunction m_batch;
    ProbeFunction m_probe;
};

QSharedPointer<ISpeechProvider> createWindowsSpeechProvider();

#endif // VOCEKIT_WINDOWS_SPEECH_PROVIDER_H
