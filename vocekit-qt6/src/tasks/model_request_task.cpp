#include "model_request_task.h"

#include "../config/app_settings_defaults.h"
#include "../config/model_advanced_settings.h"
#include "../providers/model_catalog.h"
#include "../storage/model_request_log.h"
#include "cancellation_token.h"

#include <QCryptographicHash>
#include <QElapsedTimer>

namespace {

QString promptVersionFor(const QString &prompt)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            prompt.toUtf8(),
            QCryptographicHash::Sha256
        ).toHex().left(12)
    );
}

} // namespace

ModelRequestTaskResult runModelRequestTask(
    const ModelRequestTaskRequest &taskRequest,
    const QSharedPointer<IModelProvider> &provider,
    const ModelDeltaCallback &onDelta
)
{
    ModelRequestTaskResult taskResult;
    taskResult.promptVersion = promptVersionFor(taskRequest.systemPrompt);
    taskResult.executionId = taskRequest.cancellation.executionId();

    if (provider.isNull()) {
        taskResult.errorMessage = QStringLiteral("大模型接口不可用。");
        return taskResult;
    }

    QElapsedTimer timer;
    timer.start();

    CancellationSource ownedCancellation;
    const CancellationToken cancellation =
        taskRequest.cancellation.isValid()
            ? taskRequest.cancellation
            : ownedCancellation.token();
    ModelRequest request;
    request.modelId = normalizeModelId(
        taskRequest.modelId,
        defaultModelForFunction(QString())
    );
    const ModelAdvancedProfile advanced =
        loadModelAdvancedProfile(request.modelId);
    request.systemPrompt = taskRequest.systemPrompt;
    if (advanced.enabled && advanced.systemPromptOverrideEnabled) {
        for (const ModelSystemPromptPreset &preset : advanced.systemPrompts) {
            if (preset.id == advanced.activeSystemPromptId) {
                request.systemPrompt = preset.content;
                break;
            }
        }
    }
    taskResult.promptVersion = promptVersionFor(request.systemPrompt);
    request.userPrompt = taskRequest.userPrompt;
    request.sampling =
        normalizeModelSamplingSettings(taskRequest.sampling);
    request.stream = taskRequest.stream;
    if (advanced.enabled) {
        request.extra.insert(
            QStringLiteral("vocekit_advanced"),
            modelAdvancedProfileToJson(advanced)
        );
        const QJsonValue stream = advanced.parameters.value(
            QStringLiteral("stream")
        );
        if (stream.isBool()) {
            request.stream = stream.toBool();
        }
        const QJsonValue rawStream = advanced.rawJson.value(
            QStringLiteral("stream")
        );
        if (rawStream.isBool()) {
            request.stream = rawStream.toBool();
        }
    }
    request.network.globalUseSystemProxy = taskRequest.useSystemProxy;
    request.network.networkPolicy = taskRequest.networkPolicy;
    request.executionId = cancellation.executionId();
    taskResult.executionId = request.executionId;

    const ModelResult result = provider->complete(
        request,
        onDelta,
        cancellation
    );

    taskResult.text = result.text;
    taskResult.rawResponse = result.rawResponse;
    taskResult.durationMs = result.durationMs >= 0
        ? result.durationMs
        : timer.elapsed();
    taskResult.telemetry = result.telemetry;
    taskResult.telemetry.totalDurationMs = taskResult.durationMs;
    if (advanced.enabled && taskResult.telemetry.usage.hasAny()) {
        double estimate = 0.0;
        bool hasPrice = false;
        if (advanced.inputPricePerMillion >= 0.0
            && taskResult.telemetry.usage.inputTokens >= 0) {
            estimate += advanced.inputPricePerMillion
                * double(taskResult.telemetry.usage.inputTokens)
                / 1000000.0;
            hasPrice = true;
        }
        if (advanced.outputPricePerMillion >= 0.0
            && taskResult.telemetry.usage.outputTokens >= 0) {
            estimate += advanced.outputPricePerMillion
                * double(taskResult.telemetry.usage.outputTokens)
                / 1000000.0;
            hasPrice = true;
        }
        if (advanced.reasoningPricePerMillion >= 0.0
            && taskResult.telemetry.usage.reasoningTokens >= 0) {
            estimate += advanced.reasoningPricePerMillion
                * double(taskResult.telemetry.usage.reasoningTokens)
                / 1000000.0;
            hasPrice = true;
        }
        if (hasPrice) {
            taskResult.telemetry.estimatedCost = estimate;
        }
    }
    taskResult.cancelled =
        cancellation.isCancellationRequested()
        || result.error.code == QStringLiteral("request.cancelled");
    if (!result.error.isEmpty()) {
        taskResult.errorMessage = result.error.message;
    }
    ModelResult loggedResult = result;
    loggedResult.durationMs = taskResult.durationMs;
    loggedResult.telemetry = taskResult.telemetry;
    appendModelRequestLog(loggedResult);
    return taskResult;
}
