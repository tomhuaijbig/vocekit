#ifndef VOCEKIT_STREAMING_SPEECH_SESSION_FACTORY_H
#define VOCEKIT_STREAMING_SPEECH_SESSION_FACTORY_H

#include "streaming_speech_session.h"
#include "../config/secret_config.h"

#include <QSharedPointer>

#include <functional>

struct StreamingSpeechSessionCreation
{
    QSharedPointer<IStreamingSpeechSession> session;
    QString unavailableReason;
};

struct StreamingSpeechSessionFactoryDependencies
{
    using ProviderFactory = std::function<
        QSharedPointer<IStreamingSpeechSession>(
            const SecretConfig &,
            const StreamingSpeechSessionRequest &,
            const StreamingSpeechCallbacks &
        )
    >;
    using LocalProviderFactory = std::function<
        QSharedPointer<IStreamingSpeechSession>(
            const StreamingSpeechSessionRequest &,
            const StreamingSpeechCallbacks &
        )
    >;

    std::function<SecretConfig()> loadSecrets;
    ProviderFactory createXfyun;
    ProviderFactory createBaidu;
    LocalProviderFactory createWindows;
};

StreamingSpeechSessionCreation createStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const StreamingSpeechSessionFactoryDependencies &dependencies
);

StreamingSpeechSessionCreation createDefaultStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks
);

#endif // VOCEKIT_STREAMING_SPEECH_SESSION_FACTORY_H
