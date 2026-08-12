#ifndef VOCEKIT_APP_SETTINGS_DEFAULTS_H
#define VOCEKIT_APP_SETTINGS_DEFAULTS_H

#include <QString>
#include <QStringList>

QString defaultModelForFunction(const QString &id);
QString modelProvider(const QString &model);
QString providerModelId(const QString &model);

QString outputModeAutoWrite();
QString outputModePopup();
QString outputModeScreenshotPanel();
QString normalizeOutputMode(const QString &value, const QString &fallback = QString());
QString defaultOutputModeForFunction(const QString &id);
QString outputModeTitle(const QString &mode);

QString floatingBarStyleStatusPill();
QString floatingBarStyleLiveTranscriptCard();
QString floatingBarStyleInherit();
QString normalizeGlobalFloatingBarStyle(const QString &value);
QString normalizeFunctionFloatingBarStyle(const QString &value);
QString resolveFloatingBarStyle(
    const QString &overrideValue,
    const QString &globalValue
);
QString floatingBarStyleTitle(const QString &value, bool allowInherit);

QString resultTemplateSimple();
QString resultTemplateDetail();
QString resultTemplateCompare();
QString resultTemplateOutputOnly();
QString normalizeResultTemplate(const QString &value, const QString &fallback = QString());
QString resultTemplateTitle(const QString &value);

QString vocabularyAddModeAi();
QString vocabularyAddModeAsk();
QString vocabularyAddModeManual();
QString normalizeVocabularyAddMode(const QString &value);
QString vocabularyAddModeTitle(const QString &mode);

bool defaultUseSelectionForFunction(const QString &id);
bool defaultUseVoiceForFunction(const QString &id);
QString defaultPromptIdForFunction(const QString &id);

int defaultFloatingBarSeconds();
int defaultCountdownSeconds();
int defaultResultPopupSeconds();

QString speechProviderBaidu();
QString speechProviderXfyun();
QString speechProviderCustom();
QStringList supportedSpeechProviderIds();
QString normalizeSpeechProvider(const QString &provider);
QString speechProviderTitle(const QString &provider);

QString ocrEngineAutomatic();
QString ocrEngineRapid();
QString ocrEngineWindows();
QString ocrEngineCustomCloud();
QString ocrEngineVision();
QStringList supportedOcrEngineIds();
QString normalizeOcrEngine(const QString &engine);

#endif // VOCEKIT_APP_SETTINGS_DEFAULTS_H
