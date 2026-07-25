#include "custom_function_creation_coordinator.h"

#include "hub_settings_state.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../input/hotkey_definitions.h"

namespace {

void runIfPresent(const std::function<void()> &action)
{
    if (action) {
        action();
    }
}

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

bool createAndEditCustomFunction(const CustomFunctionCreationActions &actions)
{
    if (!actions.settings) {
        return false;
    }

    const CustomFunctionDef function = defaultCustomFunction(actions.settings);
    actions.settings->addCustomFunction(function);
    runIfPresent(actions.saveSettings);

    const bool accepted = !actions.editFunction || actions.editFunction(function);
    if (accepted) {
        return true;
    }

    actions.settings->removeCustomFunction(function.id);
    runIfPresent(actions.saveSettings);
    return false;
}
