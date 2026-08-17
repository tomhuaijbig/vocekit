#include "custom_function_creation_coordinator.h"

#include "hub_settings_state.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../input/hotkey_definitions.h"

namespace {

CustomFunctionDef defaultCustomFunction(HubSettingsState *settings)
{
    CustomFunctionDef function;
    function.id = settings->nextCustomFunctionId();
    function.name = QString::fromUtf8("自定义功能 ") + function.id.mid(7);
    function.shortcut = settings->suggestedCustomShortcut();
    function.model = QStringLiteral("deepseek-v4-flash");
    function.outputMode = outputModePopup();
    function.resultTemplate = resultTemplateSimple();
    function.useSelection = true;
    function.useVoice = true;
    function.useScreenshot = false;
    function.screenshotTriggerMode = screenshotTriggerSeparate();
    function.screenshotShortcut = screenshotShortcutFromFunctionShortcut(
        function.shortcut
    );
    function.floatingBarSeconds = defaultFloatingBarSeconds();
    function.resultPopupSeconds = defaultResultPopupSeconds();
    function.countdownSeconds = defaultCountdownSeconds();
    function.recordingBeepEnabled = true;
    function.recordingBeepPath.clear();
    function.prompt = QString::fromUtf8(
        "请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。"
    );
    return function;
}

} // namespace

QString createCustomFunction(
    const CustomFunctionCreationActions &actions,
    OperationError *error)
{
    if (error) {
        *error = OperationError();
    }
    if (!actions.settings || !actions.flows.addCustomFunction) {
        if (error) {
            error->code =
                QStringLiteral("flow_add_function_unavailable");
        }
        return QString();
    }

    const CustomFunctionDef function = defaultCustomFunction(actions.settings);
    FunctionSettings settings =
        functionSettingsFromCustomFunction(function);
    settings.flow = FunctionFlowState();
    OperationError localError;
    if (!actions.flows.addCustomFunction(settings, &localError)) {
        if (error) {
            *error = localError;
        }
        return QString();
    }
    actions.settings->load();
    return function.id;
}
