#ifndef VOCEKIT_FUNCTION_COMMAND_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_FUNCTION_COMMAND_PAGE_ACCESS_FACTORY_H

#include "function_command_page.h"

// 功能配置页的装配输入。主窗口只提供状态和动作，工厂负责组合页面契约。
struct FunctionCommandPageAccessDependencies
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
    std::function<void(
        const QString &,
        const QString &,
        const CustomFunctionDef &
    )> editCustomFunction;
};

FunctionCommandPageAccess createFunctionCommandPageAccess(
    const FunctionCommandPageAccessDependencies &dependencies
);

#endif // VOCEKIT_FUNCTION_COMMAND_PAGE_ACCESS_FACTORY_H
