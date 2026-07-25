#ifndef VOCEKIT_COMMAND_CENTER_SHELL_ACCESS_FACTORY_H
#define VOCEKIT_COMMAND_CENTER_SHELL_ACCESS_FACTORY_H

#include "command_center_shell.h"

class HubSettingsState;

// 命令中心导航的装配输入。工厂集中生成内置和自定义功能项，
// 主窗口只提供页面跳转及提示动作。
struct CommandCenterShellAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    std::function<void(const QString &)> openFunction;
    std::function<void(const QString &)> openTool;
    std::function<void()> addFunction;
    std::function<void(const QString &)> searchMissed;
};

CommandCenterShellAccess createCommandCenterShellAccess(
    const CommandCenterShellAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_COMMAND_CENTER_SHELL_ACCESS_FACTORY_H
