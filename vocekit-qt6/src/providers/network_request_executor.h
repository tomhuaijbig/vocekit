#ifndef VOCEKIT_NETWORK_REQUEST_EXECUTOR_H
#define VOCEKIT_NETWORK_REQUEST_EXECUTOR_H

#include "provider_types.h"

#include "../tasks/cancellation_token.h"

#include <QNetworkRequest>

// 所有 HTTP 请求统一经过这里处理代理、超时、取消和错误分类。
class NetworkRequestExecutor
{
public:
    static NetworkResponse get(
        const QNetworkRequest &request,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    );

    static NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    );

    static NetworkResponse postEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &cancellation
    );

    static QString resolvedNetworkPolicy(
        const NetworkRequestOptions &options
    );

private:
    static NetworkResponse execute(
        const QByteArray &method,
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &cancellation
    );
};

#endif // VOCEKIT_NETWORK_REQUEST_EXECUTOR_H
