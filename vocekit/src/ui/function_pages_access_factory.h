#ifndef VOCEKIT_FUNCTION_PAGES_ACCESS_FACTORY_H
#define VOCEKIT_FUNCTION_PAGES_ACCESS_FACTORY_H

#include "function_command_page_access_factory.h"
#include "function_management_page_access_factory.h"

// 集中装配当前功能页和功能管理页，共享保存、编辑和刷新动作。
struct FunctionPagesAccessDependencies
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
    std::function<QString(const QString &, const QString &)> summaryProvider;
    std::function<void()> addFunction;
    std::function<void(
        const QString &,
        const QString &,
        bool,
        const CustomFunctionDef &
    )> editFunction;
};

struct FunctionPagesAccessAssembly
{
    FunctionCommandPageAccess command;
    FunctionManagementPageAccess management;
};

FunctionPagesAccessAssembly createFunctionPagesAccess(
    const FunctionPagesAccessDependencies &dependencies
);

#endif // VOCEKIT_FUNCTION_PAGES_ACCESS_FACTORY_H
