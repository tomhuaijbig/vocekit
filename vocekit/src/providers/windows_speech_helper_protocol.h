#ifndef VOCEKIT_WINDOWS_SPEECH_HELPER_PROTOCOL_H
#define VOCEKIT_WINDOWS_SPEECH_HELPER_PROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QStringList>

enum class WindowsSpeechHelperEventType
{
    Invalid,
    Ready,
    Hypothesis,
    Recognized,
    Final,
    Probe,
    Error,
    SelfTest
};

struct WindowsSpeechHelperEvent
{
    bool valid = false;
    WindowsSpeechHelperEventType type =
        WindowsSpeechHelperEventType::Invalid;
    QString runId;
    QString text;
    QString errorCode;
    QString errorMessage;
    QString resolvedLanguage;
    QStringList installedLanguages;
    bool inputStreamEnded = false;
    qint64 pcmBytesObserved = -1;
};

QString windowsSpeechHelperPathForApplicationDir(
    const QString &applicationDir
);

QStringList windowsSpeechHelperArguments(
    const QString &mode,
    const QString &runId,
    const QString &language,
    int sampleRate,
    int channelCount,
    int bits
);

WindowsSpeechHelperEvent parseWindowsSpeechHelperEvent(
    const QByteArray &line
);

QString windowsSpeechOperationErrorCode(const QString &helperErrorCode);

bool isWindowsSpeechConfigurationErrorCode(
    const QString &operationErrorCode
);

QString appendWindowsSpeechRecognizedSegment(
    const QString &committed,
    const QString &segment
);

#endif // VOCEKIT_WINDOWS_SPEECH_HELPER_PROTOCOL_H
