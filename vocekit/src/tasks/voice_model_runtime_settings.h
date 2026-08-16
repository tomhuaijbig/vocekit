#ifndef VOCEKIT_VOICE_MODEL_RUNTIME_SETTINGS_H
#define VOCEKIT_VOICE_MODEL_RUNTIME_SETTINGS_H

#include "../config/app_settings_data.h"

#include <QString>

struct VoiceModelRuntimeSettings
{
    QString defaultModel;
    QString systemPrompt;
    ModelSamplingSettings sampling;
    bool useSystemProxy = false;
    bool dictatePolishEnabled = false;
};

QString defaultVoiceModelSystemPrompt(const QString &modeId);

// 从应用设置中提取一次模型处理实际需要的只读字段。
VoiceModelRuntimeSettings buildVoiceModelRuntimeSettings(
    const AppSettingsData &settings,
    const QString &modeId,
    const QString &configuredPrompt
);

#endif // VOCEKIT_VOICE_MODEL_RUNTIME_SETTINGS_H
