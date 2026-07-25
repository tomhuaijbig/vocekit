#ifndef VOCEKIT_BAIDU_SPEECH_PROVIDER_H
#define VOCEKIT_BAIDU_SPEECH_PROVIDER_H

#include "provider_network_transport.h"
#include "speech_provider.h"

#include "../config/secret_config.h"

#include <QDateTime>
#include <QSharedPointer>

#include <functional>

// 百度语音 Provider 独立负责令牌缓存、短语音识别和结构化错误转换。
class BaiduSpeechProvider : public ISpeechProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;

    explicit BaiduSpeechProvider(bool useSystemProxy = false);
    BaiduSpeechProvider(
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
    struct AccessTokenResult
    {
        QString token;
        QByteArray rawResponse;
        OperationError error;
        qint64 durationMs = -1;
    };

    AccessTokenResult accessToken(
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    ) const;

    QSharedPointer<IProviderNetworkTransport> m_transport;
    SecretLoader m_secretLoader;
    SecretConfig m_secrets;
    bool m_useSystemProxy = false;
    mutable QString m_accessToken;
    mutable QDateTime m_tokenExpiry;
};

QSharedPointer<ISpeechProvider> createBaiduSpeechProvider(
    bool useSystemProxy = false
);
QSharedPointer<ISpeechProvider> createBaiduSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy = false
);

#endif // VOCEKIT_BAIDU_SPEECH_PROVIDER_H
