#include "hotkey_settings_snapshot.h"

#include "hotkey_definitions.h"

namespace {

FunctionSettings functionSettingsById(
    const AppSettingsData &settings,
    const QString &id)
{
    return settings.function(id);
}

GlobalHotkeyFunction globalHotkeyFunctionFromData(
    const FunctionSettings &settings,
    const QString &id,
    const QString &title,
    const QString &shortcut)
{
    GlobalHotkeyFunction function;
    function.id = id;
    function.title = title;
    function.shortcut = shortcut;
    function.recordingTriggerMode = settings.recording.triggerMode;
    function.useVoice = settings.input.useVoice;
    function.useScreenshot = settings.input.useScreenshot;
    function.screenshotTriggerMode = settings.input.screenshotTriggerMode;
    function.screenshotShortcut = settings.input.screenshotShortcut.trimmed();
    if (function.screenshotShortcut.isEmpty()) {
        function.screenshotShortcut =
            screenshotShortcutFromFunctionShortcut(shortcut);
    }
    return function;
}

} // namespace

GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings)
{
    GlobalHotkeySettingsSnapshot snapshot;
    for (const HotkeyDef &def : hotkeyDefs()) {
        const FunctionSettings functionSettings =
            functionSettingsById(settings, def.id);
        const QString shortcut = settings.applicationHotkeys
            .value(def.id, def.defaultValue)
            .trimmed();
        snapshot.functions.append(
            globalHotkeyFunctionFromData(
                functionSettings,
                def.id,
                def.title,
                shortcut.isEmpty() ? def.defaultValue : shortcut
            )
        );
    }
    for (const FunctionSettings &functionSettings : settings.functions) {
        if (functionSettings.builtIn) {
            continue;
        }
        snapshot.functions.append(
            globalHotkeyFunctionFromData(
                functionSettings,
                functionSettings.id,
                functionSettings.name,
                functionSettings.shortcut
            )
        );
    }
    return snapshot;
}
