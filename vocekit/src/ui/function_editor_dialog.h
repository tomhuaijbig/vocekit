#ifndef VOCEKIT_FUNCTION_EDITOR_DIALOG_H
#define VOCEKIT_FUNCTION_EDITOR_DIALOG_H

#include "../domain/app_legacy_types.h"
#include "prompt_settings_adapter.h"

#include <QString>

#include <functional>

class HubSettingsState;
class QWidget;

// 功能编辑器只接收当前功能快照和保存入口，不依赖主窗口的页面实现。
struct FunctionEditorDialogRequest
{
    QString id;
    QString title;
    bool custom = false;
    CustomFunctionDef function;
    QString summaryText;
};

struct FunctionEditorDialogAccess
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
};

bool runFunctionEditorDialog(
    const FunctionEditorDialogRequest &request,
    const FunctionEditorDialogAccess &access,
    QWidget *parent = nullptr
);

#endif // VOCEKIT_FUNCTION_EDITOR_DIALOG_H
