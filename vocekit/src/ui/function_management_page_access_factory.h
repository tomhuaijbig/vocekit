#ifndef VOCEKIT_FUNCTION_MANAGEMENT_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_FUNCTION_MANAGEMENT_PAGE_ACCESS_FACTORY_H

#include "function_management_page.h"

class HubSettingsState;

// 功能管理页的装配输入。工厂负责生成摘要项并编排编辑、删除后的刷新动作。
struct FunctionManagementPageAccessDependencies
{
    HubSettingsState *settings = nullptr;
    std::function<QString(const QString &, const QString &)> summaryProvider;
    std::function<void()> addFunction;
    std::function<void(
        const QString &,
        const QString &,
        bool,
        const CustomFunctionDef &
    )> editFunction;
    std::function<void()> saveSettings;
};

FunctionManagementPageAccess createFunctionManagementPageAccess(
    const FunctionManagementPageAccessDependencies &dependencies
);

#endif // VOCEKIT_FUNCTION_MANAGEMENT_PAGE_ACCESS_FACTORY_H
