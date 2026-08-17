#ifndef VOCEKIT_STREAMING_SPEECH_SESSION_H
#define VOCEKIT_STREAMING_SPEECH_SESSION_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <functional>

enum class StreamingSpeechState
{
    Idle,
    Connecting,
    Streaming,
    Finalizing,
    Completed,
    Degraded,
    Cancelled
};

struct StreamingTranscriptSnapshot
{
    quint64 revision = 0;
    QString committedText;
    QString provisionalText;

    QString displayText() const
    {
        return committedText + provisionalText;
    }
};

struct StreamingSpeechSessionRequest
{
    QString provider;
    QString language = QStringLiteral("follow-windows");
    QString runId;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
    int sampleRate = 16000;
    int channelCount = 1;
    int sampleSizeBits = 16;
};

struct StreamingSpeechCallbacks
{
    std::function<void(const StreamingTranscriptSnapshot &)>
        transcriptUpdated;
    std::function<void(const QString &)> degraded;
    std::function<void(const QString &)> completed;
    std::function<void(const QString &, const QString &)>
        configurationFailed;
};

class IStreamingSpeechSession
{
public:
    virtual ~IStreamingSpeechSession()
    {
    }

    virtual bool start(QString *error) = 0;
    virtual bool pushAudio(const QByteArray &pcm) = 0;
    virtual void finish() = 0;
    virtual void cancel() = 0;
    virtual StreamingSpeechState state() const = 0;
};

#endif // VOCEKIT_STREAMING_SPEECH_SESSION_H
