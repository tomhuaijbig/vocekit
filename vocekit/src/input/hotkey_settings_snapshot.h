#ifndef VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H
#define VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H

#include "../config/app_settings_data.h"
#include "global_hotkeys.h"

// 将当前设置转换为全局快捷键注册需要的只读快照。
GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings
);

#endif // VOCEKIT_HOTKEY_SETTINGS_SNAPSHOT_H
