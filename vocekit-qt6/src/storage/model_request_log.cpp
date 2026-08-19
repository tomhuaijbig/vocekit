#include "model_request_log.h"

#include "../config/app_paths.h"
#include "../file_utils.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {

bool isSensitiveKey(QString key)
{
    key = key.trimmed().toLower();
    return key == QStringLiteral("authorization")
        || key == QStringLiteral("api_key")
        || key == QStringLiteral("apikey")
        || key == QStringLiteral("x-api-key")
        || key == QStringLiteral("access_token")
        || key == QStringLiteral("refresh_token")
        || key == QStringLiteral("password")
        || key == QStringLiteral("secret")
        || key.endsWith(QStringLiteral("_api_key"))
        || key.endsWith(QStringLiteral("_secret"));
}

QJsonValue redactValue(const QString &key, const QJsonValue &value)
{
    if (isSensitiveKey(key)) {
        return QStringLiteral("***REDACTED***");
    }
    if (value.isObject()) {
        return redactedModelRequestJson(value.toObject());
    }
    if (value.isArray()) {
        QJsonArray array;
        for (const QJsonValue &item : value.toArray()) {
            array.append(redactValue(QString(), item));
        }
        return array;
    }
    return value;
}

QJsonObject usageJson(const ModelTokenUsage &usage)
{
    QJsonObject object;
    if (usage.inputTokens >= 0) {
        object.insert(QStringLiteral("input_tokens"), double(usage.inputTokens));
    }
    if (usage.outputTokens >= 0) {
        object.insert(QStringLiteral("output_tokens"), double(usage.outputTokens));
    }
    if (usage.totalTokens >= 0) {
        object.insert(QStringLiteral("total_tokens"), double(usage.totalTokens));
    }
    if (usage.reasoningTokens >= 0) {
        object.insert(QStringLiteral("reasoning_tokens"), double(usage.reasoningTokens));
    }
    if (usage.cacheHitTokens >= 0) {
        object.insert(QStringLiteral("cache_hit_tokens"), double(usage.cacheHitTokens));
    }
    return object;
}

QJsonObject requestMetadataJson(const QJsonObject &request)
{
    QJsonObject metadata;
    QJsonArray fieldNames;
    for (const QString &key : request.keys()) {
        fieldNames.append(key);
    }
    metadata.insert(QStringLiteral("field_names"), fieldNames);

    const QStringList safeScalarKeys = QStringList()
        << QStringLiteral("model")
        << QStringLiteral("temperature")
        << QStringLiteral("top_p")
        << QStringLiteral("max_tokens")
        << QStringLiteral("max_output_tokens")
        << QStringLiteral("frequency_penalty")
        << QStringLiteral("presence_penalty")
        << QStringLiteral("reasoning_effort")
        << QStringLiteral("stream")
        << QStringLiteral("seed")
        << QStringLiteral("n")
        << QStringLiteral("logprobs")
        << QStringLiteral("top_logprobs")
        << QStringLiteral("parallel_tool_calls")
        << QStringLiteral("service_tier");
    QJsonObject parameters;
    for (const QString &key : safeScalarKeys) {
        const QJsonValue value = request.value(key);
        if (value.isBool() || value.isDouble() || value.isString()) {
            parameters.insert(key, value);
        }
    }
    if (!parameters.isEmpty()) {
        metadata.insert(QStringLiteral("parameters"), parameters);
    }

    const QJsonValue messages = request.value(QStringLiteral("messages"));
    if (messages.isArray()) {
        metadata.insert(
            QStringLiteral("message_count"),
            messages.toArray().size()
        );
    }
    const QJsonValue input = request.value(QStringLiteral("input"));
    if (!input.isUndefined()) {
        metadata.insert(QStringLiteral("input_present"), true);
        if (input.isArray()) {
            metadata.insert(
                QStringLiteral("input_item_count"),
                input.toArray().size()
            );
        }
    }
    const QJsonValue tools = request.value(QStringLiteral("tools"));
    if (tools.isArray()) {
        metadata.insert(QStringLiteral("tool_count"), tools.toArray().size());
    }
    const QJsonObject responseFormat = request
        .value(QStringLiteral("response_format"))
        .toObject();
    const QString responseFormatType = responseFormat
        .value(QStringLiteral("type"))
        .toString()
        .trimmed();
    if (!responseFormatType.isEmpty()) {
        metadata.insert(
            QStringLiteral("response_format_type"),
            responseFormatType
        );
    }
    return metadata;
}

} // namespace

QString modelRequestLogPath()
{
    return QDir(appBasePath()).filePath(QStringLiteral("logs/model-requests.jsonl"));
}

QJsonObject redactedModelRequestJson(const QJsonObject &object)
{
    QJsonObject result;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        result.insert(it.key(), redactValue(it.key(), it.value()));
    }
    return result;
}

QString redactedModelLogText(const QString &text)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        text.trimmed().toUtf8(),
        &parseError
    );
    if (parseError.error == QJsonParseError::NoError) {
        const QJsonValue safe = document.isObject()
            ? QJsonValue(redactedModelRequestJson(document.object()))
            : redactValue(QString(), QJsonValue(document.array()));
        if (safe.isObject()) {
            return QString::fromUtf8(
                QJsonDocument(safe.toObject()).toJson(QJsonDocument::Compact)
            );
        }
        if (safe.isArray()) {
            return QString::fromUtf8(
                QJsonDocument(safe.toArray()).toJson(QJsonDocument::Compact)
            );
        }
    }

    QString result = text;
    result.replace(
        QRegularExpression(
            QStringLiteral("(Bearer\\s+)[A-Za-z0-9._~+/=-]+"),
            QRegularExpression::CaseInsensitiveOption
        ),
        QStringLiteral("\\1***REDACTED***")
    );
    result.replace(
        QRegularExpression(
            QStringLiteral(
                "(\\\"(?:api[_-]?key|authorization|access_token|refresh_token|password|secret)\\\"\\s*:\\s*\\\")[^\\\"]*(\\\")"
            ),
            QRegularExpression::CaseInsensitiveOption
        ),
        QStringLiteral("\\1***REDACTED***\\2")
    );
    return result;
}

QJsonObject modelRequestLogEntry(
    const ModelResult &result,
    bool includeRequestResponseContent)
{
    QJsonObject entry;
    const ModelRequestTelemetry &telemetry = result.telemetry;
    entry.insert(
        QStringLiteral("request_time"),
        (telemetry.requestedAtUtc.isValid()
            ? telemetry.requestedAtUtc
            : QDateTime::currentDateTimeUtc()).toString(Qt::ISODateWithMs)
    );
    entry.insert(QStringLiteral("provider"), telemetry.providerId);
    entry.insert(QStringLiteral("model"), telemetry.modelId);
    entry.insert(
        QStringLiteral("request_metadata"),
        requestMetadataJson(telemetry.actualRequest)
    );
    if (includeRequestResponseContent) {
        entry.insert(
            QStringLiteral("actual_request"),
            redactedModelRequestJson(telemetry.actualRequest)
        );
    }
    entry.insert(QStringLiteral("http_status"), telemetry.httpStatusCode);
    entry.insert(QStringLiteral("duration_ms"), double(result.durationMs));
    if (telemetry.firstResponseMs >= 0) {
        entry.insert(QStringLiteral("first_response_ms"), double(telemetry.firstResponseMs));
    }
    if (telemetry.firstTokenMs >= 0) {
        entry.insert(QStringLiteral("first_token_ms"), double(telemetry.firstTokenMs));
    }
    if (telemetry.usage.hasAny()) {
        entry.insert(QStringLiteral("token_usage"), usageJson(telemetry.usage));
    }
    if (!telemetry.finishReason.trimmed().isEmpty()) {
        entry.insert(QStringLiteral("finish_reason"), telemetry.finishReason);
    }
    if (telemetry.estimatedCost >= 0.0) {
        entry.insert(QStringLiteral("estimated_cost"), telemetry.estimatedCost);
        entry.insert(
            QStringLiteral("estimated_cost_currency"),
            telemetry.estimatedCostCurrency
        );
        entry.insert(QStringLiteral("cost_is_estimate"), true);
    }
    entry.insert(QStringLiteral("success"), result.error.isEmpty());
    if (!result.error.isEmpty()) {
        QJsonObject error;
        error.insert(QStringLiteral("code"), result.error.code);
        if (includeRequestResponseContent) {
            error.insert(
                QStringLiteral("message"),
                redactedModelLogText(result.error.message)
            );
            error.insert(
                QStringLiteral("detail"),
                redactedModelLogText(result.error.detail)
            );
        } else {
            error.insert(
                QStringLiteral("message_available"),
                !result.error.message.trimmed().isEmpty()
            );
            error.insert(
                QStringLiteral("detail_available"),
                !result.error.detail.trimmed().isEmpty()
            );
        }
        entry.insert(QStringLiteral("error"), error);
    }
    if (!result.rawResponse.isEmpty()) {
        entry.insert(
            QStringLiteral("raw_response_bytes"),
            double(result.rawResponse.size())
        );
        if (includeRequestResponseContent) {
            QJsonParseError parseError;
            const QJsonDocument raw = QJsonDocument::fromJson(
                result.rawResponse,
                &parseError
            );
            if (parseError.error == QJsonParseError::NoError) {
                const QJsonValue safeResponse = raw.isObject()
                    ? QJsonValue(redactedModelRequestJson(raw.object()))
                    : redactValue(QString(), QJsonValue(raw.array()));
                entry.insert(
                    QStringLiteral("raw_response"),
                    safeResponse
                );
            } else {
                entry.insert(
                    QStringLiteral("raw_response_text"),
                    redactedModelLogText(QString::fromUtf8(result.rawResponse))
                );
            }
        }
    }
    entry.insert(
        QStringLiteral("content_logging_enabled"),
        includeRequestResponseContent
    );
    return entry;
}

bool appendModelRequestLog(
    const ModelResult &result,
    bool includeRequestResponseContent)
{
    const QString path = modelRequestLogPath();
    if (!ensureParentDirectoryForFile(path)) {
        return false;
    }
    const QJsonObject entry = modelRequestLogEntry(
        result,
        includeRequestResponseContent
    );

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    QByteArray line = QJsonDocument(entry).toJson(QJsonDocument::Compact);
    line.append('\n');
    return file.write(line) == line.size();
}

QVector<QJsonObject> recentModelRequestLogs(int maximumCount)
{
    QVector<QJsonObject> result;
    QFile file(modelRequestLogPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }
    const QList<QByteArray> lines = file.readAll().split('\n');
    const int begin = qMax(0, lines.size() - qMax(1, maximumCount) - 1);
    for (int i = begin; i < lines.size(); ++i) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(lines.at(i), &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            result.append(document.object());
        }
    }
    return result;
}
