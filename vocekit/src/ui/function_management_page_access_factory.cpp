#include "function_management_page_access_factory.h"

#include "hub_settings_state.h"

#include "../input/hotkey_definitions.h"

FunctionManagementPageAccess createFunctionManagementPageAccess(
    const FunctionManagementPageAccessDependencies &dependencies
)
{
    FunctionManagementPageAccess access;
    HubSettingsState *settings = dependencies.settings;
    const std::function<QString(const QString &, const QString &)> summaryProvider =
        dependencies.summaryProvider;

    access.itemsProvider = [settings, summaryProvider]() {
        QVector<FunctionManagementItem> items;
        if (!settings) {
            return items;
        }
        for (const HotkeyDef &definition : coreFunctionDefs()) {
            FunctionManagementItem item;
            item.id = definition.id;
            item.title = definition.title;
            item.shortcut = settings->hotkey(definition.id);
            item.summary = summaryProvider
                ? summaryProvider(item.id, item.shortcut)
                : QString();
            items.append(item);
        }
        for (const CustomFunctionDef &function : settings->customFunctions()) {
            FunctionManagementItem item;
            item.id = function.id;
            item.title = function.name;
            item.shortcut = function.shortcut;
            item.summary = summaryProvider
                ? summaryProvider(item.id, item.shortcut)
                : QString();
            item.custom = true;
            item.function = function;
            items.append(item);
        }
        return items;
    };

    access.addFunction = dependencies.addFunction;
    const auto editFunction = dependencies.editFunction;
    access.editFunction = [editFunction](const FunctionManagementItem &item) {
        if (editFunction) {
            editFunction(item.id, item.title, item.custom, item.function);
        }
    };

    const auto saveSettings = dependencies.saveSettings;
    access.removeFunction = [settings, saveSettings](
        const FunctionManagementItem &item
    ) {
        if (!settings || !item.custom) {
            return;
        }
        settings->removeCustomFunction(item.id);
        if (saveSettings) {
            saveSettings();
        }
    };
    return access;
}
