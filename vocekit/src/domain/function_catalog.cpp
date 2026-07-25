#include "function_catalog.h"

#include "../input/hotkey_definitions.h"

namespace {

QString s(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

FunctionSettings functionSettingsById(
    const AppSettingsData &settings,
    const QString &id,
    bool *found
)
{
    if (found) {
        *found = false;
    }
    for (const FunctionSettings &function : settings.functions) {
        if (function.id == id) {
            if (found) {
                *found = true;
            }
            return function;
        }
    }
    return FunctionSettings();
}

QString functionDisplayTitle(
    const AppSettingsData &settings,
    const QString &id,
    const QString &fallback
)
{
    for (const HotkeyDef &def : coreFunctionDefs()) {
        if (def.id == id) {
            return def.title;
        }
    }

    bool found = false;
    const FunctionSettings function =
        functionSettingsById(settings, id, &found);
    if (found && !function.name.trimmed().isEmpty()) {
        return function.name.trimmed();
    }

    return fallback.trimmed().isEmpty() ? s("功能") : fallback;
}
