#include "diagnostics_settings_snapshot.h"

#include <QtGlobal>

namespace {

QString normalizedWindowsSpeechLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    if (normalized == QStringLiteral("zh-cn")) {
        return QStringLiteral("zh-CN");
    }
    if (normalized == QStringLiteral("en-us")) {
        return QStringLiteral("en-US");
    }
    return QStringLiteral("follow-windows");
}

} // namespace

DiagnosticsSettingsSnapshot buildDiagnosticsSettingsSnapshot(
    const AppSettingsData &settings
)
{
    DiagnosticsSettingsSnapshot snapshot;
    snapshot.useSystemProxy = settings.useSystemProxy;
    snapshot.ocrEngine = settings.ocrEngine;
    snapshot.windowsSpeechLanguage = normalizedWindowsSpeechLanguage(
        settings.windowsSpeechLanguage
    );
    snapshot.ocrTimeoutMs = qBound(5000, settings.ocrTimeoutMs, 120000);
    snapshot.floatingBarEnabled = settings.floatingBarEnabled;

    const int dictateIndex = settings.functionIndex(QStringLiteral("dictate"));
    if (dictateIndex >= 0) {
        snapshot.dictateFloatingBarSeconds = qBound(
            0,
            settings.functions.at(dictateIndex).output.floatingBarSeconds,
            60
        );
    }
    return snapshot;
}
