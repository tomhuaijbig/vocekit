#include "voice_model_runtime_settings.h"

namespace {

QString s(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString defaultVoiceModelSystemPrompt(const QString &modeId)
{
    if (modeId == QStringLiteral("dictate")) {
        return s("整理语音识别文本，只输出可直接粘贴的结果。");
    }
    if (modeId == QStringLiteral("translate")) {
        return s("翻译为简体中文，只输出翻译结果。");
    }
    if (modeId == QStringLiteral("ask")) {
        return s("基于选中文本回答用户问题。");
    }
    return s("根据选中文本和语音要求完成任务，只输出可直接使用的结果。");
}

VoiceModelRuntimeSettings buildVoiceModelRuntimeSettings(
    const AppSettingsData &settings,
    const QString &modeId,
    const QString &configuredPrompt)
{
    VoiceModelRuntimeSettings runtime;
    runtime.defaultModel = settings.function(modeId).modelId.trimmed();
    runtime.sampling = settings.function(modeId).sampling;
    runtime.systemPrompt = configuredPrompt.trimmed();
    if (runtime.systemPrompt.isEmpty()) {
        runtime.systemPrompt = defaultVoiceModelSystemPrompt(modeId);
    }
    runtime.useSystemProxy = settings.useSystemProxy;
    runtime.dictatePolishEnabled = settings.dictatePolishEnabled;
    return runtime;
}
