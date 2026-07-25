#include "openai_compatible_model_provider.h"

#include "../api/api_client_utils.h"
#include "../runtime_log.h"

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

QString normalizedProviderId(QString providerId)
{
    providerId = providerId.trimmed();
    if (providerId.isEmpty()) {
        return QStringLiteral("openai");
    }
    if (providerId.startsWith(QStringLiteral("custom:"), Qt::CaseInsensitive)) {
        return QStringLiteral("custom:")
            + normalizeCustomModelProfileId(
                providerId.mid(QStringLiteral("custom:").size())
            );
    }
    return providerId.toLower();
}

bool isCustomProvider(const QString &providerId)
{
    return providerId == QStringLiteral("custom")
        || providerId.startsWith(QStringLiteral("custom:"));
}

struct ResolvedProviderConfig
{
    QString displayName;
    QUrl endpoint;
    QString apiKey;
    QString defaultModel;
    OperationError error;
};

CustomModelProfile customProfile(
    const SecretConfig &secrets,
    const QString &providerId,
    const QString &requestModelId)
{
    if (requestModelId.startsWith(
            QStringLiteral("custom:"),
            Qt::CaseInsensitive
        )) {
        return secrets.customModelProfileForProviderId(requestModelId);
    }
    if (providerId.startsWith(QStringLiteral("custom:"))) {
        return secrets.customModelProfileForProviderId(providerId);
    }
    return secrets.customModelProfileForProviderId(QString());
}

ResolvedProviderConfig resolveConfig(
    const QString &providerId,
    const SecretConfig &secrets,
    const QString &requestModelId)
{
    ResolvedProviderConfig config;
    if (!isCustomProvider(providerId)) {
        config.displayName = QStringLiteral("OpenAI");
        config.endpoint = QUrl(QStringLiteral(
            "https://api.openai.com/v1/chat/completions"
        ));
        config.apiKey = secrets.openaiApiKey.trimmed();
        config.defaultModel = QStringLiteral("gpt-5.5");
        if (config.apiKey.isEmpty()) {
            config.error = operationError(
                QStringLiteral("provider.configuration"),
                tr8(
                    "缺少 OpenAI 密钥。"
                    "请在“设置 -> 接口”中填写 OpenAI API Key。"
                )
            );
        }
        return config;
    }

    const CustomModelProfile profile = customProfile(
        secrets,
        providerId,
        requestModelId
    );
    config.displayName = profile.name.trimmed().isEmpty()
        ? tr8("自定义大模型")
        : profile.name.trimmed();
    if (!profile.hasEndpoint()) {
        config.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少自定义大模型接口地址。"
                "请在“设置 -> 接口”中配置对应的自定义大模型。"
            )
        );
        return config;
    }

    config.endpoint = openAiCompatibleChatUrl(profile.url);
    if (!config.endpoint.isValid() || config.endpoint.host().isEmpty()) {
        config.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "自定义大模型接口地址无效。"
                "可以填写根地址，也可以填写完整的 "
                "/v1/chat/completions 地址。"
            )
        );
        return config;
    }
    config.apiKey = profile.apiKey.trimmed();
    config.defaultModel = profile.model.trimmed();
    if (config.defaultModel.isEmpty()) {
        QString fallback = requestModelId.trimmed();
        if (fallback.startsWith(
                QStringLiteral("custom:"),
                Qt::CaseInsensitive
            )) {
            fallback = fallback.mid(QStringLiteral("custom:").size());
        }
        config.defaultModel =
            fallback.isEmpty() || fallback == QStringLiteral("model")
            ? QStringLiteral("custom-model")
            : fallback;
    }
    return config;
}

QString requestModelName(
    const QString &providerId,
    const ModelRequest &request,
    const ResolvedProviderConfig &config)
{
    if (isCustomProvider(providerId)) {
        return config.defaultModel;
    }

    QString model = request.modelId.trimmed();
    if (model.startsWith(QStringLiteral("openai:"), Qt::CaseInsensitive)) {
        model = model.mid(QStringLiteral("openai:").size());
    }
    return model.isEmpty() ? config.defaultModel : model;
}

QJsonObject requestBody(
    const QString &providerId,
    const ModelRequest &request,
    const ResolvedProviderConfig &config)
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
        requestModelName(providerId, request, config)
    );
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("temperature"), 0.2);
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
    return firstJsonStringValue(
        root,
        QVector<QStringList>()
            << (QStringList()
                << QStringLiteral("choices")
                << QStringLiteral("0")
                << QStringLiteral("message")
                << QStringLiteral("content"))
            << (QStringList() << QStringLiteral("text"))
            << (QStringList() << QStringLiteral("result"))
            << (QStringList()
                << QStringLiteral("data")
                << QStringLiteral("text"))
            << (QStringList()
                << QStringLiteral("data")
                << QStringLiteral("result"))
    );
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
        options.timeoutMs = request.stream ? 90000 : 35000;
    }
    return options;
}

OperationError networkOperationError(
    const NetworkResponse &response,
    const ResolvedProviderConfig &config)
{
    if (response.cancelled
        || response.error.code == QStringLiteral("request.cancelled")) {
        return cancelledError();
    }

    QString message = response.error.message.trimmed();
    const QString detail = response.error.detail.trimmed();
    if (response.error.code == QStringLiteral("network.timeout")) {
        message = config.displayName + tr8(" 模型阶段失败：网络请求超时。");
    } else if (detail.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
        message = config.displayName + tr8(
            " 模型阶段失败：SSL 运行库缺失或版本不匹配。"
            "请确认程序目录中存在 libeay32.dll 和 ssleay32.dll。"
        );
    } else if (response.statusCode == 401
               || detail.contains(
                   QStringLiteral("authentication"),
                   Qt::CaseInsensitive
               )) {
        message = config.displayName + tr8(
            " 模型阶段失败：接口认证失败。请检查 API Key。"
        );
    } else if (message.isEmpty()) {
        message = config.displayName + tr8(
            " 模型阶段失败：网络请求失败。"
        );
    } else {
        message = config.displayName + tr8(" 模型阶段失败：") + message;
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

OpenAiCompatibleModelProvider::OpenAiCompatibleModelProvider(
    const QString &providerId,
    bool useSystemProxy)
    : OpenAiCompatibleModelProvider(
          providerId,
          createProviderNetworkTransport(),
          []() { return loadSecrets(); },
          useSystemProxy
      )
{
}

OpenAiCompatibleModelProvider::OpenAiCompatibleModelProvider(
    const QString &providerId,
    const QSharedPointer<IProviderNetworkTransport> &transport,
    const SecretLoader &secretLoader,
    bool useSystemProxy)
    : m_providerId(normalizedProviderId(providerId)),
      m_transport(
          transport.isNull() ? createProviderNetworkTransport() : transport
      ),
      m_secretLoader(secretLoader),
      m_useSystemProxy(useSystemProxy)
{
    refreshConfiguration();
}

QString OpenAiCompatibleModelProvider::id() const
{
    return m_providerId;
}

ProviderCheckResult
OpenAiCompatibleModelProvider::checkConfiguration(
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
    const ResolvedProviderConfig config = resolveConfig(
        m_providerId,
        m_secrets,
        m_providerId
    );
    if (!config.error.isEmpty()) {
        result.error = config.error;
        return result;
    }

    ModelRequest request;
    request.executionId = effectiveCancellation.executionId();
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

ModelResult OpenAiCompatibleModelProvider::complete(
    const ModelRequest &request,
    const ModelDeltaCallback &onDelta,
    const CancellationToken &cancellation)
{
    return execute(request, onDelta, cancellation);
}

void OpenAiCompatibleModelProvider::refreshConfiguration()
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

ModelResult OpenAiCompatibleModelProvider::execute(
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

    const ResolvedProviderConfig config = resolveConfig(
        m_providerId,
        m_secrets,
        request.modelId
    );
    if (!config.error.isEmpty()) {
        result.error = config.error;
        return result;
    }

    QNetworkRequest networkRequest(config.endpoint);
    networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json")
    );
    if (!config.apiKey.isEmpty()) {
        networkRequest.setRawHeader(
            "Authorization",
            QByteArrayLiteral("Bearer ") + config.apiKey.toUtf8()
        );
    }
    if (request.stream) {
        networkRequest.setRawHeader("Accept", "text/event-stream");
    }
    const QByteArray body = QJsonDocument(
        requestBody(m_providerId, request, config)
    ).toJson(QJsonDocument::Compact);

    QElapsedTimer timer;
    timer.start();
    logRuntimeEvent(
        config.displayName,
        request.stream ? tr8("流式开始") : tr8("开始"),
        QStringLiteral("模型=")
            + requestModelName(m_providerId, request, config)
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
        result.error = networkOperationError(response, config);
    } else if (request.stream) {
        if (!streamApiError.trimmed().isEmpty()) {
            result.error = operationError(
                QStringLiteral("model.api"),
                config.displayName + tr8(" 调用失败：") + streamApiError,
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
                config.displayName + tr8(" 返回的不是有效 JSON。"),
                parseError.errorString()
            );
        } else {
            const QJsonObject root = document.object();
            if (root.contains(QStringLiteral("error"))) {
                const QString apiError = apiErrorMessage(root);
                result.error = operationError(
                    QStringLiteral("model.api"),
                    config.displayName
                        + tr8(" 调用失败：")
                        + apiError,
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
            config.displayName + tr8(" 没有返回结果。")
        );
    }
    logRuntimeEvent(
        config.displayName,
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("模型=")
            + requestModelName(m_providerId, request, config)
            + QStringLiteral("，输出字数=")
            + QString::number(result.text.size())
            + (!result.error.message.trimmed().isEmpty()
                ? QStringLiteral("，错误=") + result.error.message
                : QString()),
        result.durationMs
    );
    return result;
}

QSharedPointer<IModelProvider> createOpenAiCompatibleModelProvider(
    const QString &providerId,
    bool useSystemProxy)
{
    return QSharedPointer<IModelProvider>(
        new OpenAiCompatibleModelProvider(providerId, useSystemProxy)
    );
}

QSharedPointer<IModelProvider> createOpenAiCompatibleModelProvider(
    const QString &providerId,
    const SecretConfig &secrets,
    bool useSystemProxy)
{
    return QSharedPointer<IModelProvider>(
        new OpenAiCompatibleModelProvider(
            providerId,
            createProviderNetworkTransport(),
            [secrets]() { return secrets; },
            useSystemProxy
        )
    );
}
