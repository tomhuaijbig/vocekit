#ifndef VOCEKIT_MODEL_PROVIDER_H
#define VOCEKIT_MODEL_PROVIDER_H

#include "provider_types.h"

#include "../tasks/cancellation_token.h"

// 大模型适配器统一支持普通结果和流式增量，不直接操作界面。
class IModelProvider
{
public:
    virtual ~IModelProvider()
    {
    }

    virtual QString id() const = 0;
    virtual ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation = CancellationToken()
    ) const = 0;
    virtual ModelResult complete(
        const ModelRequest &request,
        const ModelDeltaCallback &onDelta,
        const CancellationToken &cancellation
    ) = 0;
    virtual void refreshConfiguration()
    {
    }
};

#endif // VOCEKIT_MODEL_PROVIDER_H
