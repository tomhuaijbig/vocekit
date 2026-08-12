#ifndef VOCEKIT_DIAGNOSTICS_SETTINGS_SNAPSHOT_H
#define VOCEKIT_DIAGNOSTICS_SETTINGS_SNAPSHOT_H

#include "../config/app_settings_data.h"

// 测试工具页运行时只读取这些状态，不直接访问应用设置对象。
struct DiagnosticsSettingsSnapshot
{
    bool useSystemProxy = false;
    QString ocrEngine = QStringLiteral("automatic");
    QString windowsSpeechLanguage = QStringLiteral("follow-windows");
    int ocrTimeoutMs = 45000;
    bool floatingBarEnabled = true;
    int dictateFloatingBarSeconds = 2;
};

DiagnosticsSettingsSnapshot buildDiagnosticsSettingsSnapshot(
    const AppSettingsData &settings
);

#endif // VOCEKIT_DIAGNOSTICS_SETTINGS_SNAPSHOT_H
