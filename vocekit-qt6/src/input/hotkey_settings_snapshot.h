#ifndef VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H
#define VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H

#include "../config/app_settings_data.h"
#include "../domain/function_flow_compiler.h"
#include "global_hotkeys.h"

#include <QSharedPointer>

#include <functional>

using FunctionFlowPlanProvider = std::function<
    QSharedPointer<const FunctionFlowExecutionPlan>(const QString &)
>;

bool functionUsesScreenshotLauncher(
    const FunctionSettings &function,
    const QSharedPointer<const FunctionFlowExecutionPlan> &plan
);

// 将当前设置转换为全局快捷键注册需要的只读快照。
GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings
);
GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings,
    const FunctionFlowPlanProvider &flowPlanProvider
);

#endif // VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H
