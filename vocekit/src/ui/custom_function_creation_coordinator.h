#ifndef VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H
#define VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H

#include "../domain/app_legacy_types.h"

#include <functional>

class HubSettingsState;

// 新增自定义功能的事务动作。编辑取消时会撤销刚创建的临时功能。
struct CustomFunctionCreationActions
{
    HubSettingsState *settings = nullptr;
    std::function<void()> saveSettings;
    std::function<bool(const CustomFunctionDef &)> editFunction;
};

bool createAndEditCustomFunction(const CustomFunctionCreationActions &actions);

#endif // VOCEKIT_CUSTOM_FUNCTION_CREATION_COORDINATOR_H
