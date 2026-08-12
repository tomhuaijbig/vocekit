#include "app_settings_defaults.h"

namespace {

QString trText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString defaultModelForFunction(const QString &id)
{
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("deepseek-v4-pro");
    }
    return QStringLiteral("deepseek-v4-flash");
}

QString modelProvider(const QString &model)
{
    if (model.startsWith(QStringLiteral("openai:"))) {
        return QStringLiteral("openai");
    }
    if (model.startsWith(QStringLiteral("claude:"))) {
        return QStringLiteral("claude");
    }
    if (model.startsWith(QStringLiteral("custom:"))) {
        return QStringLiteral("custom");
    }
    return QStringLiteral("deepseek");
}

QString providerModelId(const QString &model)
{
    const int index = model.indexOf(QStringLiteral(":"));
    return index > 0 ? model.mid(index + 1) : model;
}

QString outputModeAutoWrite()
{
    return QStringLiteral("autoWrite");
}

QString outputModePopup()
{
    return QStringLiteral("resultPopup");
}

QString outputModeScreenshotPanel()
{
    return QStringLiteral("screenshotPanel");
}

QString normalizeOutputMode(const QString &value, const QString &fallback)
{
    if (value == outputModeAutoWrite()
        || value == outputModePopup()
        || value == outputModeScreenshotPanel()) {
        return value;
    }
    return fallback.isEmpty() ? outputModePopup() : fallback;
}

QString defaultOutputModeForFunction(const QString &id)
{
    if (id == QStringLiteral("dictate")) {
        return outputModeAutoWrite();
    }
    return outputModePopup();
}

QString outputModeTitle(const QString &mode)
{
    const QString normalized = normalizeOutputMode(mode);
    if (normalized == outputModeAutoWrite()) {
        return trText("自动写入");
    }
    if (normalized == outputModeScreenshotPanel()) {
        return trText("截图对照窗");
    }
    return trText("结果小框");
}

QString floatingBarStyleStatusPill()
{
    return QStringLiteral("statusPill");
}

QString floatingBarStyleLiveTranscriptCard()
{
    return QStringLiteral("liveTranscriptCard");
}

QString floatingBarStyleInherit()
{
    return QStringLiteral("inherit");
}

QString normalizeGlobalFloatingBarStyle(const QString &value)
{
    const QString normalized = value.trimmed();
    return normalized == floatingBarStyleLiveTranscriptCard()
        ? floatingBarStyleLiveTranscriptCard()
        : floatingBarStyleStatusPill();
}

QString normalizeFunctionFloatingBarStyle(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == floatingBarStyleStatusPill()
        || normalized == floatingBarStyleLiveTranscriptCard()) {
        return normalized;
    }
    return floatingBarStyleInherit();
}

QString resolveFloatingBarStyle(
    const QString &overrideValue,
    const QString &globalValue)
{
    const QString normalizedOverride =
        normalizeFunctionFloatingBarStyle(overrideValue);
    return normalizedOverride == floatingBarStyleInherit()
        ? normalizeGlobalFloatingBarStyle(globalValue)
        : normalizedOverride;
}

QString floatingBarStyleTitle(const QString &value, bool allowInherit)
{
    if (allowInherit
        && normalizeFunctionFloatingBarStyle(value)
            == floatingBarStyleInherit()) {
        return trText("跟随全局");
    }
    return normalizeGlobalFloatingBarStyle(value)
            == floatingBarStyleLiveTranscriptCard()
        ? trText("实时文字卡片")
        : trText("状态胶囊");
}

QString resultTemplateSimple()
{
    return QStringLiteral("simple");
}

QString resultTemplateDetail()
{
    return QStringLiteral("detail");
}

QString resultTemplateCompare()
{
    return QStringLiteral("compare");
}

QString resultTemplateOutputOnly()
{
    return QStringLiteral("outputOnly");
}

QString normalizeResultTemplate(const QString &value, const QString &fallback)
{
    if (value == resultTemplateSimple()
        || value == resultTemplateDetail()
        || value == resultTemplateCompare()
        || value == resultTemplateOutputOnly()) {
        return value;
    }
    return fallback.isEmpty() ? resultTemplateSimple() : fallback;
}

QString resultTemplateTitle(const QString &value)
{
    const QString normalized = normalizeResultTemplate(value);
    if (normalized == resultTemplateDetail()) {
        return trText("详细");
    }
    if (normalized == resultTemplateCompare()) {
        return trText("对照原文");
    }
    if (normalized == resultTemplateOutputOnly()) {
        return trText("仅输出结果");
    }
    return trText("简洁");
}

QString vocabularyAddModeAi()
{
    return QStringLiteral("ai");
}

QString vocabularyAddModeAsk()
{
    return QStringLiteral("ask");
}

QString vocabularyAddModeManual()
{
    return QStringLiteral("manual");
}

QString normalizeVocabularyAddMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == vocabularyAddModeAi()
        || mode == vocabularyAddModeAsk()
        || mode == vocabularyAddModeManual()) {
        return mode;
    }
    return vocabularyAddModeAsk();
}

QString vocabularyAddModeTitle(const QString &mode)
{
    const QString normalized = normalizeVocabularyAddMode(mode);
    if (normalized == vocabularyAddModeAi()) {
        return trText("自动使用 AI");
    }
    if (normalized == vocabularyAddModeManual()) {
        return trText("不使用 AI");
    }
    return trText("每次询问");
}

bool defaultUseSelectionForFunction(const QString &id)
{
    return id == QStringLiteral("translate") || id == QStringLiteral("ask");
}

bool defaultUseVoiceForFunction(const QString &id)
{
    return id != QStringLiteral("translate");
}

QString defaultPromptIdForFunction(const QString &id)
{
    if (id == QStringLiteral("translate")) {
        return QStringLiteral("translate");
    }
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("ask");
    }
    if (id == QStringLiteral("dictate")) {
        return QStringLiteral("dictate");
    }
    return id;
}

int defaultFloatingBarSeconds()
{
    return 2;
}

int defaultCountdownSeconds()
{
    return 3;
}

int defaultResultPopupSeconds()
{
    return 0;
}

QString speechProviderBaidu()
{
    return QStringLiteral("baidu");
}

QString speechProviderXfyun()
{
    return QStringLiteral("xfyun");
}

QString speechProviderCustom()
{
    return QStringLiteral("custom");
}

QStringList supportedSpeechProviderIds()
{
    return QStringList()
        << speechProviderBaidu()
        << speechProviderXfyun()
        << speechProviderCustom();
}

QString normalizeSpeechProvider(const QString &provider)
{
    const QString normalized = provider.trimmed().toLower();
    if (normalized == speechProviderXfyun()) {
        return speechProviderXfyun();
    }
    if (normalized == speechProviderCustom()) {
        return speechProviderCustom();
    }
    return speechProviderBaidu();
}

QString speechProviderTitle(const QString &provider)
{
    const QString normalized = normalizeSpeechProvider(provider);
    if (normalized == speechProviderXfyun()) {
        return trText("讯飞语音听写");
    }
    if (normalized == speechProviderCustom()) {
        return trText("自定义语音接口");
    }
    return trText("百度语音识别");
}

QString ocrEngineAutomatic()
{
    return QStringLiteral("automatic");
}

QString ocrEngineRapid()
{
    return QStringLiteral("rapid");
}

QString ocrEngineWindows()
{
    return QStringLiteral("windows");
}

QString ocrEngineCustomCloud()
{
    return QStringLiteral("customCloud");
}

QString ocrEngineVision()
{
    return QStringLiteral("vision");
}

QStringList supportedOcrEngineIds()
{
    return QStringList()
        << ocrEngineAutomatic()
        << ocrEngineRapid()
        << ocrEngineWindows()
        << ocrEngineCustomCloud()
        << ocrEngineVision();
}

QString normalizeOcrEngine(const QString &engine)
{
    const QString normalized = engine.trimmed();
    if (normalized == ocrEngineRapid()
        || normalized == ocrEngineWindows()
        || normalized == ocrEngineCustomCloud()
        || normalized == ocrEngineVision()) {
        return normalized;
    }
    return ocrEngineAutomatic();
}
