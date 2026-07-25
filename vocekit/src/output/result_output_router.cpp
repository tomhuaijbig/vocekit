#include "result_output_router.h"

#include "../config/app_settings_defaults.h"

namespace {

QString outputText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

ResultOutputPlan ResultOutputRouter::plan(
    const ResultOutputRouteRequest &request
)
{
    ResultOutputPlan result;
    const QString mode = normalizeOutputMode(request.outputMode);
    if (mode == outputModeAutoWrite()) {
        result.destination = ResultOutputDestination::AutoWrite;
        result.replaceSelectedText = request.hasSelectedText;
        result.progressTitle = outputText("写入中");
        result.progressMessage = outputText("正在写入当前输入位置");
        result.doneTitle = outputText("已写入");
        result.doneMessage = outputText("结果已粘贴到当前输入位置");
        result.logAction = outputText("自动写入");
        return result;
    }

    if (mode == outputModeScreenshotPanel() && request.screenshotInput) {
        result.destination = ResultOutputDestination::ScreenshotPanel;
        return result;
    }

    result.destination = ResultOutputDestination::ResultPopup;
    return result;
}

ResultOutputDestination ResultOutputRouter::route(
    const ResultOutputRouteRequest &request
)
{
    return plan(request).destination;
}
