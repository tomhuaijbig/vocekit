#ifndef VOCEKIT_FUNCTION_EDITOR_DIALOG_ACCESS_FACTORY_H
#define VOCEKIT_FUNCTION_EDITOR_DIALOG_ACCESS_FACTORY_H

#include "function_editor_dialog.h"

// 功能编辑弹窗的装配输入。工厂负责组合设置、提示词和保存后刷新动作。
struct FunctionEditorDialogAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
};

FunctionEditorDialogAccess createFunctionEditorDialogAccess(
    const FunctionEditorDialogAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_FUNCTION_EDITOR_DIALOG_ACCESS_FACTORY_H
