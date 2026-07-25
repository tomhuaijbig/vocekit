#include "screenshot_ocr_config.h"

#include "../config/app_paths.h"
#include "../config/app_settings_defaults.h"
#include "../config/secret_config.h"

#include <QDir>
#include <QFileInfo>

namespace {

QString helperProgramPath(
    const QString &primaryRelativePath,
    const QString &fallbackRelativePath
)
{
    const QDir baseDir(appBasePath());
    const QString primaryPath = baseDir.filePath(primaryRelativePath);
    if (QFileInfo::exists(primaryPath)) {
        return primaryPath;
    }
    return baseDir.filePath(fallbackRelativePath);
}

} // namespace

OcrEngine screenshotOcrEngineFromSettings(const AppSettingsData &settings)
{
    const QString engine = normalizeOcrEngine(settings.ocrEngine);
    if (engine == ocrEngineRapid()) {
        return OcrEngine::RapidOcr;
    }
    if (engine == ocrEngineWindows()) {
        return OcrEngine::WindowsOcr;
    }
    if (engine == ocrEngineCustomCloud()) {
        return OcrEngine::CustomCloud;
    }
    if (engine == ocrEngineVision()) {
        return OcrEngine::VisionModel;
    }
    return OcrEngine::Automatic;
}

OcrManagerConfig buildScreenshotOcrManagerConfig(
    const AppSettingsData &settings,
    const SecretConfig &secrets
)
{
    OcrManagerConfig config;
    config.rapidOcrProgram = helperProgramPath(
        QStringLiteral("ocr/rapidocr/vocekit-rapidocr.exe"),
        QStringLiteral("helpers/bin/vocekit-rapidocr.exe")
    );
    config.windowsOcrProgram = helperProgramPath(
        QStringLiteral("ocr/windows/vocekit-windows-ocr.exe"),
        QStringLiteral("helpers/bin/vocekit-windows-ocr.exe")
    );
    config.timeoutMs = qBound(5000, settings.ocrTimeoutMs, 120000);
    config.customCloud.url = secrets.customOcrUrl;
    config.customCloud.apiKey = secrets.customOcrApiKey;
    config.customCloud.model = secrets.customOcrModel;
    config.customCloud.timeoutMs = config.timeoutMs;
    config.customCloud.useSystemProxy = settings.useSystemProxy;
    return config;
}
