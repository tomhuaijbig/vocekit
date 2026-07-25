#include "network_request_executor.h"

#include "../result_flow_config.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QTimer>

namespace {

void applyProxy(
    QNetworkAccessManager *manager,
    const QNetworkRequest &request,
    const QString &policy)
{
    if (!manager) {
        return;
    }
    if (policy == QStringLiteral("direct")) {
        manager->setProxy(QNetworkProxy::NoProxy);
        return;
    }

    const QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(
            QNetworkProxyQuery(request.url())
        );
    manager->setProxy(
        proxies.isEmpty() ? QNetworkProxy::NoProxy : proxies.constFirst()
    );
}

OperationError httpError(int statusCode, const QByteArray &body)
{
    OperationError error;
    error.code = QStringLiteral("http.%1").arg(statusCode);
    error.message = QStringLiteral("网络请求返回错误状态：%1。")
        .arg(statusCode);
    error.detail = QString::fromUtf8(body);
    error.retryable =
        statusCode == 408 || statusCode == 425 || statusCode == 429
        || statusCode >= 500;
    return error;
}

OperationError networkError(QNetworkReply *reply)
{
    OperationError error;
    error.code = QStringLiteral("network.%1")
        .arg(reply ? int(reply->error()) : -1);
    error.message = QStringLiteral("网络请求失败。");
    error.detail = reply ? reply->errorString() : QString();
    error.retryable = true;
    return error;
}

} // namespace

NetworkResponse NetworkRequestExecutor::get(
    const QNetworkRequest &request,
    const NetworkRequestOptions &options,
    const CancellationToken &cancellation)
{
    return execute(
        QByteArrayLiteral("GET"),
        request,
        QByteArray(),
        options,
        StreamDataCallback(),
        cancellation
    );
}

NetworkResponse NetworkRequestExecutor::postJson(
    const QNetworkRequest &request,
    const QByteArray &body,
    const NetworkRequestOptions &options,
    const CancellationToken &cancellation)
{
    return execute(
        QByteArrayLiteral("POST"),
        request,
        body,
        options,
        StreamDataCallback(),
        cancellation
    );
}

NetworkResponse NetworkRequestExecutor::postEventStream(
    const QNetworkRequest &request,
    const QByteArray &body,
    const NetworkRequestOptions &options,
    const StreamDataCallback &onData,
    const CancellationToken &cancellation)
{
    return execute(
        QByteArrayLiteral("POST"),
        request,
        body,
        options,
        onData,
        cancellation
    );
}

QString NetworkRequestExecutor::resolvedNetworkPolicy(
    const NetworkRequestOptions &options)
{
    return resolveNetworkPolicy(
        options.networkPolicy,
        options.globalUseSystemProxy
    );
}

NetworkResponse NetworkRequestExecutor::execute(
    const QByteArray &method,
    const QNetworkRequest &originalRequest,
    const QByteArray &body,
    const NetworkRequestOptions &options,
    const StreamDataCallback &onData,
    const CancellationToken &cancellation)
{
    NetworkResponse response;
    response.executionId = cancellation.executionId();
    if (cancellation.isCancellationRequested()) {
        response.cancelled = true;
        response.error.code = QStringLiteral("request.cancelled");
        response.error.message = QStringLiteral("请求已取消。");
        return response;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(originalRequest);
    applyProxy(
        &manager,
        request,
        resolvedNetworkPolicy(options)
    );
    if (method == QByteArrayLiteral("POST")
        && !request.hasRawHeader("Content-Type")) {
        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            QStringLiteral("application/json")
        );
    }

    QNetworkReply *reply = method == QByteArrayLiteral("GET")
        ? manager.get(request)
        : manager.post(request, body);
    QElapsedTimer elapsed;
    elapsed.start();
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QTimer cancellationTimer;
    cancellationTimer.setInterval(15);
    bool timedOut = false;
    bool cancelled = false;

    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
        const QByteArray chunk = reply->readAll();
        response.body.append(chunk);
        if (onData && !chunk.isEmpty()) {
            onData(chunk);
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        loop.quit();
    });
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!cancellation.isCancellationRequested()) {
            return;
        }
        cancelled = true;
        reply->abort();
        loop.quit();
    });

    timeoutTimer.start(qBound(1, options.timeoutMs, 10 * 60 * 1000));
    cancellationTimer.start();
    loop.exec();
    timeoutTimer.stop();
    cancellationTimer.stop();

    const QByteArray remaining =
        reply->isOpen() ? reply->readAll() : QByteArray();
    response.body.append(remaining);
    if (onData && !remaining.isEmpty()) {
        onData(remaining);
    }
    response.durationMs = elapsed.elapsed();
    response.statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute
    ).toInt();

    if (cancelled || cancellation.isCancellationRequested()) {
        response.cancelled = true;
        response.error.code = QStringLiteral("request.cancelled");
        response.error.message = QStringLiteral("请求已取消。");
    } else if (timedOut) {
        response.error.code = QStringLiteral("network.timeout");
        response.error.message = QStringLiteral("网络请求超时。");
        response.error.retryable = true;
    } else if (response.statusCode >= 400) {
        response.error = httpError(response.statusCode, response.body);
    } else if (reply->error() != QNetworkReply::NoError) {
        response.error = networkError(reply);
    } else if (response.statusCode < 200
               || response.statusCode >= 300) {
        response.error = httpError(response.statusCode, response.body);
    }

    delete reply;
    return response;
}
