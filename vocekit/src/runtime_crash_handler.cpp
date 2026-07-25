#include "runtime_crash_handler.h"

#include "runtime_log.h"

#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString crashText(const char *text)
{
    return QString::fromUtf8(text);
}

#ifdef Q_OS_WIN
LONG WINAPI vocekitUnhandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
{
    QString detail;
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        detail = QStringLiteral("异常码=0x")
            + QString::number(
                static_cast<qulonglong>(exceptionInfo->ExceptionRecord->ExceptionCode),
                16
            ).toUpper()
            + QStringLiteral("，地址=0x")
            + QString::number(
                reinterpret_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionAddress),
                16
            ).toUpper();
    } else {
        detail = QStringLiteral("异常信息不可用");
    }
    logRuntimeEvent(crashText("崩溃"), crashText("未处理异常"), detail);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void installRuntimeCrashHandlers()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(vocekitUnhandledExceptionFilter);
#endif
    std::set_terminate([]() {
        logRuntimeEvent(
            crashText("崩溃"),
            crashText("程序终止"),
            crashText("触发 std::terminate")
        );
        std::abort();
    });
}
