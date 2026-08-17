#ifndef VOCEKIT_BAIDU_REALTIME_SPEECH_PROTOCOL_H
#define VOCEKIT_BAIDU_REALTIME_SPEECH_PROTOCOL_H

#include "../config/secret_config.h"
#include "streaming_speech_session.h"

#include <QByteArray>
#include <QUrl>

struct BaiduRealtimeRecognitionEvent
{
    bool valid = false;
    QString type;
    QString text;
    int errorNumber = 0;
    QString errorMessage;
    QByteArray raw;
};

QUrl baiduRealtimeSpeechUrl(const QString &sessionId);

QByteArray baiduRealtimeStartFrame(
    const SecretConfig &secrets,
    const StreamingSpeechSessionRequest &request,
    const QString &clientId
);

QByteArray baiduRealtimeControlFrame(const QString &type);

BaiduRealtimeRecognitionEvent parseBaiduRealtimeRecognitionEvent(
    const QByteArray &message
);

#endif // VOCEKIT_BAIDU_REALTIME_SPEECH_PROTOCOL_H
