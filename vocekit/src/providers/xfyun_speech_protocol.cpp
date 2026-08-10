#include "xfyun_speech_protocol.h"

#include "../api/api_client_utils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QUrlQuery>

QUrl xfyunSignedIatUrl(
    const SecretConfig &secrets,
    const QDateTime &utcNow
)
{
    const QString host = QStringLiteral("iat-api.xfyun.cn");
    const QString path = QStringLiteral("/v2/iat");
    const QString date = QLocale::c().toString(
        utcNow.toUTC(),
        QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'")
    );
    const QByteArray signatureOrigin = (
        QStringLiteral("host: ") + host
        + QStringLiteral("\ndate: ") + date
        + QStringLiteral("\nGET ") + path
        + QStringLiteral(" HTTP/1.1")
    ).toUtf8();
    const QByteArray signature = hmacSha256(
        secrets.xfyunApiSecret.toUtf8(),
        signatureOrigin
    ).toBase64();
    const QByteArray authorizationOrigin = (
        QStringLiteral("api_key=\"") + secrets.xfyunApiKey
        + QStringLiteral(
            "\", algorithm=\"hmac-sha256\", "
            "headers=\"host date request-line\", signature=\""
        )
        + QString::fromLatin1(signature)
        + QStringLiteral("\"")
    ).toUtf8();

    QUrl url(QStringLiteral("wss://") + host + path);
    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("authorization"),
        QString::fromLatin1(authorizationOrigin.toBase64())
    );
    query.addQueryItem(QStringLiteral("date"), date);
    query.addQueryItem(QStringLiteral("host"), host);
    url.setQuery(query);
    return url;
}

QByteArray xfyunAudioFrame(
    const SecretConfig &secrets,
    const QByteArray &audio,
    int status,
    int sampleRate,
    bool dynamicCorrection
)
{
    QJsonObject root;
    if (status == 0) {
        QJsonObject common;
        common.insert(QStringLiteral("app_id"), secrets.xfyunAppId);
        root.insert(QStringLiteral("common"), common);

        QJsonObject business;
        business.insert(QStringLiteral("language"), QStringLiteral("zh_cn"));
        business.insert(QStringLiteral("domain"), QStringLiteral("iat"));
        business.insert(QStringLiteral("accent"), QStringLiteral("mandarin"));
        business.insert(QStringLiteral("vad_eos"), 10000);
        if (dynamicCorrection) {
            business.insert(QStringLiteral("dwa"), QStringLiteral("wpgs"));
        }
        root.insert(QStringLiteral("business"), business);
    }

    QJsonObject data;
    data.insert(QStringLiteral("status"), status);
    data.insert(
        QStringLiteral("format"),
        QStringLiteral("audio/L16;rate=%1").arg(
            sampleRate > 0 ? sampleRate : 16000
        )
    );
    data.insert(QStringLiteral("encoding"), QStringLiteral("raw"));
    data.insert(
        QStringLiteral("audio"),
        QString::fromLatin1(audio.toBase64())
    );
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

XfyunRecognitionEvent parseXfyunRecognitionEvent(
    const QByteArray &message
)
{
    XfyunRecognitionEvent event;
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
    event.valid = true;
    event.code = root.value(QStringLiteral("code")).toInt(-1);
    event.message = root.value(QStringLiteral("message")).toString();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    event.dataStatus = data.value(QStringLiteral("status")).toInt(-1);
    const QJsonObject result = data.value(QStringLiteral("result")).toObject();
    event.sequence = result.value(QStringLiteral("sn")).toInt(-1);
    event.pgs = result.value(QStringLiteral("pgs")).toString();
    const QJsonArray range = result.value(QStringLiteral("rg")).toArray();
    if (range.size() == 2) {
        event.rangeStart = range.at(0).toInt(-1);
        event.rangeEnd = range.at(1).toInt(-1);
    }

    const QJsonArray words = result.value(QStringLiteral("ws")).toArray();
    for (const QJsonValue &wordValue : words) {
        const QJsonArray candidates = wordValue.toObject()
            .value(QStringLiteral("cw")).toArray();
        if (!candidates.isEmpty()) {
            event.text += candidates.first().toObject()
                .value(QStringLiteral("w")).toString();
        }
    }
    return event;
}
