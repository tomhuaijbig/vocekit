#ifndef VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H
#define VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H

#include "../domain/app_legacy_types.h"
#include "function_flow_settings_access.h"

#include <QString>

#include <functional>

class HubSettingsState;

// 创建自定义功能并立即持久化，返回新功能编号供内置功能页打开。
struct CustomFunctionCreationActions
{
    HubSettingsState *settings = nullptr;
    FunctionFlowSettingsAccess flows;
};

QString createCustomFunction(
    const CustomFunctionCreationActions &actions,
    OperationError *error = nullptr
);

#endif // VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H
