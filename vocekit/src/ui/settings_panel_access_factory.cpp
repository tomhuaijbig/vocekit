#include "settings_panel_access_factory.h"

#include "hub_settings_state.h"

SettingsPanelAssembly createSettingsPanelAssembly(
    const SettingsPanelAccessFactoryDependencies &dependencies
)
{
    SettingsPanelAssembly assembly;
    HubSettingsState *settings = dependencies.settings;

    assembly.access.snapshotProvider = [settings]() {
        return settings ? settings->toData() : AppSettingsData();
    };
    assembly.access.applyAndSave = [settings](const AppSettingsData &data) {
        return settings ? settings->replaceAndSave(data) : false;
    };
    assembly.access.previewFloatingBarStyle =
        dependencies.previewFloatingBarStyle;

    const auto notifySettingsChanged = dependencies.notifySettingsChanged;
    assembly.onChanged = [notifySettingsChanged]() {
        if (notifySettingsChanged) {
            notifySettingsChanged();
        }
    };
    return assembly;
}
