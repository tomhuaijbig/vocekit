#ifndef VOCEKIT_BAIDU_STREAMING_SPEECH_SESSION_H
#define VOCEKIT_BAIDU_STREAMING_SPEECH_SESSION_H

#include "provider_streaming_websocket_transport.h"
#include "streaming_speech_session.h"
#include "streaming_transcript_accumulator.h"
#include "../config/secret_config.h"

#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QTimer>

#include <functional>

class BaiduStreamingSpeechSession
    : public QObject,
      public IStreamingSpeechSession
{
public:
    struct Timing
    {
        Timing()
            : frameIntervalMs(160),
              queueLimitBytes(64000)
        {
        }

        int frameIntervalMs;
        int queueLimitBytes;
    };

    using TransportFactory = std::function<
        QSharedPointer<IProviderStreamingWebSocketTransport>()
    >;
    using SecretLoader = std::function<SecretConfig()>;
    using SessionIdFactory = std::function<QString()>;

    BaiduStreamingSpeechSession(
        const TransportFactory &transportFactory,
        const SecretLoader &secretLoader,
        const SessionIdFactory &sessionIdFactory,
        const StreamingSpeechSessionRequest &request,
        const StreamingSpeechCallbacks &callbacks,
        const Timing &timing = Timing(),
        QObject *parent = nullptr
    );

    bool start(QString *error) override;
    bool pushAudio(const QByteArray &pcm) override;
    void finish() override;
    void cancel() override;
    StreamingSpeechState state() const override;

private:
    void onOpened(int generation);
    void onTextMessage(int generation, const QByteArray &message);
    void onTransportFailed(int generation, const OperationError &error);
    void onTransportClosed(int generation, bool cancelled);
    void pumpAudioFrame();
    void sendFinishFrame();
    void completeSession();
    void emitSnapshot();
    void degradeOnce(const QString &message);
    bool isTerminal() const;

    TransportFactory m_transportFactory;
    SecretLoader m_secretLoader;
    SessionIdFactory m_sessionIdFactory;
    StreamingSpeechSessionRequest m_request;
    StreamingSpeechCallbacks m_callbacks;
    Timing m_timing;
    SecretConfig m_secrets;
    QString m_sessionId;
    QSharedPointer<IProviderStreamingWebSocketTransport> m_transport;
    StreamingTranscriptAccumulator m_transcript;
    QTimer m_frameTimer;
    mutable QMutex m_audioMutex;
    QByteArray m_audioQueue;
    StreamingSpeechState m_state = StreamingSpeechState::Idle;
    int m_generation = 0;
    bool m_sentFinish = false;
    bool m_degradedNotified = false;
};

#endif // VOCEKIT_BAIDU_STREAMING_SPEECH_SESSION_H
