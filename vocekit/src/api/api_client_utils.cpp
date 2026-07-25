#include "api_client_utils.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QRegExp>

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
    text = text.trimmed();
    if (text.isEmpty()) {
        return QUrl();
    }
    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }
    if (!text.endsWith(QStringLiteral("/chat/completions"))) {
        text += text.endsWith(QStringLiteral("/v1"))
            ? QStringLiteral("/chat/completions")
            : QStringLiteral("/v1/chat/completions");
    }
    return QUrl(text);
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
