#ifndef VOCEKIT_CUSTOM_SPEECH_PROVIDER_H
#define VOCEKIT_CUSTOM_SPEECH_PROVIDER_H

#include "provider_network_transport.h"
#include "speech_provider.h"

#include "../config/secret_config.h"

#include <QSharedPointer>

#include <functional>

// 自定义语音 Provider 独立负责 JSON 音频请求、响应解析、自检和错误转换。
class CustomSpeechProvider : public ISpeechProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;

    explicit CustomSpeechProvider(bool useSystemProxy = false);
    CustomSpeechProvider(
        const QSharedPointer<IProviderNetworkTransport> &transport,
        const SecretLoader &secretLoader,
        bool useSystemProxy = false
    );

    QString id() const override;
    ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation = CancellationToken()
    ) const override;
    SpeechRecognitionResult recognize(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) override;
    void refreshConfiguration() override;

private:
    SpeechRecognitionResult execute(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) const;

    QSharedPointer<IProviderNetworkTransport> m_transport;
    SecretLoader m_secretLoader;
    SecretConfig m_secrets;
    bool m_useSystemProxy = false;
};

QSharedPointer<ISpeechProvider> createCustomSpeechProvider(
    bool useSystemProxy = false
);
QSharedPointer<ISpeechProvider> createCustomSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy = false
);

#endif // VOCEKIT_CUSTOM_SPEECH_PROVIDER_H
