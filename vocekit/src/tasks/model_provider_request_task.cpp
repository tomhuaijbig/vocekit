#include "model_request_task.h"

#include "../config/app_settings_defaults.h"
#include "../providers/built_in_provider_factory.h"
#include "../providers/provider_configuration.h"

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &onDelta
)
{
    const QSharedPointer<IModelProvider> provider =
        createBuiltInModelProvider(
            modelProvider(request.modelId),
            request.useSystemProxy
        );
    return runModelRequestTask(request, provider, onDelta);
}

bool isModelProviderAvailableForTask(const QString &modelId)
{
    return isModelProviderConfiguredFromStore(modelId);
}
