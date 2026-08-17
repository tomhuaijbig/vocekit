#include "function_flow_runtime_types.h"

QString functionFlowTriggerId(FunctionFlowTrigger trigger)
{
    if (trigger == FunctionFlowTrigger::ScreenshotHotkey) {
        return QStringLiteral("screenshotHotkey");
    }
    if (trigger == FunctionFlowTrigger::ScreenshotLauncher) {
        return QStringLiteral("screenshotLauncher");
    }
    return QStringLiteral("mainHotkey");
}
