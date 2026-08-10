#ifndef VOCEKIT_XFYUN_STREAMING_SPEECH_SESSION_H
#define VOCEKIT_XFYUN_STREAMING_SPEECH_SESSION_H

#include "provider_streaming_websocket_transport.h"
#include "streaming_speech_session.h"
#include "streaming_transcript_accumulator.h"
#include "../config/secret_config.h"

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QTimer>

#include <functional>

class XfyunStreamingSpeechSession
    : public QObject,
      public IStreamingSpeechSession
{
public:
    struct Timing
    {
        Timing()
            : frameIntervalMs(40),
              rotationIntervalMs(55000),
              queueLimitBytes(64000)
        {
        }

        int frameIntervalMs;
        int rotationIntervalMs;
        int queueLimitBytes;
    };

    using TransportFactory = std::function<
        QSharedPointer<IProviderStreamingWebSocketTransport>()
    >;
    using SecretLoader = std::function<SecretConfig()>;
    using UtcNow = std::function<QDateTime()>;

    XfyunStreamingSpeechSession(
        const TransportFactory &transportFactory,
        const SecretLoader &secretLoader,
        const UtcNow &utcNow,
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
    bool openConnection(QString *error = nullptr);
    void onOpened(int generation);
    void onTextMessage(int generation, const QByteArray &message);
    void onTransportFailed(int generation, const OperationError &error);
    void onTransportClosed(int generation, bool cancelled);
    void pumpAudioFrame();
    void beginRotation();
    void sendFinalFrame();
    void completeRotation();
    void completeSession();
    void emitSnapshot();
    void degradeOnce(const QString &message);
    bool isTerminal() const;

    TransportFactory m_transportFactory;
    SecretLoader m_secretLoader;
    UtcNow m_utcNow;
    StreamingSpeechSessionRequest m_request;
    StreamingSpeechCallbacks m_callbacks;
    Timing m_timing;
    SecretConfig m_secrets;
    QSharedPointer<IProviderStreamingWebSocketTransport> m_transport;
    StreamingTranscriptAccumulator m_transcript;
    QTimer m_frameTimer;
    QTimer m_rotationTimer;
    mutable QMutex m_audioMutex;
    QByteArray m_audioQueue;
    StreamingSpeechState m_state = StreamingSpeechState::Idle;
    int m_generation = 0;
    bool m_sentFirstFrame = false;
    bool m_sentFinalFrame = false;
    bool m_rotating = false;
    bool m_degradedNotified = false;
};

#endif // VOCEKIT_XFYUN_STREAMING_SPEECH_SESSION_H
