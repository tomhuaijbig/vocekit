#include "model_request_task.h"

#include "../config/app_settings_defaults.h"
#include "../providers/model_catalog.h"
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
    request.systemPrompt = taskRequest.systemPrompt;
    request.userPrompt = taskRequest.userPrompt;
    request.stream = taskRequest.stream;
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
    taskResult.durationMs = result.durationMs >= 0
        ? result.durationMs
        : timer.elapsed();
    taskResult.cancelled =
        cancellation.isCancellationRequested()
        || result.error.code == QStringLiteral("request.cancelled");
    if (!result.error.isEmpty()) {
        taskResult.errorMessage = result.error.message;
    }
    return taskResult;
}
