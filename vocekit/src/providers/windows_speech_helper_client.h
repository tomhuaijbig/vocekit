#ifndef VOCEKIT_WINDOWS_SPEECH_HELPER_CLIENT_H
#define VOCEKIT_WINDOWS_SPEECH_HELPER_CLIENT_H

#include "../tasks/cancellation_token.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

struct WindowsSpeechBatchRequest
{
    QString runId;
    QString language;
    QByteArray pcm;
    int timeoutMs = 0;
    CancellationToken cancellation;
};

struct WindowsSpeechProbeRequest
{
    QString runId;
    QString language;
    int timeoutMs = 5000;
    CancellationToken cancellation;
};

struct WindowsSpeechHelperResult
{
    bool ok = false;
    QString text;
    QString errorCode;
    QString errorMessage;
    QString resolvedLanguage;
    QStringList installedLanguages;
    qint64 pcmBytesObserved = -1;
    qint64 maximumBytesQueued = 0;
};

int windowsSpeechBatchTimeoutMs(qint64 pcmByteCount);

class WindowsSpeechHelperClient
{
public:
    explicit WindowsSpeechHelperClient(
        const QString &programPath,
        const QStringList &prependedArguments = QStringList()
    );

    WindowsSpeechHelperResult recognize(
        const WindowsSpeechBatchRequest &request
    ) const;

    WindowsSpeechHelperResult probe(
        const WindowsSpeechProbeRequest &request
    ) const;

private:
    QString m_programPath;
    QStringList m_prependedArguments;
};

#endif // VOCEKIT_WINDOWS_SPEECH_HELPER_CLIENT_H
