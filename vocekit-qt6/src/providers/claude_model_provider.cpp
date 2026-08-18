#include "claude_model_provider.h"

#include "../api/api_client_utils.h"
#include "../runtime_log.h"
#include "model_request_customization.h"
#include "model_response_metadata.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

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

QString requestModelName(const ModelRequest &request)
{
    QString model = request.modelId.trimmed();
    if (model.startsWith(QStringLiteral("claude:"), Qt::CaseInsensitive)) {
        model = model.mid(QStringLiteral("claude:").size());
    }
    return model.isEmpty()
        ? QStringLiteral("claude-sonnet-5")
        : model;
}

QJsonObject requestBody(const ModelRequest &request)
{
    QJsonArray messages;
    QJsonObject user;
    user.insert(QStringLiteral("role"), QStringLiteral("user"));
    user.insert(QStringLiteral("content"), request.userPrompt);
    messages.append(user);

    QJsonObject body;
    body.insert(QStringLiteral("model"), requestModelName(request));
    body.insert(QStringLiteral("system"), request.systemPrompt);
    body.insert(QStringLiteral("messages"), messages);
    // Current Claude 5 models reject temperature/top_p, so omit function
    // sampling overrides instead of turning a valid request into HTTP 400.
    body.insert(QStringLiteral("max_tokens"), 1024);
    body.insert(QStringLiteral("stream"), request.stream);
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
    QStringList parts;
    const QJsonArray content = root.value(QStringLiteral("content")).toArray();
    for (const QJsonValue &value : content) {
        const QJsonObject block = value.toObject();
        if (block.value(QStringLiteral("type")).toString()
            != QStringLiteral("text")) {
            continue;
        }
        const QString text =
            block.value(QStringLiteral("text")).toString().trimmed();
        if (!text.isEmpty()) {
            parts.append(text);
        }
    }
    return parts.join(QStringLiteral("\n")).trimmed();
}

QString streamDelta(const QJsonObject &root)
{
    if (root.value(QStringLiteral("type")).toString()
        != QStringLiteral("content_block_delta")) {
        return QString();
    }
    return root.value(QStringLiteral("delta"))
        .toObject()
        .value(QStringLiteral("text"))
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
        options.timeoutMs = request.stream ? 90000 : 35000;
    }
    return options;
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
        message = tr8("Claude 模型阶段失败：网络请求超时。");
    } else if (detail.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
        message = tr8(
            "Claude 模型阶段失败：TLS 初始化或握手失败。"
            "请检查系统证书、代理设置和程序目录中的 Qt 6 TLS 插件。"
        );
    } else if (response.statusCode == 401
               || detail.contains(
                   QStringLiteral("authentication"),
                   Qt::CaseInsensitive
               )) {
        message = tr8(
            "Claude 模型阶段失败：接口认证失败。"
            "请检查 Anthropic API Key。"
        );
    } else if (message.isEmpty()) {
        message = tr8("Claude 模型阶段失败：网络请求失败。");
    } else {
        message = tr8("Claude 模型阶段失败：") + message;
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

} // namespace

ClaudeModelProvider::ClaudeModelProvider(bool useSystemProxy)
    : ClaudeModelProvider(
          createProviderNetworkTransport(),
          []() { return loadSecrets(); },
          useSystemProxy
      )
{
}

ClaudeModelProvider::ClaudeModelProvider(
    const QSharedPointer<IProviderNetworkTransport> &transport,
    const SecretLoader &secretLoader,
    bool useSystemProxy)
    : m_transport(
          transport.isNull() ? createProviderNetworkTransport() : transport
      ),
      m_secretLoader(secretLoader),
      m_useSystemProxy(useSystemProxy)
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

QString ClaudeModelProvider::id() const
{
    return QStringLiteral("claude");
}

ProviderCheckResult ClaudeModelProvider::checkConfiguration(
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
    if (!m_secrets.hasAnthropic()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8("未填写 Anthropic API Key。")
        );
        return result;
    }

    ModelRequest request;
    request.executionId = effectiveCancellation.executionId();
    request.modelId = QStringLiteral("claude:claude-sonnet-5");
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

ModelResult ClaudeModelProvider::complete(
    const ModelRequest &request,
    const ModelDeltaCallback &onDelta,
    const CancellationToken &cancellation)
{
    return execute(request, onDelta, cancellation);
}

void ClaudeModelProvider::refreshConfiguration()
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

ModelResult ClaudeModelProvider::execute(
    const ModelRequest &request,
    const ModelDeltaCallback &onDelta,
    const CancellationToken &cancellation) const
{
    ModelResult result;
    result.executionId = request.executionId.isValid()
        ? request.executionId
        : cancellation.executionId();
    result.telemetry.providerId = QStringLiteral("claude");
    result.telemetry.requestedAtUtc = QDateTime::currentDateTimeUtc();
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasAnthropic()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少 Claude 密钥。"
                "请在“设置 -> 接口”中填写 Anthropic API Key。"
            )
        );
        return result;
    }

    QUrl endpoint(QStringLiteral("https://api.anthropic.com/v1/messages"));
    const QString configuredBaseUrl =
        m_secrets.anthropicBaseUrl.trimmed();
    if (!configuredBaseUrl.isEmpty()) {
        endpoint = anthropicMessagesUrl(configuredBaseUrl);
        if (!endpoint.isValid() || endpoint.host().isEmpty()) {
            result.error = operationError(
                QStringLiteral("provider.configuration"),
                tr8(
                    "Anthropic Base URL 无效。"
                    "请填写根地址、/v1 地址或完整的 /v1/messages 地址。"
                )
            );
            return result;
        }
    }

    QNetworkRequest networkRequest(endpoint);
    networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json")
    );
    networkRequest.setRawHeader(
        "x-api-key",
        m_secrets.anthropicApiKey.trimmed().toUtf8()
    );
    networkRequest.setRawHeader(
        "anthropic-version",
        QByteArrayLiteral("2023-06-01")
    );
    QJsonObject bodyObject = customizedModelRequestBody(
        requestBody(request),
        advancedModelSettings(request.extra)
    );
    const bool effectiveStream = bodyObject
        .value(QStringLiteral("stream"))
        .toBool(false);
    result.telemetry.actualRequest = bodyObject;
    result.telemetry.modelId = bodyObject
        .value(QStringLiteral("model"))
        .toString(requestModelName(request));
    if (effectiveStream) {
        networkRequest.setRawHeader("Accept", "text/event-stream");
    }
    const QByteArray body = QJsonDocument(bodyObject)
        .toJson(QJsonDocument::Compact);

    QElapsedTimer timer;
    timer.start();
    logRuntimeEvent(
        tr8("Claude"),
        effectiveStream ? tr8("流式开始") : tr8("开始"),
        QStringLiteral("模型=") + requestModelName(request)
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
            if (payload.isEmpty()) {
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
            updateModelMetadataFromJson(root, &result.telemetry);
            if (root.contains(QStringLiteral("error"))) {
                streamApiError = apiErrorMessage(root);
                continue;
            }
            const QString delta = streamDelta(root);
            if (!delta.isEmpty()) {
                if (result.telemetry.firstTokenMs < 0) {
                    result.telemetry.firstTokenMs = timer.elapsed();
                }
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
    if (effectiveStream) {
        response = m_transport->postEventStream(
            networkRequest,
            body,
            options,
            [&](const QByteArray &chunk) {
                if (result.telemetry.firstResponseMs < 0) {
                    result.telemetry.firstResponseMs = timer.elapsed();
                }
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
    result.telemetry.httpStatusCode = response.statusCode;
    result.durationMs = response.durationMs >= 0
        ? response.durationMs
        : timer.elapsed();
    result.telemetry.totalDurationMs = result.durationMs;
    if (!response.isSuccess()) {
        result.error = networkOperationError(response);
    } else if (effectiveStream) {
        if (!streamApiError.trimmed().isEmpty()) {
            result.error = operationError(
                QStringLiteral("model.api"),
                tr8("Claude 调用失败：") + streamApiError,
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
                tr8("Claude 返回的不是有效 JSON。"),
                parseError.errorString()
            );
        } else {
            const QJsonObject root = document.object();
            updateModelMetadataFromJson(root, &result.telemetry);
            if (root.contains(QStringLiteral("error"))) {
                const QString apiError = apiErrorMessage(root);
                result.error = operationError(
                    QStringLiteral("model.api"),
                    tr8("Claude 调用失败：") + apiError,
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
            tr8("Claude 没有返回结果。")
        );
    }
    logRuntimeEvent(
        tr8("Claude"),
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("模型=") + requestModelName(request)
            + QStringLiteral("，输出字数=")
            + QString::number(result.text.size())
            + (!result.error.message.trimmed().isEmpty()
                ? QStringLiteral("，错误=") + result.error.message
                : QString()),
        result.durationMs
    );
    return result;
}

QSharedPointer<IModelProvider> createClaudeModelProvider(
    bool useSystemProxy)
{
    return QSharedPointer<IModelProvider>(
        new ClaudeModelProvider(useSystemProxy)
    );
}

QSharedPointer<IModelProvider> createClaudeModelProvider(
    const SecretConfig &secrets,
    bool useSystemProxy)
{
    return QSharedPointer<IModelProvider>(
        new ClaudeModelProvider(
            createProviderNetworkTransport(),
            [secrets]() { return secrets; },
            useSystemProxy
        )
    );
}
