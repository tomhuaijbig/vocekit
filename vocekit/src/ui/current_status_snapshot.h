#ifndef VOCEKIT_CURRENT_STATUS_SNAPSHOT_H
#define VOCEKIT_CURRENT_STATUS_SNAPSHOT_H

#include "../config/app_settings_data.h"

// 主界面状态卡片只消费这份只读快照，不直接访问可变设置对象。
struct CurrentStatusSnapshot
{
    bool trayResident = true;
    bool autoStartEnabled = false;
    bool strongSelectionEnabled = false;
    bool vocabularyEnabled = true;
    QString vocabularyAddMode;
    bool dictatePolishEnabled = false;
    QString speechProvider;
    bool useSystemProxy = false;
    bool floatingBarEnabled = true;
    bool usesDefaultRecordDirectory = true;
    int holdToTalkFunctionCount = 0;
    int longRecordingFunctionCount = 0;
};

CurrentStatusSnapshot buildCurrentStatusSnapshot(
    const AppSettingsData &settings
);

#endif // VOCEKIT_CURRENT_STATUS_SNAPSHOT_H
