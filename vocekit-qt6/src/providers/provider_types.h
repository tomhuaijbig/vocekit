#ifndef VOCEKIT_PROVIDER_TYPES_H
#define VOCEKIT_PROVIDER_TYPES_H

#include "../domain/execution_types.h"
#include "../domain/model_sampling_settings.h"
#include "../domain/operation_error.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

struct NetworkRequestOptions
{
    int timeoutMs = 30000;
    QString networkPolicy = QStringLiteral("inherit");
    bool globalUseSystemProxy = false;
};

struct NetworkResponse
{
    ExecutionId executionId;
    int statusCode = 0;
    QByteArray body;
    OperationError error;
    bool cancelled = false;
    qint64 durationMs = -1;

    bool isSuccess() const
    {
        return error.isEmpty()
            && statusCode >= 200
            && statusCode < 300;
    }
};

using StreamDataCallback = std::function<void(const QByteArray &)>;

struct ProviderCheckResult
{
    bool available = false;
    QString message;
    OperationError error;
    qint64 durationMs = -1;
};

struct SpeechRecognitionRequest
{
    ExecutionId executionId;
    QByteArray audioData;
    QString audioPath;
    QString audioFormat = QStringLiteral("wav");
    int sampleRate = 16000;
    QString language = QStringLiteral("follow-windows");
    NetworkRequestOptions network;
};

struct SpeechRecognitionResult
{
    ExecutionId executionId;
    QString text;
    QByteArray rawResponse;
    OperationError error;
    qint64 durationMs = -1;

    bool isSuccess() const
    {
        return error.isEmpty() && !text.trimmed().isEmpty();
    }
};

struct ModelRequest
{
    ExecutionId executionId;
    QString modelId;
    QString systemPrompt;
    QString userPrompt;
    QStringList contextMessages;
    QJsonObject extra;
    ModelSamplingSettings sampling;
    NetworkRequestOptions network;
    bool stream = true;
};

struct ModelTokenUsage
{
    qint64 inputTokens = -1;
    qint64 outputTokens = -1;
    qint64 totalTokens = -1;
    qint64 reasoningTokens = -1;
    qint64 cacheHitTokens = -1;

    bool hasAny() const
    {
        return inputTokens >= 0
            || outputTokens >= 0
            || totalTokens >= 0
            || reasoningTokens >= 0
            || cacheHitTokens >= 0;
    }
};

struct ModelCitation
{
    QString title;
    QString siteName;
    QString url;
    QString snippet;
};

struct ModelRequestTelemetry
{
    QString providerId;
    QString modelId;
    QDateTime requestedAtUtc;
    QJsonObject actualRequest;
    int httpStatusCode = 0;
    ModelTokenUsage usage;
    QString finishReason;
    qint64 firstResponseMs = -1;
    qint64 firstTokenMs = -1;
    qint64 totalDurationMs = -1;
    double estimatedCost = -1.0;
    QString estimatedCostCurrency = QStringLiteral("USD");
    QVector<ModelCitation> citations;
};

struct ModelResult
{
    ExecutionId executionId;
    QString text;
    QByteArray rawResponse;
    OperationError error;
    qint64 durationMs = -1;
    ModelRequestTelemetry telemetry;

    bool isSuccess() const
    {
        return error.isEmpty() && !text.isNull();
    }
};

using ModelDeltaCallback = std::function<void(const QString &)>;

#endif // VOCEKIT_PROVIDER_TYPES_H
