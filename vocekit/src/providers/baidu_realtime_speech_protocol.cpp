#include "baidu_realtime_speech_protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

QUrl baiduRealtimeSpeechUrl(const QString &sessionId)
{
    QUrl url(QStringLiteral("wss://vop.baidu.com/realtime_asr"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("sn"), sessionId);
    url.setQuery(query);
    return url;
}

QByteArray baiduRealtimeStartFrame(
    const SecretConfig &secrets,
    const StreamingSpeechSessionRequest &request,
    const QString &clientId
)
{
    bool appIdOk = false;
    const qlonglong appId = secrets.baiduAppId.trimmed().toLongLong(&appIdOk);

    QJsonObject data;
    data.insert(
        QStringLiteral("appid"),
        appIdOk ? static_cast<double>(appId) : 0.0
    );
    data.insert(QStringLiteral("appkey"), secrets.baiduApiKey.trimmed());
    data.insert(QStringLiteral("dev_pid"), 15372);
    data.insert(QStringLiteral("cuid"), clientId);
    data.insert(QStringLiteral("format"), QStringLiteral("pcm"));
    data.insert(
        QStringLiteral("sample"),
        request.sampleRate > 0 ? request.sampleRate : 16000
    );

    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("START"));
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray baiduRealtimeControlFrame(const QString &type)
{
    QJsonObject root;
    root.insert(QStringLiteral("type"), type);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

BaiduRealtimeRecognitionEvent parseBaiduRealtimeRecognitionEvent(
    const QByteArray &message
)
{
    BaiduRealtimeRecognitionEvent event;
    event.raw = message;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        message,
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return event;
    }

    const QJsonObject root = document.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    event.valid = true;
    event.type = root.value(QStringLiteral("type")).toString(
        data.value(QStringLiteral("type")).toString()
    );
    event.errorNumber = root.value(QStringLiteral("err_no")).toInt(
        data.value(QStringLiteral("err_no")).toInt(0)
    );
    event.errorMessage = root.value(QStringLiteral("err_msg")).toString(
        data.value(QStringLiteral("err_msg")).toString()
    );

    QJsonValue result = root.value(QStringLiteral("result"));
    if (result.isUndefined()) {
        result = data.value(QStringLiteral("result"));
    }
    if (result.isString()) {
        event.text = result.toString();
    } else if (result.isArray() && !result.toArray().isEmpty()) {
        event.text = result.toArray().first().toString();
    }
    return event;
}
