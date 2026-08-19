#include "runtime_crash_handler.h"

#include "runtime_log.h"
#include "runtime_session.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#endif

namespace {

QString crashText(const char *text)
{
    return QString::fromUtf8(text);
}

#ifdef Q_OS_WIN
volatile LONG g_crashHandlerActive = 0;

QString crashDumpPath()
{
    const RuntimeSessionInfo session = currentRuntimeSession();
    const QString sessionId = session.sessionId.isEmpty()
        ? QStringLiteral("unknown-session")
        : session.sessionId;
    const QString crashDirectory = QDir(runtimeLogDirectory()).filePath(
        QStringLiteral("crashes")
    );
    if (!QDir().mkpath(crashDirectory)) {
        return QString();
    }
    return QDir(crashDirectory).filePath(
        QStringLiteral("vocekit-%1-%2.dmp").arg(
            QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd-HHmmss-zzz")
            ),
            sessionId
        )
    );
}

QString writeMiniDump(EXCEPTION_POINTERS *exceptionInfo)
{
    const QString path = crashDumpPath();
    if (path.isEmpty()) {
        return QString();
    }
    const HANDLE file = CreateFileW(
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return QString();
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionData;
    exceptionData.ThreadId = GetCurrentThreadId();
    exceptionData.ExceptionPointers = exceptionInfo;
    exceptionData.ClientPointers = FALSE;
    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal
        | MiniDumpWithIndirectlyReferencedMemory
        | MiniDumpScanMemory
        | MiniDumpWithThreadInfo
    );
    const BOOL written = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        dumpType,
        exceptionInfo ? &exceptionData : nullptr,
        nullptr,
        nullptr
    );
    CloseHandle(file);
    if (!written) {
        QFile::remove(path);
        return QString();
    }
    return path;
}

void captureCrash(
    const QString &kind,
    quint64 exceptionCode,
    quintptr exceptionAddress,
    EXCEPTION_POINTERS *exceptionInfo)
{
    if (InterlockedCompareExchange(&g_crashHandlerActive, 1, 0) != 0) {
        return;
    }
    const QString dumpPath = writeMiniDump(exceptionInfo);
    recordRuntimeCrash(kind, exceptionCode, exceptionAddress, dumpPath);
    logRuntimeEvent(
        crashText("崩溃"),
        kind == QStringLiteral("std_terminate")
            ? crashText("程序终止")
            : crashText("未处理异常"),
        QStringLiteral("异常码=0x%1，地址=0x%2，转储=%3")
            .arg(QString::number(exceptionCode, 16).toUpper())
            .arg(QString::number(exceptionAddress, 16).toUpper())
            .arg(dumpPath.isEmpty() ? QStringLiteral("写入失败") : dumpPath)
    );
}

LONG WINAPI vocekitUnhandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
{
    quint64 exceptionCode = 0;
    quintptr exceptionAddress = 0;
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        exceptionCode = static_cast<quint64>(
            exceptionInfo->ExceptionRecord->ExceptionCode
        );
        exceptionAddress = reinterpret_cast<quintptr>(
            exceptionInfo->ExceptionRecord->ExceptionAddress
        );
    }
    captureCrash(
        QStringLiteral("unhandled_exception"),
        exceptionCode,
        exceptionAddress,
        exceptionInfo
    );
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
#ifdef Q_OS_WIN
        captureCrash(
            QStringLiteral("std_terminate"),
            0,
            0,
            nullptr
        );
#else
        logRuntimeEvent(
            crashText("崩溃"),
            crashText("程序终止"),
            crashText("触发 std::terminate")
        );
#endif
        std::abort();
    });
}
