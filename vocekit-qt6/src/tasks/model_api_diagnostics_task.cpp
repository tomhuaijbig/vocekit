#include "model_api_diagnostics_task.h"

#include "../api/api_client_utils.h"
#include "../config/app_settings_defaults.h"
#include "../config/model_advanced_settings.h"
#include "../config/secret_config.h"
#include "../providers/claude_model_provider.h"
#include "../providers/deepseek_model_provider.h"
#include "../providers/openai_compatible_model_provider.h"
#include "../providers/provider_network_transport.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

struct ResolvedDiagnostics
{
    QString provider;
    QUrl modelsUrl;
    QByteArray authHeaderName;
    QByteArray authHeaderValue;
    bool anthropic = false;
};

QUrl siblingEndpoint(const QUrl &chatUrl, const QString &suffix)
{
    if (!chatUrl.isValid()) {
        return QUrl();
    }
    QUrl url = chatUrl;
    QString path = url.path();
    const QString chatSuffix = QStringLiteral("/chat/completions");
    const QString messagesSuffix = QStringLiteral("/messages");
    if (path.endsWith(chatSuffix)) {
        path.chop(chatSuffix.size());
    } else if (path.endsWith(messagesSuffix)) {
        path.chop(messagesSuffix.size());
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    path += suffix.startsWith(QLatin1Char('/')) ? suffix : QStringLiteral("/") + suffix;
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

ResolvedDiagnostics resolveDiagnostics(const QString &modelId)
{
    const SecretConfig secrets = loadSecrets();
    ResolvedDiagnostics resolved;
    resolved.provider = modelProvider(modelId);
    if (resolved.provider == QStringLiteral("deepseek")) {
        resolved.modelsUrl = QUrl(QStringLiteral("https://api.deepseek.com/models"));
        resolved.authHeaderName = QByteArrayLiteral("Authorization");
        resolved.authHeaderValue = QByteArrayLiteral("Bearer ")
            + secrets.deepseekApiKey.trimmed().toUtf8();
        return resolved;
    }
    if (resolved.provider == QStringLiteral("claude")) {
        QUrl messages(QStringLiteral("https://api.anthropic.com/v1/messages"));
        if (!secrets.anthropicBaseUrl.trimmed().isEmpty()) {
            messages = anthropicMessagesUrl(secrets.anthropicBaseUrl);
        }
        resolved.modelsUrl = siblingEndpoint(messages, QStringLiteral("/models"));
        resolved.authHeaderName = QByteArrayLiteral("x-api-key");
        resolved.authHeaderValue = secrets.anthropicApiKey.trimmed().toUtf8();
        resolved.anthropic = true;
        return resolved;
    }

    QString baseUrl = secrets.openaiBaseUrl;
    QString apiKey = secrets.openaiApiKey;
    if (resolved.provider.startsWith(QStringLiteral("custom"))) {
        const CustomModelProfile profile =
            secrets.customModelProfileForProviderId(resolved.provider);
        baseUrl = profile.url;
        apiKey = profile.apiKey;
    }
    QUrl chat(QStringLiteral("https://api.openai.com/v1/chat/completions"));
    if (!baseUrl.trimmed().isEmpty()) {
        chat = openAiCompatibleChatUrl(baseUrl);
    }
    resolved.modelsUrl = siblingEndpoint(chat, QStringLiteral("/models"));
    resolved.authHeaderName = QByteArrayLiteral("Authorization");
    resolved.authHeaderValue = QByteArrayLiteral("Bearer ")
        + apiKey.trimmed().toUtf8();
    return resolved;
}

QString categoryFor(const NetworkResponse &response)
{
    if (response.error.code == QStringLiteral("network.timeout")) {
        return QStringLiteral("Timeout");
    }
    if (response.statusCode == 401 || response.statusCode == 403) {
        return QStringLiteral("Authentication Failed");
    }
    if (response.statusCode == 402) {
        return QStringLiteral("Insufficient Balance");
    }
    if (response.statusCode == 429) {
        return QStringLiteral("Rate Limit");
    }
    if (response.statusCode >= 500) {
        return QStringLiteral("Server Error");
    }
    if (!response.error.isEmpty()) {
        return QStringLiteral("Connection Failed");
    }
    return response.isSuccess()
        ? QStringLiteral("Success")
        : QStringLiteral("Request Failed");
}

QString serverMessage(const QByteArray &body)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return QString::fromUtf8(body).trimmed();
    }
    const QJsonObject root = document.object();
    const QJsonValue errorValue = root.value(QStringLiteral("error"));
    if (errorValue.isObject()) {
        return errorValue.toObject().value(QStringLiteral("message")).toString();
    }
    return errorValue.toString();
}

ModelApiDiagnosticsResult getEndpoint(
    const ModelApiDiagnosticsRequest &request)
{
    ModelApiDiagnosticsResult result;
    const ResolvedDiagnostics resolved = resolveDiagnostics(request.modelId);
    QUrl endpoint = resolved.modelsUrl;
    if (!request.endpointOverride.trimmed().isEmpty()) {
        endpoint = urlWithDefaultHttps(request.endpointOverride);
    }
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        result.category = QStringLiteral("Unsupported");
        result.message = tr8("模型列表接口地址无效。请检查地址或手动填写模型名称。");
        return result;
    }

    QNetworkRequest networkRequest(endpoint);
    if (!resolved.authHeaderName.isEmpty()
        && !resolved.authHeaderValue.trimmed().isEmpty()) {
        networkRequest.setRawHeader(
            resolved.authHeaderName,
            resolved.authHeaderValue
        );
    }
    if (resolved.anthropic) {
        networkRequest.setRawHeader(
            "anthropic-version",
            QByteArrayLiteral("2023-06-01")
        );
    }
    NetworkRequestOptions options;
    options.timeoutMs = 15000;
    options.globalUseSystemProxy = request.useSystemProxy;
    CancellationSource owned;
    const CancellationToken cancellation = request.cancellation.isValid()
        ? request.cancellation
        : owned.token();
    const NetworkResponse response = createProviderNetworkTransport()->get(
        networkRequest,
        options,
        cancellation
    );
    result.httpStatusCode = response.statusCode;
    result.durationMs = response.durationMs;
    result.rawResponse = response.body;
    result.category = categoryFor(response);
    result.success = response.isSuccess();
    if (!result.success) {
        const QString original = serverMessage(response.body);
        result.message = result.category
            + (original.isEmpty() ? QString() : QStringLiteral("：") + original);
        if (result.message == result.category
            && !response.error.message.trimmed().isEmpty()) {
            result.message += QStringLiteral("：") + response.error.message;
        }
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.success = false;
        result.category = QStringLiteral("Invalid Response");
        result.message = tr8("服务端返回成功，但内容不是 JSON 对象：")
            + parseError.errorString();
        return result;
    }
    result.data = document.object();
    auto appendModels = [&](const QJsonArray &array) {
        for (const QJsonValue &value : array) {
            const QString id = value.isObject()
                ? value.toObject().value(QStringLiteral("id")).toString().trimmed()
                : value.toString().trimmed();
            if (!id.isEmpty() && !result.models.contains(id)) {
                result.models.append(id);
            }
        }
    };
    appendModels(result.data.value(QStringLiteral("data")).toArray());
    appendModels(result.data.value(QStringLiteral("models")).toArray());
    result.message = result.models.isEmpty()
        ? tr8("接口连接成功，但响应中没有识别到模型名称；仍可手动填写。")
        : tr8("获取模型成功，共 ") + QString::number(result.models.size()) + tr8(" 个。");
    return result;
}

ModelApiDiagnosticsResult providerCheck(
    const ModelApiDiagnosticsRequest &request,
    const QString &successMessage,
    bool useFullAdvancedConfiguration)
{
    ModelApiDiagnosticsResult result;
    const QString providerId = modelProvider(request.modelId);
    QSharedPointer<IModelProvider> provider;
    if (providerId == QStringLiteral("deepseek")) {
        provider = createDeepSeekModelProvider(request.useSystemProxy);
    } else if (providerId == QStringLiteral("claude")) {
        provider = createClaudeModelProvider(request.useSystemProxy);
    } else if (providerId == QStringLiteral("openai")
               || providerId == QStringLiteral("custom")
               || providerId.startsWith(QStringLiteral("custom:"))) {
        provider = createOpenAiCompatibleModelProvider(
            providerId,
            request.useSystemProxy
        );
    }
    if (provider.isNull()) {
        result.category = QStringLiteral("Unsupported");
        result.message = tr8("无法识别当前模型对应的服务商。");
        return result;
    }
    CancellationSource owned;
    const CancellationToken cancellation = request.cancellation.isValid()
        ? request.cancellation
        : owned.token();
    ModelRequest modelRequest;
    modelRequest.modelId = request.modelId;
    modelRequest.systemPrompt = tr8("你是接口自检助手。只回复 OK。");
    modelRequest.userPrompt = tr8("请只回复 OK，用于确认当前模型接口可用。");
    modelRequest.stream = false;
    modelRequest.network.timeoutMs = 15000;
    modelRequest.network.globalUseSystemProxy = request.useSystemProxy;
    ModelAdvancedProfile advanced = useFullAdvancedConfiguration
        ? loadModelAdvancedProfile(request.modelId)
        : ModelAdvancedProfile();
    if (!request.modelNameOverride.trimmed().isEmpty()) {
        advanced.enabled = true;
        advanced.parameters.insert(
            QStringLiteral("model"),
            request.modelNameOverride.trimmed()
        );
    }
    if (advanced.enabled) {
        if (advanced.systemPromptOverrideEnabled) {
            for (const ModelSystemPromptPreset &preset : advanced.systemPrompts) {
                if (preset.id == advanced.activeSystemPromptId) {
                    modelRequest.systemPrompt = preset.content;
                    break;
                }
            }
        }
        modelRequest.extra.insert(
            QStringLiteral("vocekit_advanced"),
            modelAdvancedProfileToJson(advanced)
        );
    }
    const ModelResult modelResult = provider->complete(
        modelRequest,
        ModelDeltaCallback(),
        cancellation
    );
    result.success = modelResult.error.isEmpty()
        && !modelResult.text.trimmed().isEmpty();
    result.durationMs = modelResult.durationMs;
    result.httpStatusCode = modelResult.telemetry.httpStatusCode;
    result.rawResponse = modelResult.rawResponse;
    if (result.success) {
        result.category = QStringLiteral("Success");
        result.message = successMessage;
        return result;
    }
    if (result.httpStatusCode == 401 || result.httpStatusCode == 403) {
        result.category = QStringLiteral("Authentication Failed");
    } else if (result.httpStatusCode == 402) {
        result.category = QStringLiteral("Insufficient Balance");
    } else if (result.httpStatusCode == 429) {
        result.category = QStringLiteral("Rate Limit");
    } else if (result.httpStatusCode >= 500) {
        result.category = QStringLiteral("Server Error");
    } else if (modelResult.error.code == QStringLiteral("network.timeout")
               || modelResult.error.code == QStringLiteral("request.timeout")) {
        result.category = QStringLiteral("Timeout");
    } else if (modelResult.error.message.contains(QStringLiteral("认证"))
               || modelResult.error.message.contains(QStringLiteral("API Key"))) {
        result.category = QStringLiteral("Authentication Failed");
    } else {
        result.category = QStringLiteral("Connection Failed");
    }
    result.message = result.category + QStringLiteral("：")
        + modelResult.error.message;
    if (!modelResult.error.detail.trimmed().isEmpty()) {
        result.message += QStringLiteral("\n\n") + modelResult.error.detail;
    }
    return result;
}

} // namespace

ModelApiDiagnosticsResult testModelApiKey(
    const ModelApiDiagnosticsRequest &request)
{
    return providerCheck(
        request,
        tr8("API Key 有效，模型请求成功。"),
        false
    );
}

ModelApiDiagnosticsResult testModelConnection(
    const ModelApiDiagnosticsRequest &request)
{
    return providerCheck(
        request,
        tr8("连接成功，当前模型 API 可以正常响应。"),
        true
    );
}

ModelApiDiagnosticsResult fetchModelApiModels(
    const ModelApiDiagnosticsRequest &request)
{
    return getEndpoint(request);
}
