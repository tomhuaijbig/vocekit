#ifndef VOCEKIT_WINDOWS_STREAMING_SPEECH_SESSION_H
#define VOCEKIT_WINDOWS_STREAMING_SPEECH_SESSION_H

#include "streaming_speech_session.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

class WindowsStreamingSpeechSession
    : public QObject,
      public IStreamingSpeechSession
{
public:
    struct Timing
    {
        Timing()
            : startupTimeoutMs(5000),
              finalTimeoutMs(8000),
              killTimeoutMs(250),
              writeChunkBytes(32 * 1024),
              queueLimitBytes(64000)
        {
        }

        int startupTimeoutMs;
        int finalTimeoutMs;
        int killTimeoutMs;
        int writeChunkBytes;
        int queueLimitBytes;
    };

    WindowsStreamingSpeechSession(
        const QString &programPath,
        const QStringList &prependedArguments,
        const StreamingSpeechSessionRequest &request,
        const StreamingSpeechCallbacks &callbacks,
        const Timing &timing = Timing(),
        QObject *parent = nullptr
    );
    ~WindowsStreamingSpeechSession() override;

    bool start(QString *error) override;
    bool pushAudio(const QByteArray &pcm) override;
    void finish() override;
    void cancel() override;
    StreamingSpeechState state() const override;

private:
    void onStarted();
    void onReadyReadStandardOutput();
    void onBytesWritten(qint64 bytes);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void consumeOutput();
    void consumeLine(const QByteArray &line);
    void pumpAudio();
    void closeInputIfDrained();
    void emitSnapshot();
    void completeOnce(const QString &text);
    void fail(const QString &helperCode, const QString &message);
    void cleanup(bool cancelled);
    bool isTerminal() const;

    QString m_programPath;
    QStringList m_prependedArguments;
    StreamingSpeechSessionRequest m_request;
    StreamingSpeechCallbacks m_callbacks;
    Timing m_timing;
    QProcess m_process;
    QTimer m_startupTimer;
    QTimer m_finalTimer;
    QByteArray m_stdoutBuffer;
    qint64 m_stdoutBytes = 0;
    QByteArray m_audioQueue;
    qint64 m_pendingWriteBytes = 0;
    QString m_committedText;
    QString m_provisionalText;
    quint64 m_revision = 0;
    StreamingSpeechState m_state = StreamingSpeechState::Idle;
    bool m_ready = false;
    bool m_finishRequested = false;
    bool m_inputClosed = false;
    bool m_terminalNotified = false;
    bool m_cleaningUp = false;
};

#endif // VOCEKIT_WINDOWS_STREAMING_SPEECH_SESSION_H
