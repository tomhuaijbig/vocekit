#ifndef VOCEKIT_XFYUN_SPEECH_PROVIDER_H
#define VOCEKIT_XFYUN_SPEECH_PROVIDER_H

#include "provider_websocket_transport.h"
#include "speech_provider.h"

#include "../config/secret_config.h"

#include <QDateTime>
#include <QSharedPointer>

#include <functional>

// 讯飞语音 Provider 独立负责鉴权、音频分片、响应解析和结构化错误转换。
class XfyunSpeechProvider : public ISpeechProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;
    using UtcNow = std::function<QDateTime()>;

    explicit XfyunSpeechProvider(bool useSystemProxy = false);
    XfyunSpeechProvider(
        const QSharedPointer<IProviderWebSocketTransport> &transport,
        const SecretLoader &secretLoader,
        const UtcNow &utcNow,
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
    SpeechRecognitionResult recognizeRequest(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) const;

    QSharedPointer<IProviderWebSocketTransport> m_transport;
    SecretLoader m_secretLoader;
    UtcNow m_utcNow;
    SecretConfig m_secrets;
    bool m_useSystemProxy = false;
};

QSharedPointer<ISpeechProvider> createXfyunSpeechProvider(
    bool useSystemProxy = false
);
QSharedPointer<ISpeechProvider> createXfyunSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy = false
);

#endif // VOCEKIT_XFYUN_SPEECH_PROVIDER_H
