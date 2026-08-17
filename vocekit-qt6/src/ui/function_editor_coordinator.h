#ifndef VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H
#define VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H

#include "prompt_settings_adapter.h"

#include <functional>

class HubSettingsState;

// 为功能管理摘要提供只读数据，不再创建独立编辑窗口。
struct FunctionEditorCoordinatorActions
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
};

QString functionEditorSummaryText(
    const QString &id,
    const QString &shortcut,
    const FunctionEditorCoordinatorActions &actions
);

#endif // VOCEKIT_FUNCTION_EDITOR_COORDINATOR_H
