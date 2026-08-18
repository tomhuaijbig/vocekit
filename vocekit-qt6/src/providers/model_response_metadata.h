#ifndef VOCEKIT_MODEL_RESPONSE_METADATA_H
#define VOCEKIT_MODEL_RESPONSE_METADATA_H

#include "provider_types.h"

#include <QJsonArray>
#include <QJsonObject>

inline qint64 jsonInteger(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? qint64(value.toDouble()) : -1;
}

inline void appendCitationValue(
    const QJsonValue &value,
    ModelRequestTelemetry *telemetry
);

inline void updateModelUsageFromJson(
    const QJsonObject &root,
    ModelRequestTelemetry *telemetry)
{
    if (!telemetry) {
        return;
    }
    const QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    if (usage.isEmpty()) {
        return;
    }
    auto firstAvailable = [&](const QString &first, const QString &second) {
        const qint64 value = jsonInteger(usage, first);
        return value >= 0 ? value : jsonInteger(usage, second);
    };
    const qint64 input = firstAvailable(
        QStringLiteral("input_tokens"),
        QStringLiteral("prompt_tokens")
    );
    const qint64 output = firstAvailable(
        QStringLiteral("output_tokens"),
        QStringLiteral("completion_tokens")
    );
    const qint64 total = jsonInteger(usage, QStringLiteral("total_tokens"));
    if (input >= 0) {
        telemetry->usage.inputTokens = input;
    }
    if (output >= 0) {
        telemetry->usage.outputTokens = output;
    }
    if (total >= 0) {
        telemetry->usage.totalTokens = total;
    } else if (telemetry->usage.inputTokens >= 0
               && telemetry->usage.outputTokens >= 0) {
        telemetry->usage.totalTokens = telemetry->usage.inputTokens
            + telemetry->usage.outputTokens;
    }

    const QJsonObject completionDetails = usage
        .value(QStringLiteral("completion_tokens_details"))
        .toObject();
    const QJsonObject outputDetails = usage
        .value(QStringLiteral("output_tokens_details"))
        .toObject();
    qint64 reasoning = jsonInteger(
        completionDetails,
        QStringLiteral("reasoning_tokens")
    );
    if (reasoning < 0) {
        reasoning = jsonInteger(outputDetails, QStringLiteral("reasoning_tokens"));
    }
    if (reasoning >= 0) {
        telemetry->usage.reasoningTokens = reasoning;
    }
    const QJsonObject promptDetails = usage
        .value(QStringLiteral("prompt_tokens_details"))
        .toObject();
    qint64 cache = jsonInteger(promptDetails, QStringLiteral("cached_tokens"));
    if (cache < 0) {
        cache = jsonInteger(
            usage.value(QStringLiteral("input_tokens_details")).toObject(),
            QStringLiteral("cached_tokens")
        );
    }
    if (cache < 0) {
        cache = jsonInteger(usage, QStringLiteral("cache_read_input_tokens"));
    }
    if (cache < 0) {
        cache = jsonInteger(usage, QStringLiteral("cached_tokens"));
    }
    if (cache >= 0) {
        telemetry->usage.cacheHitTokens = cache;
    }
}

inline void appendStructuredCitations(
    const QJsonValue &value,
    ModelRequestTelemetry *telemetry,
    int depth = 0)
{
    if (!telemetry || depth > 8) {
        return;
    }
    if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            appendStructuredCitations(item, telemetry, depth + 1);
        }
        return;
    }
    if (!value.isObject()) {
        return;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if ((it.key() == QStringLiteral("citations")
             || it.key() == QStringLiteral("sources")
             || it.key() == QStringLiteral("annotations"))
            && it.value().isArray()) {
            for (const QJsonValue &source : it.value().toArray()) {
                appendCitationValue(source, telemetry);
            }
        }
        appendStructuredCitations(it.value(), telemetry, depth + 1);
    }
}

inline void appendCitationValue(
    const QJsonValue &value,
    ModelRequestTelemetry *telemetry)
{
    if (!telemetry || !value.isObject()) {
        return;
    }
    const QJsonObject object = value.toObject();
    const QJsonObject citation = object.value(QStringLiteral("url_citation")).isObject()
        ? object.value(QStringLiteral("url_citation")).toObject()
        : object;
    ModelCitation source;
    source.title = citation.value(QStringLiteral("title")).toString();
    source.siteName = citation.value(QStringLiteral("site_name")).toString();
    source.url = citation.value(QStringLiteral("url")).toString();
    source.snippet = citation.value(QStringLiteral("snippet")).toString();
    if (source.snippet.isEmpty()) {
        source.snippet = citation.value(QStringLiteral("description")).toString();
    }
    if (source.url.trimmed().isEmpty()) {
        return;
    }
    for (const ModelCitation &existing : telemetry->citations) {
        if (existing.url == source.url) {
            return;
        }
    }
    telemetry->citations.append(source);
}

inline void updateModelMetadataFromJson(
    const QJsonObject &root,
    ModelRequestTelemetry *telemetry)
{
    if (!telemetry) {
        return;
    }
    updateModelUsageFromJson(root, telemetry);
    const QJsonObject messageEnvelope = root
        .value(QStringLiteral("message"))
        .toObject();
    if (!messageEnvelope.isEmpty()) {
        updateModelUsageFromJson(messageEnvelope, telemetry);
        const QString messageStop = messageEnvelope
            .value(QStringLiteral("stop_reason"))
            .toString();
        if (!messageStop.isEmpty()) {
            telemetry->finishReason = messageStop;
        }
    }

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject choice = choices.first().toObject();
        const QString finish = choice.value(QStringLiteral("finish_reason")).toString();
        if (!finish.isEmpty()) {
            telemetry->finishReason = finish;
        }
        const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
        for (const QJsonValue &annotation :
             message.value(QStringLiteral("annotations")).toArray()) {
            appendCitationValue(annotation, telemetry);
        }
    }
    const QString stopReason = root.value(QStringLiteral("stop_reason")).toString();
    if (!stopReason.isEmpty()) {
        telemetry->finishReason = stopReason;
    }
    const QString deltaStopReason = root
        .value(QStringLiteral("delta"))
        .toObject()
        .value(QStringLiteral("stop_reason"))
        .toString();
    if (!deltaStopReason.isEmpty()) {
        telemetry->finishReason = deltaStopReason;
    }
    appendStructuredCitations(root, telemetry);
}

#endif // VOCEKIT_MODEL_RESPONSE_METADATA_H
