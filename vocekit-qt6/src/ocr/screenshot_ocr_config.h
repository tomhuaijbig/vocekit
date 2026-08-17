#ifndef VOCEKIT_SCREENSHOT_OCR_CONFIG_H
#define VOCEKIT_SCREENSHOT_OCR_CONFIG_H

#include "../config/app_settings_data.h"
#include "ocr_manager.h"

struct SecretConfig;

// Converts user settings and secrets into the structure consumed by OcrManager.
OcrEngine screenshotOcrEngineFromSettings(const AppSettingsData &settings);
OcrManagerConfig buildScreenshotOcrManagerConfig(
    const AppSettingsData &settings,
    const SecretConfig &secrets
);

#endif // VOCEKIT_SCREENSHOT_OCR_CONFIG_H
