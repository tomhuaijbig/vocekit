#ifndef VOCEKIT_PROMPT_ACCESS_FACTORY_H
#define VOCEKIT_PROMPT_ACCESS_FACTORY_H

#include "prompt_settings_adapter.h"
#include "prompts_panel.h"

class HubSettingsState;

// 提示词访问对象的装配输入。工厂负责把设置状态转换为页面可用的读写契约。
struct PromptAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
};

struct PromptAccessAssembly
{
    PromptSettingsAccess settings;
    PromptsPanelAccess panel;
};

PromptAccessAssembly createPromptAccessAssembly(
    const PromptAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_PROMPT_ACCESS_FACTORY_H
