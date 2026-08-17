#ifndef VOCEKIT_SETTINGS_PANEL_ACCESS_FACTORY_H
#define VOCEKIT_SETTINGS_PANEL_ACCESS_FACTORY_H

#include "settings_panel.h"

class HubSettingsState;

// 设置页装配输入。工厂集中提供配置快照、保存和保存后的刷新动作。
struct SettingsPanelAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    std::function<void()> notifySettingsChanged;
    std::function<void(const QString &)> previewFloatingBarStyle;
};

struct SettingsPanelAssembly
{
    SettingsPanelAccess access;
    std::function<void()> onChanged;
};

SettingsPanelAssembly createSettingsPanelAssembly(
    const SettingsPanelAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_SETTINGS_PANEL_ACCESS_FACTORY_H
