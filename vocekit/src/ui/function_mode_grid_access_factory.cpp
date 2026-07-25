#include "function_mode_grid_access_factory.h"

#include "hub_settings_state.h"
#include "../input/hotkey_definitions.h"

namespace {

QString builtInTitle(const QString &id)
{
    if (id == QStringLiteral("dictate")) {
        return QString::fromUtf8("听写");
    }
    if (id == QStringLiteral("translate")) {
        return QString::fromUtf8("翻译");
    }
    return QString::fromUtf8("问答");
}

FunctionModeGridSnapshot settingsSnapshot(HubSettingsState *settings)
{
    FunctionModeGridSnapshot snapshot;
    if (!settings) {
        return snapshot;
    }

    for (const HotkeyDef &definition : coreFunctionDefs()) {
        FunctionModeCardSnapshot card;
        card.id = definition.id;
        card.title = builtInTitle(definition.id);
        card.shortcut = settings->hotkey(definition.id);
        card.model = settings->modelFor(definition.id);
        card.outputMode = settings->outputModeFor(definition.id);
        card.useSelection = settings->useSelectionFor(definition.id);
        card.useVoice = settings->useVoiceFor(definition.id);
        card.useScreenshot = settings->useScreenshotFor(definition.id);
        snapshot.cards.append(card);
    }

    for (const CustomFunctionDef &function : settings->customFunctions()) {
        FunctionModeCardSnapshot card;
        card.id = function.id;
        card.title = function.name;
        card.shortcut = function.shortcut;
        card.model = function.model;
        card.outputMode = settings->outputModeFor(function.id);
        card.useSelection = function.useSelection;
        card.useVoice = function.useVoice;
        card.useScreenshot = function.useScreenshot;
        card.custom = true;
        card.customFunction = function;
        snapshot.cards.append(card);
    }
    snapshot.order = settings->functionOrderIds();
    return snapshot;
}

bool saveFunctionOrder(
    HubSettingsState *settings,
    const QStringList &ids,
    QString *error
)
{
    if (!settings) {
        if (error) {
            *error = QString::fromUtf8("设置对象不可用。");
        }
        return false;
    }

    const QStringList previous = settings->functionOrderIds();
    if (!settings->setFunctionOrderIds(ids)) {
        return true;
    }
    if (settings->save()) {
        return true;
    }

    settings->setFunctionOrderIds(previous);
    if (error) {
        *error = QString::fromUtf8("无法写入 config/settings.json。");
    }
    return false;
}

} // namespace

FunctionModeGridAccess createFunctionModeGridAccess(
    HubSettingsState *settings
)
{
    FunctionModeGridAccess access;
    access.snapshotProvider = [settings]() {
        return settingsSnapshot(settings);
    };
    access.saveOrder = [settings](const QStringList &ids, QString *error) {
        return saveFunctionOrder(settings, ids, error);
    };
    return access;
}
