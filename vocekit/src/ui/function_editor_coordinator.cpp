#include "function_editor_coordinator.h"

#include "function_summary_formatter.h"
#include "hub_settings_state.h"
#include "shortcut_display.h"

#include "../config/app_settings_defaults.h"
#include "../domain/prompt_runtime_library.h"
#include "../providers/model_catalog.h"

namespace {

QString promptTitleForFunction(
    const QString &id,
    const FunctionEditorCoordinatorActions &actions
)
{
    PromptRuntimeSnapshot snapshot;
    if (actions.prompts.snapshotProvider) {
        snapshot = actions.prompts.snapshotProvider();
    }
    const PromptTargetInfo target = promptRuntimeTargetForId(
        snapshot,
        actions.settings->promptIdFor(id)
    );
    return target.title.trimmed().isEmpty()
        ? QString::fromUtf8("默认提示词")
        : target.title;
}

FunctionSummaryViewData summaryData(
    const QString &id,
    const QString &shortcut,
    const FunctionEditorCoordinatorActions &actions
)
{
    FunctionSummaryViewData data;
    data.shortcut = displayShortcut(shortcut);
    data.modelTitle = modelTitle(actions.settings->modelFor(id));
    data.useSelection = actions.settings->useSelectionFor(id);
    data.useVoice = actions.settings->useVoiceFor(id);
    data.useScreenshot = actions.settings->useScreenshotFor(id);
    data.outputModeTitle = outputModeTitle(actions.settings->outputModeFor(id));
    data.resultTemplateTitle = resultTemplateTitle(
        actions.settings->resultTemplateFor(id)
    );
    data.floatingBarSeconds = actions.settings->floatingBarSecondsFor(id);
    data.resultPopupSeconds = actions.settings->resultPopupSecondsFor(id);
    data.countdownSeconds = actions.settings->countdownSecondsFor(id);
    data.recordingBeepEnabled = actions.settings->recordingBeepEnabledFor(id);
    data.recordingTriggerMode = actions.settings->recordingTriggerModeFor(id);
    data.longRecordingEnabled = actions.settings->longRecordingEnabledFor(id);
    data.promptTitle = promptTitleForFunction(id, actions);
    return data;
}

} // namespace

QString functionEditorSummaryText(
    const QString &id,
    const QString &shortcut,
    const FunctionEditorCoordinatorActions &actions
)
{
    if (!actions.settings) {
        return QString();
    }
    return functionSummaryText(summaryData(id, shortcut, actions));
}
