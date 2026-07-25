#include "command_center_shell_access_factory.h"

#include "hub_settings_state.h"
#include "shortcut_display.h"

#include "../domain/function_catalog.h"
#include "../input/hotkey_definitions.h"

CommandCenterShellAccess createCommandCenterShellAccess(
    const CommandCenterShellAccessFactoryDependencies &dependencies
)
{
    CommandCenterShellAccess access;
    HubSettingsState *settings = dependencies.settings;
    access.functionsProvider = [settings]() {
        QVector<CommandCenterFunctionItem> items;
        if (!settings) {
            return items;
        }

        const AppSettingsData snapshot = settings->toData();
        for (const HotkeyDef &definition : coreFunctionDefs()) {
            CommandCenterFunctionItem item;
            item.id = definition.id;
            item.title = functionDisplayTitle(snapshot, definition.id);
            item.shortcut = displayShortcut(settings->hotkey(definition.id));
            items.append(item);
        }
        for (const CustomFunctionDef &function : settings->customFunctions()) {
            CommandCenterFunctionItem item;
            item.id = function.id;
            item.title = function.name;
            item.shortcut = displayShortcut(function.shortcut);
            items.append(item);
        }
        return items;
    };

    access.openFunction = dependencies.openFunction
        ? dependencies.openFunction
        : [](const QString &) {};
    access.openTool = dependencies.openTool
        ? dependencies.openTool
        : [](const QString &) {};
    access.addFunction = dependencies.addFunction
        ? dependencies.addFunction
        : []() {};
    access.searchMissed = dependencies.searchMissed
        ? dependencies.searchMissed
        : [](const QString &) {};
    return access;
}
