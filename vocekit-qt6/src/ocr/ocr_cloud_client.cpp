#include "ocr_cloud_client.h"
#include "../result_flow_config.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

QString jsonTextValue(const QJsonObject &object)
{
    for (const QString &key : {
             QStringLiteral("text"),
             QStringLiteral("result"),
             QStringLiteral("content")
         }) {
        const QJsonValue value = object.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty()) {
            return value.toString().trimmed();
        }
    }

    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    for (const QString &key : {
             QStringLiteral("text"),
             QStringLiteral("result"),
             QStringLiteral("content")
         }) {
        const QJsonValue value = data.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty()) {
            return value.toString().trimmed();
        }
    }
    return QString();
}

QString mimeTypeForImage(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        return QStringLiteral("image/jpeg");
    }
    if (suffix == QStringLiteral("bmp")) {
        return QStringLiteral("image/bmp");
    }
    if (suffix == QStringLiteral("webp")) {
        return QStringLiteral("image/webp");
    }
    return QStringLiteral("image/png");
}

OcrResult cloudError(
    const QString &code,
    const QString &message,
    qint64 elapsedMs = -1)
{
    OcrResult result;
    result.engine = OcrEngine::CustomCloud;
    result.errorCode = code;
    result.errorMessage = message;
    result.elapsedMs = elapsedMs;
    return result;
}

}

QString extractCloudOcrText(const QByteArray &payload, QString *error)
{
    if (error) {
        error->clear();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("云端 OCR 返回的 JSON 无法解析。");
        }
        return QString();
    }

    const QJsonObject root = document.object();
    const QString text = jsonTextValue(root);
    if (!text.isEmpty()) {
        return text;
    }

    QString message = root.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        message = root.value(QStringLiteral("error")).toString().trimmed();
    }
    if (message.isEmpty() && root.value(QStringLiteral("error")).isObject()) {
        message = root.value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("message")).toString().trimmed();
    }
    if (error) {
        *error = message.isEmpty()
            ? QStringLiteral("云端 OCR 没有返回可用文字。")
            : message;
    }
    return QString();
}

OcrResult OcrCloudClient::recognize(
    const OcrCloudConfig &config,
    const OcrRequest &request,
    const CancellationToken &cancellation) const
{
    QElapsedTimer timer;
    timer.start();

    if (cancellation.isCancellationRequested()) {
        return cloudError(
            QStringLiteral("CANCELLED"),
            QStringLiteral("OCR 识别已取消。"),
            timer.elapsed()
        );
    }

    QString validationError;
    if (!validateOcrImage(request.imagePath, &validationError)) {
        return cloudError(QStringLiteral("INVALID_IMAGE"), validationError, timer.elapsed());
    }

    const QUrl url(config.url.trimmed());
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        return cloudError(
            QStringLiteral("INVALID_ENDPOINT"),
            QStringLiteral("自定义云 OCR 接口地址无效。"),
            timer.elapsed()
        );
    }

    QFile imageFile(request.imagePath);
    if (!imageFile.open(QIODevice::ReadOnly)) {
        return cloudError(
            QStringLiteral("IMAGE_READ_FAILED"),
            QStringLiteral("无法读取待识别图片。"),
            timer.elapsed()
        );
    }
    const QByteArray imageBytes = imageFile.readAll();

    QJsonArray languages;
    for (const QString &language : request.languages) {
        languages.append(language);
    }

    QJsonObject body;
    body.insert(QStringLiteral("image"), QString::fromLatin1(imageBytes.toBase64()));
    body.insert(QStringLiteral("mimeType"), mimeTypeForImage(request.imagePath));
    body.insert(QStringLiteral("languages"), languages);
    if (!config.model.trimmed().isEmpty()) {
        body.insert(QStringLiteral("model"), config.model.trimmed());
    }

    QNetworkAccessManager network;
    const QString networkPolicy = config.networkPolicy.trimmed().isEmpty()
        ? (config.useSystemProxy
            ? QStringLiteral("systemProxy")
            : QStringLiteral("direct"))
        : normalizeNetworkPolicy(config.networkPolicy);
    if (networkPolicy == QStringLiteral("systemProxy")) {
        const QList<QNetworkProxy> proxies =
            QNetworkProxyFactory::systemProxyForQuery(
                QNetworkProxyQuery(url)
            );
        QNetworkProxy selected(QNetworkProxy::NoProxy);
        for (const QNetworkProxy &proxy : proxies) {
            if (proxy.type() != QNetworkProxy::DefaultProxy) {
                selected = proxy;
                break;
            }
        }
        network.setProxy(selected);
    } else {
        network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }

    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    networkRequest.setRawHeader("Accept", "application/json");
    networkRequest.setRawHeader("User-Agent", "vocekit/ocr");
    if (!config.apiKey.trimmed().isEmpty()) {
        networkRequest.setRawHeader(
            "Authorization",
            QByteArrayLiteral("Bearer ") + config.apiKey.trimmed().toUtf8()
        );
    }

    QNetworkReply *reply = network.post(
        networkRequest,
        QJsonDocument(body).toJson(QJsonDocument::Compact)
    );

    QEventLoop loop;
    QTimer timeout;
    QTimer cancelPoll;
    bool didTimeout = false;
    bool didCancel = false;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        didTimeout = true;
        reply->abort();
        loop.quit();
    });
    cancelPoll.setInterval(50);
    QObject::connect(&cancelPoll, &QTimer::timeout, &loop, [&]() {
        if (cancellation.isCancellationRequested()) {
            didCancel = true;
            reply->abort();
            loop.quit();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(qMax(1000, config.timeoutMs));
    if (cancellation.isValid()) {
        cancelPoll.start();
    }
    loop.exec();

    timeout.stop();
    cancelPoll.stop();
    const QByteArray response = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    if (didCancel) {
        return cloudError(
            QStringLiteral("CANCELLED"),
            QStringLiteral("OCR 识别已取消。"),
            timer.elapsed()
        );
    }
    if (didTimeout) {
        return cloudError(
            QStringLiteral("TIMEOUT"),
            QStringLiteral("云端 OCR 请求超时。"),
            timer.elapsed()
        );
    }
    if (statusCode == 401 || statusCode == 403) {
        return cloudError(
            QStringLiteral("AUTHENTICATION_FAILED"),
            QStringLiteral("云端 OCR 鉴权失败，请检查接口密钥和权限。"),
            timer.elapsed()
        );
    }
    if (networkError != QNetworkReply::NoError) {
        return cloudError(
            QStringLiteral("NETWORK_ERROR"),
            QStringLiteral("云端 OCR 网络请求失败：") + networkErrorText,
            timer.elapsed()
        );
    }
    if (statusCode < 200 || statusCode >= 300) {
        return cloudError(
            QStringLiteral("HTTP_") + QString::number(statusCode),
            QStringLiteral("云端 OCR 返回 HTTP ") + QString::number(statusCode) + QStringLiteral("。"),
            timer.elapsed()
        );
    }

    QString parseError;
    const QString text = extractCloudOcrText(response, &parseError);
    if (text.isEmpty()) {
        return cloudError(
            QStringLiteral("INVALID_RESPONSE"),
            parseError,
            timer.elapsed()
        );
    }

    OcrResult result;
    result.ok = true;
    result.engine = OcrEngine::CustomCloud;
    result.text = text;
    result.elapsedMs = timer.elapsed();
    return result;
}
