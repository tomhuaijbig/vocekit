#include "model_request_task.h"

#include "../config/app_settings_defaults.h"
#include "../providers/built_in_provider_factory.h"
#include "../providers/model_catalog.h"
#include "../providers/provider_configuration.h"

namespace {

QString normalizedTaskModelId(const QString &modelId)
{
    return normalizeModelId(
        modelId,
        defaultModelForFunction(QString())
    );
}

ModelProviderRequestTaskDependencies defaultDependencies()
{
    ModelProviderRequestTaskDependencies dependencies;
    dependencies.createProvider = [](
        const QString &providerKey,
        bool useSystemProxy) {
        return createBuiltInModelProvider(
            providerKey,
            useSystemProxy
        );
    };
    dependencies.isProviderConfigured = [](
        const QString &canonicalModelId) {
        return isModelProviderConfiguredFromStore(
            canonicalModelId
        );
    };
    return dependencies;
}

} // namespace

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &onDelta
)
{
    return runModelProviderRequestTask(
        request,
        defaultDependencies(),
        onDelta
    );
}

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelProviderRequestTaskDependencies &dependencies,
    const ModelDeltaCallback &onDelta
)
{
    ModelRequestTaskRequest normalizedRequest = request;
    normalizedRequest.modelId = normalizedTaskModelId(
        request.modelId
    );
    QSharedPointer<IModelProvider> provider;
    if (dependencies.createProvider) {
        provider = dependencies.createProvider(
            modelProvider(normalizedRequest.modelId),
            normalizedRequest.useSystemProxy
        );
    }
    return runModelRequestTask(
        normalizedRequest,
        provider,
        onDelta
    );
}

bool isModelProviderAvailableForTask(const QString &modelId)
{
    return isModelProviderAvailableForTask(
        modelId,
        defaultDependencies()
    );
}

bool isModelProviderAvailableForTask(
    const QString &modelId,
    const ModelProviderRequestTaskDependencies &dependencies
)
{
    return dependencies.isProviderConfigured
        && dependencies.isProviderConfigured(
            normalizedTaskModelId(modelId)
        );
}
