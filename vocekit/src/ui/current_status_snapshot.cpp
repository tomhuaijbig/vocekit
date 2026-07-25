#include "current_status_snapshot.h"

CurrentStatusSnapshot buildCurrentStatusSnapshot(
    const AppSettingsData &settings
)
{
    CurrentStatusSnapshot snapshot;
    snapshot.trayResident = settings.trayResident;
    snapshot.autoStartEnabled = settings.autoStartEnabled;
    snapshot.strongSelectionEnabled = settings.strongSelectionEnabled;
    snapshot.vocabularyEnabled = settings.vocabularyEnabled;
    snapshot.vocabularyAddMode = settings.vocabularyAddMode;
    snapshot.dictatePolishEnabled = settings.dictatePolishEnabled;
    snapshot.speechProvider = settings.speechProvider;
    snapshot.useSystemProxy = settings.useSystemProxy;
    snapshot.floatingBarEnabled = settings.floatingBarEnabled;
    snapshot.usesDefaultRecordDirectory =
        settings.recordDirectory.trimmed().isEmpty();

    for (const FunctionSettings &function : settings.functions) {
        if (!function.input.useVoice) {
            continue;
        }
        if (function.recording.triggerMode == QStringLiteral("hold")) {
            ++snapshot.holdToTalkFunctionCount;
        }
        if (function.recording.longRecordingEnabled) {
            ++snapshot.longRecordingFunctionCount;
        }
    }

    return snapshot;
}
