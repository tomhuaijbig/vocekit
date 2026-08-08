#include "api_client_utils.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QRegExp>

namespace {

QUrl normalizedEndpointUrl(QString text, const QString &endpointPath)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QUrl();
    }
    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }

    QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        || url.host().isEmpty()
        || url.hasQuery()
        || url.hasFragment()) {
        return QUrl();
    }

    QString path = url.path(QUrl::FullyEncoded);
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.contains(QStringLiteral("/v1/v1/"))
        || path.endsWith(QStringLiteral("/v1/v1"))) {
        return QUrl();
    }
    if (path.isEmpty() || path == QStringLiteral("/")) {
        path = endpointPath;
    } else if (path.endsWith(endpointPath)) {
        // The caller supplied the complete endpoint, possibly below a gateway path.
    } else if (path.endsWith(QStringLiteral("/v1"))) {
        path += endpointPath.mid(3);
    } else {
        path += endpointPath;
    }
    url.setPath(path, QUrl::StrictMode);
    return url;
}

} // namespace

QString compactLogText(QString text, int maxLength)
{
    text.replace(QRegExp(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    text = text.trimmed();
    if (text.size() > maxLength) {
        text = text.left(maxLength) + QStringLiteral("...");
    }
    return text;
}

QString networkLogTarget(const QUrl &url)
{
    QString target = url.host();
    if (!url.path().isEmpty()) {
        target += url.path();
    }
    return target.isEmpty() ? QStringLiteral("unknown") : target;
}

QUrl urlWithDefaultHttps(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QUrl();
    }
    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }
    return QUrl(text);
}

QUrl openAiCompatibleChatUrl(QString text)
{
    return normalizedEndpointUrl(
        text,
        QStringLiteral("/v1/chat/completions")
    );
}

QUrl anthropicMessagesUrl(QString text)
{
    return normalizedEndpointUrl(
        text,
        QStringLiteral("/v1/messages")
    );
}

QString jsonPathStringValue(const QJsonValue &value, const QStringList &path)
{
    QJsonValue current = value;
    for (const QString &part : path) {
        if (current.isObject()) {
            current = current.toObject().value(part);
        } else if (current.isArray()) {
            bool ok = false;
            const int index = part.toInt(&ok);
            const QJsonArray array = current.toArray();
            current = ok && index >= 0 && index < array.size()
                ? array.at(index)
                : QJsonValue();
        } else {
            return QString();
        }
    }
    if (current.isString()) {
        return current.toString().trimmed();
    }
    if (current.isDouble()) {
        return QString::number(current.toDouble()).trimmed();
    }
    return QString();
}

QString firstJsonStringValue(
    const QJsonObject &root,
    const QVector<QStringList> &paths)
{
    for (const QStringList &path : paths) {
        const QString value = jsonPathStringValue(root, path);
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    const int blockSize = 64;
    QByteArray normalizedKey = key;
    if (normalizedKey.size() > blockSize) {
        normalizedKey = QCryptographicHash::hash(
            normalizedKey,
            QCryptographicHash::Sha256
        );
    }
    normalizedKey = normalizedKey.leftJustified(blockSize, '\0', true);

    QByteArray innerPad(blockSize, char(0x36));
    QByteArray outerPad(blockSize, char(0x5c));
    for (int i = 0; i < blockSize; ++i) {
        innerPad[i] = char(innerPad.at(i) ^ normalizedKey.at(i));
        outerPad[i] = char(outerPad.at(i) ^ normalizedKey.at(i));
    }
    const QByteArray innerHash = QCryptographicHash::hash(
        innerPad + message,
        QCryptographicHash::Sha256
    );
    return QCryptographicHash::hash(
        outerPad + innerHash,
        QCryptographicHash::Sha256
    );
}
