#ifndef VOCEKIT_XFYUN_SPEECH_PROTOCOL_H
#define VOCEKIT_XFYUN_SPEECH_PROTOCOL_H

#include "../config/secret_config.h"

#include <QByteArray>
#include <QDateTime>
#include <QUrl>

struct XfyunRecognitionEvent
{
    bool valid = false;
    int code = -1;
    QString message;
    int dataStatus = -1;
    int sequence = -1;
    QString pgs;
    int rangeStart = -1;
    int rangeEnd = -1;
    QString text;
    QByteArray raw;

    bool isFinal() const
    {
        return code != 0 || dataStatus == 2;
    }
};

QUrl xfyunSignedIatUrl(
    const SecretConfig &secrets,
    const QDateTime &utcNow
);

QByteArray xfyunAudioFrame(
    const SecretConfig &secrets,
    const QByteArray &audio,
    int status,
    int sampleRate,
    bool dynamicCorrection
);

XfyunRecognitionEvent parseXfyunRecognitionEvent(
    const QByteArray &message
);

#endif // VOCEKIT_XFYUN_SPEECH_PROTOCOL_H
