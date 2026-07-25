#ifndef VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H
#define VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H

#include "function_editor_dialog.h"

#include <functional>

class HubSettingsState;

// 功能编辑流程的外部动作。协调器负责构造请求，主窗口只提供保存、刷新和弹窗入口。
struct FunctionEditorCoordinatorActions
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
    std::function<bool(
        const FunctionEditorDialogRequest &,
        const FunctionEditorDialogAccess &
    )> openDialog;
};

QString functionEditorSummaryText(
    const QString &id,
    const QString &shortcut,
    const FunctionEditorCoordinatorActions &actions
);

bool runFunctionEditorCoordinator(
    const QString &id,
    const QString &title,
    bool custom,
    const CustomFunctionDef &function,
    const FunctionEditorCoordinatorActions &actions
);

#endif // VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H
