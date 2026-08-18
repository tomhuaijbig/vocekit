#ifndef VOCEKIT_MODEL_REQUEST_TASK_H
#define VOCEKIT_MODEL_REQUEST_TASK_H

#include "../providers/model_provider.h"

#include <QSharedPointer>

#include <functional>

struct ModelRequestTaskRequest
{
    QString modelId;
    QString systemPrompt;
    QString userPrompt;
    ModelSamplingSettings sampling;
    bool stream = false;
    bool useSystemProxy = false;
    QString networkPolicy = QStringLiteral("inherit");
    CancellationToken cancellation;
};

struct ModelRequestTaskResult
{
    QString text;
    QByteArray rawResponse;
    QString errorMessage;
    qint64 durationMs = -1;
    QString promptVersion;
    ExecutionId executionId;
    ModelRequestTelemetry telemetry;
    bool cancelled = false;
};

struct ModelProviderRequestTaskDependencies
{
    std::function<QSharedPointer<IModelProvider>(
        const QString &providerKey,
        bool useSystemProxy
    )> createProvider;
    std::function<bool(
        const QString &canonicalModelId
    )> isProviderConfigured;
};

// ModelRequestTask 负责执行单次大模型请求，并把耗时、提示词版本和错误文本统一返回。
// UI 层只关心结果，不需要知道 ModelRequest、CancellationSource 和 Provider 的组装细节。
ModelRequestTaskResult runModelRequestTask(
    const ModelRequestTaskRequest &request,
    const QSharedPointer<IModelProvider> &provider,
    const ModelDeltaCallback &onDelta = ModelDeltaCallback()
);

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &onDelta = ModelDeltaCallback()
);

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelProviderRequestTaskDependencies &dependencies,
    const ModelDeltaCallback &onDelta = ModelDeltaCallback()
);

bool isModelProviderAvailableForTask(const QString &modelId);

bool isModelProviderAvailableForTask(
    const QString &modelId,
    const ModelProviderRequestTaskDependencies &dependencies
);

#endif // VOCEKIT_MODEL_REQUEST_TASK_H
