#include "deepseek_model_provider.h"

#include "../runtime_log.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

OperationError operationError(
    const QString &code,
    const QString &message,
    const QString &detail = QString(),
    bool retryable = false)
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    error.retryable = retryable;
    return error;
}

OperationError cancelledError()
{
    return operationError(
        QStringLiteral("task.cancelled"),
        tr8("任务已取消。")
    );
}

OperationError networkOperationError(const NetworkResponse &response)
{
    if (response.cancelled
        || response.error.code == QStringLiteral("request.cancelled")) {
        return cancelledError();
    }

    QString message = response.error.message.trimmed();
    const QString detail = response.error.detail.trimmed();
    if (response.error.code == QStringLiteral("network.timeout")) {
        message = tr8("DeepSeek 模型阶段失败：网络请求超时。");
    } else if (detail.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
        message = tr8(
            "DeepSeek 模型阶段失败：TLS 初始化或握手失败。"
            "请检查系统证书、代理设置和程序目录中的 Qt 6 TLS 插件。"
        );
    } else if (response.statusCode == 401
               || detail.contains(
                   QStringLiteral("authentication"),
                   Qt::CaseInsensitive
               )) {
        message = tr8(
            "DeepSeek 模型阶段失败：接口认证失败。"
            "请检查 DeepSeek API Key。"
        );
    } else if (message.isEmpty()) {
        message = tr8("DeepSeek 模型阶段失败：网络请求失败。");
    } else {
        message = tr8("DeepSeek 模型阶段失败：") + message;
    }

    OperationError error = response.error;
    error.code = error.code.trimmed().isEmpty()
        ? QStringLiteral("model.network")
        : error.code;
    error.message = message;
    error.detail = detail;
    error.retryable = true;
    return error;
}

QJsonObject requestBody(const ModelRequest &request, bool stream)
{
    QJsonArray messages;
    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"), request.systemPrompt);
    messages.append(system);
    QJsonObject user;
    user.insert(QStringLiteral("role"), QStringLiteral("user"));
    user.insert(QStringLiteral("content"), request.userPrompt);
    messages.append(user);

    QJsonObject body;
    body.insert(
        QStringLiteral("model"),
        request.modelId.trimmed().isEmpty()
            ? QStringLiteral("deepseek-v4-flash")
            : request.modelId.trimmed()
    );
    body.insert(QStringLiteral("messages"), messages);
    const ModelSamplingSettings sampling =
        normalizeModelSamplingSettings(request.sampling);
    if (sampling.temperatureEnabled) {
        body.insert(QStringLiteral("temperature"), sampling.temperature);
    }
    if (sampling.topPEnabled) {
        body.insert(QStringLiteral("top_p"), sampling.topP);
    }
    body.insert(QStringLiteral("max_tokens"), 1024);
    body.insert(QStringLiteral("stream"), stream);
    QJsonObject thinking;
    thinking.insert(QStringLiteral("type"), QStringLiteral("disabled"));
    body.insert(QStringLiteral("thinking"), thinking);
    return body;
}

QString apiErrorMessage(const QJsonObject &root)
{
    const QJsonValue value = root.value(QStringLiteral("error"));
    if (value.isObject()) {
        return value.toObject().value(QStringLiteral("message")).toString();
    }
    return value.toString();
}

QString completionText(const QJsonObject &root)
{
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return QString();
    }
    return choices.at(0)
        .toObject()
        .value(QStringLiteral("message"))
        .toObject()
        .value(QStringLiteral("content"))
        .toString()
        .trimmed();
}

QString streamDelta(const QJsonObject &root)
{
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return QString();
    }
    return choices.at(0)
        .toObject()
        .value(QStringLiteral("delta"))
        .toObject()
        .value(QStringLiteral("content"))
        .toString();
}

NetworkRequestOptions requestOptions(
    const ModelRequest &request,
    bool useSystemProxy)
{
    NetworkRequestOptions options = request.network;
    options.globalUseSystemProxy =
        request.network.globalUseSystemProxy || useSystemProxy;
    if (options.timeoutMs <= 0
        || options.timeoutMs == NetworkRequestOptions().timeoutMs) {
        options.timeoutMs = request.stream ? 90000 : 25000;
    }
    return options;
}

} // namespace

DeepSeekModelProvider::DeepSeekModelProvider(bool useSystemProxy)
    : DeepSeekModelProvider(
        createProviderNetworkTransport(),
        []() { return loadSecrets(); },
        useSystemProxy
    )
{
}

DeepSeekModelProvider::DeepSeekModelProvider(
    const QSharedPointer<IProviderNetworkTransport> &transport,
    const SecretLoader &secretLoader,
    bool useSystemProxy)
    : m_transport(
          transport.isNull() ? createProviderNetworkTransport() : transport
      ),
      m_secretLoader(secretLoader),
      m_useSystemProxy(useSystemProxy)
{
    refreshConfiguration();
}

QString DeepSeekModelProvider::id() const
{
    return QStringLiteral("deepseek");
}

ProviderCheckResult DeepSeekModelProvider::checkConfiguration(
    const CancellationToken &cancellation) const
{
    ProviderCheckResult result;
    CancellationSource ownedCancellation;
    const CancellationToken effectiveCancellation = cancellation.isValid()
        ? cancellation
        : ownedCancellation.token();
    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasDeepSeek()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8("未填写 DeepSeek API Key。")
        );
        return result;
    }

    ModelRequest request;
    request.executionId = effectiveCancellation.executionId();
    request.modelId = QStringLiteral("deepseek-v4-flash");
    request.systemPrompt = tr8("你是接口自检助手。只回复 OK。");
    request.userPrompt = tr8("请只回复 OK，用于确认接口可用。");
    request.stream = false;
    request.network.timeoutMs = 15000;
    request.network.globalUseSystemProxy = m_useSystemProxy;
    const ModelResult modelResult = execute(
        request,
        ModelDeltaCallback(),
        effectiveCancellation
    );
    result.available = modelResult.error.isEmpty()
        && !modelResult.text.trimmed().isEmpty();
    result.durationMs = modelResult.durationMs;
    result.error = modelResult.error;
    if (result.available) {
        result.message = tr8("模型接口返回成功。");
    }
    return result;
}

ModelResult DeepSeekModelProvider::complete(
    const ModelRequest &request,
    const ModelDeltaCallback &onDelta,
    const CancellationToken &cancellation)
{
    return execute(request, onDelta, cancellation);
}

void DeepSeekModelProvider::refreshConfiguration()
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

ModelResult DeepSeekModelProvider::execute(
    const ModelRequest &request,
    const ModelDeltaCallback &onDelta,
    const CancellationToken &cancellation) const
{
    ModelResult result;
    result.executionId = request.executionId.isValid()
        ? request.executionId
        : cancellation.executionId();
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasDeepSeek()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少 DeepSeek 密钥。"
                "请在“设置 -> 接口”中填写 DeepSeek API Key。"
            )
        );
        return result;
    }

    QNetworkRequest networkRequest(
        QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"))
    );
    networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json")
    );
    networkRequest.setRawHeader(
        "Authorization",
        QByteArrayLiteral("Bearer ")
            + m_secrets.deepseekApiKey.trimmed().toUtf8()
    );
    if (request.stream) {
        networkRequest.setRawHeader("Accept", "text/event-stream");
    }
    const QByteArray body = QJsonDocument(
        requestBody(request, request.stream)
    ).toJson(QJsonDocument::Compact);

    QElapsedTimer timer;
    timer.start();
    logRuntimeEvent(
        tr8("DeepSeek"),
        request.stream ? tr8("流式开始") : tr8("开始"),
        QStringLiteral("模型=") + request.modelId
            + QStringLiteral("，输入字数=")
            + QString::number(request.userPrompt.size())
    );

    QString text;
    QString streamApiError;
    QByteArray streamBuffer;
    auto processStreamBuffer = [&]() {
        int newline = -1;
        while ((newline = streamBuffer.indexOf('\n')) >= 0) {
            const QByteArray line = streamBuffer.left(newline).trimmed();
            streamBuffer.remove(0, newline + 1);
            if (!line.startsWith(QByteArrayLiteral("data:"))) {
                continue;
            }
            const QByteArray payload = line.mid(5).trimmed();
            if (payload.isEmpty()
                || payload == QByteArrayLiteral("[DONE]")) {
                continue;
            }
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError
                || !document.isObject()) {
                continue;
            }
            const QJsonObject root = document.object();
            if (root.contains(QStringLiteral("error"))) {
                streamApiError = apiErrorMessage(root);
                continue;
            }
            const QString delta = streamDelta(root);
            if (!delta.isEmpty()) {
                text += delta;
                if (onDelta) {
                    onDelta(delta);
                }
            }
        }
    };

    NetworkResponse response;
    const NetworkRequestOptions options = requestOptions(
        request,
        m_useSystemProxy
    );
    if (request.stream) {
        response = m_transport->postEventStream(
            networkRequest,
            body,
            options,
            [&](const QByteArray &chunk) {
                streamBuffer += chunk;
                processStreamBuffer();
            },
            cancellation
        );
        if (!streamBuffer.isEmpty()) {
            streamBuffer.append('\n');
            processStreamBuffer();
        }
    } else {
        response = m_transport->postJson(
            networkRequest,
            body,
            options,
            cancellation
        );
    }

    result.rawResponse = response.body;
    result.durationMs = response.durationMs >= 0
        ? response.durationMs
        : timer.elapsed();
    if (!response.isSuccess()) {
        result.error = networkOperationError(response);
    } else if (request.stream) {
        if (!streamApiError.trimmed().isEmpty()) {
            result.error = operationError(
                QStringLiteral("model.api"),
                tr8("DeepSeek 调用失败：") + streamApiError,
                streamApiError
            );
        } else {
            result.text = text.trimmed();
        }
    } else {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(response.body, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            result.error = operationError(
                QStringLiteral("model.invalid_response"),
                tr8("DeepSeek 返回的不是有效 JSON。"),
                parseError.errorString()
            );
        } else {
            const QJsonObject root = document.object();
            if (root.contains(QStringLiteral("error"))) {
                const QString apiError = apiErrorMessage(root);
                result.error = operationError(
                    QStringLiteral("model.api"),
                    tr8("DeepSeek 调用失败：") + apiError,
                    apiError
                );
            } else {
                result.text = completionText(root);
            }
        }
    }

    if (result.error.isEmpty() && result.text.trimmed().isEmpty()) {
        result.error = operationError(
            QStringLiteral("model.empty_result"),
            tr8("DeepSeek 没有返回结果。")
        );
    }
    logRuntimeEvent(
        tr8("DeepSeek"),
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("模型=") + request.modelId
            + QStringLiteral("，输出字数=")
            + QString::number(result.text.size())
            + (!result.error.message.trimmed().isEmpty()
                ? QStringLiteral("，错误=") + result.error.message
                : QString()),
        result.durationMs
    );
    return result;
}

QSharedPointer<IModelProvider> createDeepSeekModelProvider(
    bool useSystemProxy)
{
    return QSharedPointer<IModelProvider>(
        new DeepSeekModelProvider(useSystemProxy)
    );
}
