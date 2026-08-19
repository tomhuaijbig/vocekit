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
bool g_forceMiniDumpFallbackForTests = false;

struct MiniDumpResult
{
    QString path;
    QString mode;
    QString errorStage;
    DWORD errorCode = ERROR_SUCCESS;
    bool fallbackUsed = false;
    DWORD primaryErrorCode = ERROR_SUCCESS;
};

struct MiniDumpAttemptResult
{
    bool written = false;
    QString errorStage;
    DWORD errorCode = ERROR_SUCCESS;
};

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

MiniDumpAttemptResult tryWriteMiniDump(
    const QString &path,
    MINIDUMP_TYPE dumpType,
    EXCEPTION_POINTERS *exceptionInfo)
{
    MiniDumpAttemptResult result;
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
        result.errorStage = QStringLiteral("create_dump_file");
        result.errorCode = GetLastError();
        return result;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionData;
    exceptionData.ThreadId = GetCurrentThreadId();
    exceptionData.ExceptionPointers = exceptionInfo;
    exceptionData.ClientPointers = FALSE;
    const BOOL written = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        dumpType,
        exceptionInfo ? &exceptionData : nullptr,
        nullptr,
        nullptr
    );
    const DWORD writeError = written ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!written) {
        QFile::remove(path);
        result.errorStage = QStringLiteral("write_minidump");
        result.errorCode = writeError;
        return result;
    }
    result.written = true;
    return result;
}

MiniDumpResult writeMiniDump(EXCEPTION_POINTERS *exceptionInfo)
{
    MiniDumpResult result;
    SetLastError(ERROR_SUCCESS);
    const QString path = crashDumpPath();
    if (path.isEmpty()) {
        result.errorStage = QStringLiteral("create_directory");
        result.errorCode = GetLastError();
        return result;
    }

    const MINIDUMP_TYPE richDumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal
        | MiniDumpWithIndirectlyReferencedMemory
        | MiniDumpScanMemory
        | MiniDumpWithThreadInfo
    );
    MiniDumpAttemptResult rich;
    if (g_forceMiniDumpFallbackForTests) {
        rich.errorStage = QStringLiteral("write_minidump");
        rich.errorCode = ERROR_INVALID_PARAMETER;
    } else {
        rich = tryWriteMiniDump(path, richDumpType, exceptionInfo);
    }
    if (rich.written) {
        result.path = path;
        result.mode = QStringLiteral("rich");
        return result;
    }
    if (rich.errorStage != QStringLiteral("write_minidump")) {
        result.errorStage = rich.errorStage;
        result.errorCode = rich.errorCode;
        return result;
    }

    result.fallbackUsed = true;
    result.primaryErrorCode = rich.errorCode;
    const MiniDumpAttemptResult fallback = tryWriteMiniDump(
        path,
        MiniDumpNormal,
        exceptionInfo
    );
    if (fallback.written) {
        result.path = path;
        result.mode = QStringLiteral("normal_fallback");
        return result;
    }
    result.errorStage = fallback.errorStage == QStringLiteral("write_minidump")
        ? QStringLiteral("write_minidump_fallback")
        : QStringLiteral("create_fallback_dump_file");
    result.errorCode = fallback.errorCode;
    return result;
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
    const MiniDumpResult dump = writeMiniDump(exceptionInfo);
    recordRuntimeCrash(
        kind,
        exceptionCode,
        exceptionAddress,
        dump.path,
        dump.errorStage,
        static_cast<quint64>(dump.errorCode),
        dump.mode,
        dump.fallbackUsed,
        static_cast<quint64>(dump.primaryErrorCode)
    );
    logRuntimeEvent(
        crashText("崩溃"),
        kind == QStringLiteral("std_terminate")
            ? crashText("程序终止")
            : crashText("未处理异常"),
        QStringLiteral("异常码=0x%1，地址=0x%2，转储=%3")
            .arg(QString::number(exceptionCode, 16).toUpper())
            .arg(QString::number(exceptionAddress, 16).toUpper())
            .arg(dump.path.isEmpty()
                ? QStringLiteral("写入失败(%1:0x%2)").arg(
                    dump.errorStage,
                    QStringLiteral("%1").arg(
                        dump.errorCode,
                        8,
                        16,
                        QLatin1Char('0')
                    ).toUpper()
                )
                : QStringLiteral("%1(%2)").arg(dump.path, dump.mode))
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
    g_forceMiniDumpFallbackForTests =
        qEnvironmentVariable("VOCEKIT_ENABLE_TEST_HOOKS") == QStringLiteral("1")
        && qEnvironmentVariable("VOCEKIT_FORCE_MINIDUMP_FALLBACK_FOR_TESTS")
            == QStringLiteral("1");
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
