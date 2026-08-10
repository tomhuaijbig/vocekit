#include "streaming_speech_session_factory.h"

#include "baidu_streaming_speech_session.h"
#include "provider_streaming_websocket_transport.h"
#include "xfyun_streaming_speech_session.h"

#include <QDateTime>

StreamingSpeechSessionCreation createDefaultStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks
)
{
    StreamingSpeechSessionFactoryDependencies dependencies;
    dependencies.loadSecrets = []() { return loadSecrets(); };
    dependencies.createXfyun = [](
        const SecretConfig &secrets,
        const StreamingSpeechSessionRequest &sessionRequest,
        const StreamingSpeechCallbacks &sessionCallbacks
    ) {
        return QSharedPointer<IStreamingSpeechSession>(
            new XfyunStreamingSpeechSession(
                []() { return createProviderStreamingWebSocketTransport(); },
                [secrets]() { return secrets; },
                []() { return QDateTime::currentDateTimeUtc(); },
                sessionRequest,
                sessionCallbacks
            )
        );
    };
    dependencies.createBaidu = [](
        const SecretConfig &secrets,
        const StreamingSpeechSessionRequest &sessionRequest,
        const StreamingSpeechCallbacks &sessionCallbacks
    ) {
        return QSharedPointer<IStreamingSpeechSession>(
            new BaiduStreamingSpeechSession(
                []() { return createProviderStreamingWebSocketTransport(); },
                [secrets]() { return secrets; },
                BaiduStreamingSpeechSession::SessionIdFactory(),
                sessionRequest,
                sessionCallbacks
            )
        );
    };
    return createStreamingSpeechSession(
        request,
        callbacks,
        dependencies
    );
}
