#include "hotkey_settings_snapshot.h"

#include "../capture/screenshot_types.h"
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
    function.useHoldToTalk =
        function.recordingTriggerMode == QStringLiteral("hold")
        && function.useVoice
        && !(function.useScreenshot
            && screenshotTriggerUsesPrimary(
                function.screenshotTriggerMode
            ));
    function.registerScreenshotHotkey =
        function.useScreenshot
        && screenshotTriggerUsesSeparate(
            function.screenshotTriggerMode
        );
    return function;
}

bool triggerHasNodeType(
    const FunctionFlowExecutionPlan &plan,
    const FunctionFlowTriggerPlan &trigger,
    FunctionFlowNodeType type)
{
    for (const QString &nodeId : trigger.activeSourceNodeIds) {
        if (plan.nodes.contains(nodeId)
            && plan.nodes.value(nodeId).type == type) {
            return true;
        }
    }
    return false;
}

QString screenshotShortcutForTrigger(
    const FunctionFlowExecutionPlan &plan,
    const FunctionFlowTriggerPlan &trigger)
{
    for (const QString &nodeId : trigger.activeSourceNodeIds) {
        if (!plan.nodes.contains(nodeId)) {
            continue;
        }
        const FunctionFlowCompiledNode node =
            plan.nodes.value(nodeId);
        if (node.type == FunctionFlowNodeType::ScreenshotSource) {
            return node.config.screenshot.separateShortcut.trimmed();
        }
    }
    return QString();
}

void applyPublishedFlowProfiles(
    const FunctionFlowExecutionPlan &plan,
    GlobalHotkeyFunction *function)
{
    if (!function) {
        return;
    }

    const FunctionFlowTriggerPlan main =
        plan.triggers.value(FunctionFlowTrigger::MainHotkey);
    if (main.available) {
        function->useVoice = triggerHasNodeType(
            plan,
            main,
            FunctionFlowNodeType::VoiceSource
        );
        function->recordingTriggerMode =
            main.usesHoldToTalk
                ? QStringLiteral("hold")
                : QStringLiteral("toggle");
        function->useHoldToTalk = main.usesHoldToTalk;
    }

    const FunctionFlowTriggerPlan screenshot =
        plan.triggers.value(FunctionFlowTrigger::ScreenshotHotkey);
    if (screenshot.available) {
        function->registerScreenshotHotkey =
            triggerHasNodeType(
                plan,
                screenshot,
                FunctionFlowNodeType::ScreenshotSource
            );
        function->useScreenshot =
            function->useScreenshot
            || function->registerScreenshotHotkey;
        function->screenshotTriggerMode =
            screenshotTriggerSeparate();
        const QString shortcut = screenshotShortcutForTrigger(
            plan,
            screenshot
        );
        if (!shortcut.isEmpty()) {
            function->screenshotShortcut = shortcut;
        }
    }
}

void clearClassicExecutionProfiles(GlobalHotkeyFunction *function)
{
    if (!function) {
        return;
    }
    function->recordingTriggerMode = QStringLiteral("toggle");
    function->useVoice = false;
    function->useScreenshot = false;
    function->useHoldToTalk = false;
    function->registerScreenshotHotkey = false;
    function->screenshotShortcut.clear();
}

void applyExecutionModeProfiles(
    const FunctionSettings &settings,
    const FunctionFlowPlanProvider &flowPlanProvider,
    GlobalHotkeyFunction *function)
{
    if (settings.executionMode != FunctionExecutionMode::Canvas) {
        return;
    }

    clearClassicExecutionProfiles(function);
    if (!flowPlanProvider) {
        return;
    }
    const QSharedPointer<const FunctionFlowExecutionPlan> plan =
        flowPlanProvider(settings.id);
    if (!plan.isNull()) {
        applyPublishedFlowProfiles(*plan, function);
    }
}

} // namespace

bool functionUsesScreenshotLauncher(
    const FunctionSettings &function,
    const QSharedPointer<const FunctionFlowExecutionPlan> &plan)
{
    if (function.executionMode == FunctionExecutionMode::Classic) {
        return function.input.useScreenshot
            && screenshotTriggerUsesLauncher(
                function.input.screenshotTriggerMode
            );
    }
    return !plan.isNull()
        && plan->triggers.value(
            FunctionFlowTrigger::ScreenshotLauncher
        ).available;
}

GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings)
{
    return globalHotkeySnapshotFromData(
        settings,
        FunctionFlowPlanProvider()
    );
}

GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings,
    const FunctionFlowPlanProvider &flowPlanProvider)
{
    GlobalHotkeySettingsSnapshot snapshot;
    for (const HotkeyDef &def : hotkeyDefs()) {
        const FunctionSettings functionSettings =
            functionSettingsById(settings, def.id);
        const QString shortcut = settings.applicationHotkeys
            .value(def.id, def.defaultValue)
            .trimmed();
        GlobalHotkeyFunction function =
            globalHotkeyFunctionFromData(
                functionSettings,
                def.id,
                def.title,
                shortcut.isEmpty() ? def.defaultValue : shortcut
            );
        applyExecutionModeProfiles(
            functionSettings,
            flowPlanProvider,
            &function
        );
        snapshot.functions.append(function);
    }
    for (const FunctionSettings &functionSettings : settings.functions) {
        if (functionSettings.builtIn) {
            continue;
        }
        GlobalHotkeyFunction function =
            globalHotkeyFunctionFromData(
                functionSettings,
                functionSettings.id,
                functionSettings.name,
                functionSettings.shortcut
            );
        applyExecutionModeProfiles(
            functionSettings,
            flowPlanProvider,
            &function
        );
        snapshot.functions.append(function);
    }
    return snapshot;
}
