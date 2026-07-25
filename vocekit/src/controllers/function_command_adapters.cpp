#include "function_command_controller.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

FunctionCommandWindowHandle
captureForegroundFunctionCommandWindow()
{
#ifdef Q_OS_WIN
    return GetForegroundWindow();
#else
    return nullptr;
#endif
}
