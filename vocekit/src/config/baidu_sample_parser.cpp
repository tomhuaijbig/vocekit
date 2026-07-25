#include "baidu_sample_parser.h"

#include <QRegExp>
#include <QStringList>
#include <QUrl>

namespace {

QString decodedCredentialValue(QString value)
{
    value = value.trimmed();
    value.remove(QLatin1Char('\\'));
    value = value.section(QLatin1Char('&'), 0, 0);
    value = value.section(QLatin1Char(';'), 0, 0);
    return QUrl::fromPercentEncoding(value.toUtf8()).trimmed();
}

QString extractNamedCredential(const QString &text, const QString &name)
{
    const QString normalized = QString(text)
        .replace(QStringLiteral("&amp;"), QStringLiteral("&"))
        .replace(QStringLiteral("\\u0026"), QStringLiteral("&"))
        .replace(QStringLiteral("\\/"), QStringLiteral("/"));
    const QString decodedText = QUrl::fromPercentEncoding(normalized.toUtf8());
    const QStringList candidates = QStringList() << normalized << decodedText;

    for (const QString &candidate : candidates) {
        QRegExp rx(name + QStringLiteral("\\s*[=:]\\s*[\"']?([^&\\s\"'<>\\\\]+)"));
        rx.setMinimal(false);
        if (rx.indexIn(candidate) >= 0) {
            const QString value = decodedCredentialValue(rx.cap(1));
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return QString();
}

} // namespace

bool extractBaiduCredentialsFromSampleCode(const QString &code, QString *apiKey, QString *secretKey)
{
    if (apiKey) {
        *apiKey = extractNamedCredential(code, QStringLiteral("client_id"));
    }
    if (secretKey) {
        *secretKey = extractNamedCredential(code, QStringLiteral("client_secret"));
    }
    return apiKey && secretKey && !apiKey->trimmed().isEmpty() && !secretKey->trimmed().isEmpty();
}
