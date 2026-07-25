#include "diagnostic_helpers.h"

#include <QDnsHostAddressRecord>
#include <QDnsLookup>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtGlobal>

namespace {

QString dhTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString compactDiagnosticError(const QString &error)
{
    QString text = error.trimmed();
    if (text.isEmpty()) {
        return dhTr8("没有返回具体错误。");
    }
    text.replace(QStringLiteral("\n"), QStringLiteral(" "));
    if (text.size() > 180) {
        text = text.left(180) + QStringLiteral("...");
    }
    return text;
}

QString diagnosticStatusLine(const QString &name, const QString &status, const QString &detail)
{
    return detail.trimmed().isEmpty()
        ? name + dhTr8("：") + status
        : name + dhTr8("：") + status + dhTr8("\n  ") + detail.trimmed();
}

int pcm16PeakLevel(const QByteArray &pcm)
{
    int peak = 0;
    for (int i = 0; i + 1 < pcm.size(); i += 2) {
        const uchar lo = static_cast<uchar>(pcm.at(i));
        const uchar hi = static_cast<uchar>(pcm.at(i + 1));
        const qint16 sample = static_cast<qint16>((static_cast<int>(hi) << 8) | static_cast<int>(lo));
        peak = qMax(peak, qAbs(static_cast<int>(sample)));
    }
    return peak;
}

QString networkDnsLookupLine(
    const QString &host,
    const CancellationToken &cancellation,
    int timeoutMs)
{
    if (cancellation.isCancellationRequested()) {
        return QString();
    }

    QDnsLookup lookup;
    lookup.setType(QDnsLookup::A);
    lookup.setName(host);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QTimer cancellationTimer;
    cancellationTimer.setInterval(25);
    bool cancelled = false;

    QObject::connect(&lookup, &QDnsLookup::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!cancellation.isCancellationRequested()) {
            return;
        }
        cancelled = true;
        lookup.abort();
        loop.quit();
    });

    timeoutTimer.start(qMax(1, timeoutMs));
    cancellationTimer.start();
    lookup.lookup();
    loop.exec();
    cancellationTimer.stop();

    if (cancelled || cancellation.isCancellationRequested()) {
        return QString();
    }
    if (!timeoutTimer.isActive()) {
        lookup.abort();
        return diagnosticStatusLine(
            dhTr8("DNS ") + host,
            QStringLiteral("\u5931\u8d25"),
            QStringLiteral("\u89e3\u6790\u8d85\u65f6\u3002")
        );
    }
    timeoutTimer.stop();

    if (lookup.error() != QDnsLookup::NoError) {
        return diagnosticStatusLine(
            dhTr8("DNS ") + host,
            QStringLiteral("\u5931\u8d25"),
            compactDiagnosticError(lookup.errorString())
        );
    }

    const QList<QDnsHostAddressRecord> records = lookup.hostAddressRecords();
    return diagnosticStatusLine(
        dhTr8("DNS ") + host,
        records.isEmpty()
            ? QStringLiteral("\u5931\u8d25")
            : QStringLiteral("\u901a\u8fc7"),
        records.isEmpty()
            ? QStringLiteral(
                "\u672a\u8fd4\u56de IPv4 \u5730\u5740\u3002"
            )
            : records.constFirst().value().toString()
    );
}

QString networkProbeLine(
    const QString &name,
    const QUrl &url,
    bool useSystemProxy,
    const CancellationToken &cancellation)
{
    if (cancellation.isCancellationRequested()) {
        return QString();
    }

    QNetworkAccessManager manager;
    if (useSystemProxy) {
        const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
        for (const QNetworkProxy &proxy : proxies) {
            if (proxy.type() != QNetworkProxy::NoProxy && proxy.type() != QNetworkProxy::DefaultProxy) {
                manager.setProxy(proxy);
                break;
            }
        }
    } else {
        manager.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "vocekit-Diagnostics");
    QNetworkReply *reply = manager.head(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QTimer cancellationTimer;
    cancellationTimer.setInterval(25);
    bool cancelled = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!cancellation.isCancellationRequested()) {
            return;
        }
        cancelled = true;
        reply->abort();
        loop.quit();
    });
    timer.start(9000);
    cancellationTimer.start();
    loop.exec();
    cancellationTimer.stop();

    if (cancelled || cancellation.isCancellationRequested()) {
        reply->deleteLater();
        return QString();
    }
    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        return diagnosticStatusLine(name, dhTr8("失败"), dhTr8("连接超时。"));
    }
    timer.stop();

    const QNetworkReply::NetworkError error = reply->error();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorString = reply->errorString();
    reply->deleteLater();

    if (error == QNetworkReply::NoError || statusCode > 0) {
        return diagnosticStatusLine(
            name,
            dhTr8("可连接"),
            statusCode > 0 ? dhTr8("HTTP 状态：") + QString::number(statusCode) : QString()
        );
    }
    return diagnosticStatusLine(name, dhTr8("失败"), compactDiagnosticError(errorString));
}
