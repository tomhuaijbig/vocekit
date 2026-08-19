#include "../../src/runtime_crash_handler.h"
#include "../../src/runtime_session.h"

#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("<unreadable: %1>").arg(file.errorString());
    }
    return QString::fromUtf8(file.readAll());
}

QString crashDiagnostics(
    const QProcess &child,
    qint64 childPid,
    const QString &rootPath,
    const QByteArray &standardOutput,
    const QByteArray &standardError)
{
    QStringList lines;
    lines << QStringLiteral("program=%1").arg(QCoreApplication::applicationFilePath())
          << QStringLiteral("child_pid=%1").arg(childPid)
          << QStringLiteral("exit_status=%1").arg(
                 child.exitStatus() == QProcess::CrashExit
                     ? QStringLiteral("CrashExit")
                     : QStringLiteral("NormalExit")
             )
          << QStringLiteral("exit_code=%1").arg(child.exitCode())
          << QStringLiteral("process_error=%1").arg(static_cast<int>(child.error()))
          << QStringLiteral("process_error_text=%1").arg(child.errorString())
          << QStringLiteral("root=%1").arg(QDir::toNativeSeparators(rootPath))
          << QStringLiteral("stdout=%1").arg(QString::fromUtf8(standardOutput))
          << QStringLiteral("stderr=%1").arg(QString::fromUtf8(standardError));

    QDirIterator iterator(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        const QString relativePath = QDir(rootPath).relativeFilePath(path);
        lines << QStringLiteral("file=%1 size=%2").arg(relativePath).arg(info.size());
        if (info.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
            lines << QStringLiteral("json[%1]=%2").arg(
                relativePath,
                readTextFile(path)
            );
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void verifyCrashChild(
    const QString &childMode,
    const QString &expectedKind,
    const QString &expectedExceptionCode,
    bool requireCrashExit,
    bool forceFallback = false)
{
#ifndef Q_OS_WIN
    Q_UNUSED(childMode);
    Q_UNUSED(expectedKind);
    Q_UNUSED(expectedExceptionCode);
    Q_UNUSED(requireCrashExit);
    Q_UNUSED(forceFallback);
    QSKIP("Windows minidump regression test");
#else
    const QString artifactRoot = qEnvironmentVariable(
        "VOCEKIT_CRASH_TEST_ARTIFACT_ROOT"
    ).trimmed();
    if (!artifactRoot.isEmpty()) {
        QVERIFY2(
            QDir().mkpath(artifactRoot),
            qPrintable(QStringLiteral("Cannot create diagnostic root: %1").arg(artifactRoot))
        );
    }
    const QString tempTemplate = QDir(
        artifactRoot.isEmpty() ? QDir::tempPath() : artifactRoot
    ).filePath(QStringLiteral("runtime-crash-XXXXXX"));
    QTemporaryDir temp(tempTemplate);
    temp.setAutoRemove(artifactRoot.isEmpty());
    QVERIFY(temp.isValid());

    QProcess child;
    child.setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("VOCEKIT_ENABLE_TEST_HOOKS"), QStringLiteral("1"));
    if (forceFallback) {
        environment.insert(
            QStringLiteral("VOCEKIT_FORCE_MINIDUMP_FALLBACK_FOR_TESTS"),
            QStringLiteral("1")
        );
    }
    environment.insert(
        QStringLiteral("VOCEKIT_RUNTIME_LOG_DIR_FOR_TESTS"),
        QDir::toNativeSeparators(temp.path())
    );
    child.setProcessEnvironment(environment);
    child.start(
        QCoreApplication::applicationFilePath(),
        QStringList() << childMode
    );
    const bool started = child.waitForStarted(5000);
    const qint64 childPid = started ? static_cast<qint64>(child.processId()) : 0;
    if (!started) {
        const QString diagnostics = crashDiagnostics(
            child,
            childPid,
            temp.path(),
            child.readAllStandardOutput(),
            child.readAllStandardError()
        );
        QFAIL(qPrintable(diagnostics));
    }

    const bool finished = child.waitForFinished(15000);
    if (!finished) {
        child.kill();
        child.waitForFinished(5000);
    }
    const QByteArray standardOutput = child.readAllStandardOutput();
    const QByteArray standardError = child.readAllStandardError();
    const QString diagnostics = crashDiagnostics(
        child,
        childPid,
        temp.path(),
        standardOutput,
        standardError
    );
    QVERIFY2(finished, qPrintable(diagnostics));
    if (requireCrashExit) {
        QVERIFY2(child.exitStatus() == QProcess::CrashExit, qPrintable(diagnostics));
    } else {
        QVERIFY2(
            child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0,
            qPrintable(diagnostics)
        );
    }
    QVERIFY2(
        QFileInfo(temp.filePath(QStringLiteral("session-last.json"))).isFile(),
        qPrintable(diagnostics)
    );

    const QDir crashDir(temp.filePath(QStringLiteral("crashes")));
    const QStringList dumps = crashDir.entryList(
        QStringList() << QStringLiteral("*.dmp"),
        QDir::Files
    );
    const QStringList metadata = crashDir.entryList(
        QStringList() << QStringLiteral("crash-*.json"),
        QDir::Files
    );
    QVERIFY2(metadata.size() == 1, qPrintable(diagnostics));

    QFile file(crashDir.filePath(metadata.constFirst()));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(diagnostics));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(diagnostics));
    QVERIFY2(document.isObject(), qPrintable(diagnostics));
    const QJsonObject object = document.object();
    QVERIFY2(
        !object.value(QStringLiteral("session_id")).toString().isEmpty(),
        qPrintable(diagnostics)
    );
    QCOMPARE(object.value(QStringLiteral("schema_version")).toInt(), 2);
    QCOMPARE(object.value(QStringLiteral("kind")).toString(), expectedKind);
    QCOMPARE(
        object.value(QStringLiteral("exception_code")).toString(),
        expectedExceptionCode
    );
    QCOMPARE(object.value(QStringLiteral("dump_written")).toBool(), true);
    QVERIFY2(
        object.value(QStringLiteral("dump_mode")).toString()
            == QStringLiteral("rich")
        || object.value(QStringLiteral("dump_mode")).toString()
            == QStringLiteral("normal_fallback"),
        qPrintable(diagnostics)
    );
    QCOMPARE(object.value(QStringLiteral("dump_error_stage")).toString(), QString());
    QCOMPARE(object.value(QStringLiteral("dump_error_code")).toInteger(), 0);
    QCOMPARE(
        object.value(QStringLiteral("dump_error_code_hex")).toString(),
        QStringLiteral("0x00000000")
    );
    if (forceFallback) {
        QCOMPARE(object.value(QStringLiteral("dump_fallback_used")).toBool(), true);
        QCOMPARE(
            object.value(QStringLiteral("dump_mode")).toString(),
            QStringLiteral("normal_fallback")
        );
        QCOMPARE(
            object.value(QStringLiteral("dump_primary_error_code")).toInteger(),
            static_cast<qint64>(ERROR_INVALID_PARAMETER)
        );
        QCOMPARE(
            object.value(QStringLiteral("dump_primary_error_code_hex")).toString(),
            QStringLiteral("0x00000057")
        );
    }

    QVERIFY2(dumps.size() == 1, qPrintable(diagnostics));
    QVERIFY2(
        QFileInfo(crashDir.filePath(dumps.constFirst())).size() > 0,
        qPrintable(diagnostics)
    );
    QCOMPARE(object.value(QStringLiteral("dump_file")).toString(), dumps.constFirst());
#endif
}

} // namespace

class RuntimeCrashHandlerTests : public QObject
{
    Q_OBJECT

private slots:
    void repeatedRecentCrashesTriggerSafeMode();
    void recoveryAndOldCrashesDoNotTriggerSafeMode();
    void parsesBoundedDiagnosticExitDelay();
    void unhandledChildCrashCreatesDumpAndMetadata();
    void terminateChildCreatesDumpAndMetadata();
    void richDumpFailureFallsBackToNormalDump();
};

void RuntimeCrashHandlerTests::repeatedRecentCrashesTriggerSafeMode()
{
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-19T12:00:00.000Z"),
        Qt::ISODateWithMs
    );
    const QVector<QDateTime> crashes = {
        now.addSecs(-90),
        now.addSecs(-20),
        now.addSecs(-800)
    };
    int count = 0;
    QVERIFY(runtimeSafeModeRequired(crashes, QDateTime(), now, 2, 600, &count));
    QCOMPARE(count, 2);
}

void RuntimeCrashHandlerTests::recoveryAndOldCrashesDoNotTriggerSafeMode()
{
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-19T12:00:00.000Z"),
        Qt::ISODateWithMs
    );
    const QVector<QDateTime> crashes = {
        now.addSecs(-900),
        now.addSecs(-120),
        now.addSecs(-30),
        now.addSecs(30)
    };
    int count = -1;
    QVERIFY(!runtimeSafeModeRequired(
        crashes,
        now.addSecs(-60),
        now,
        2,
        600,
        &count
    ));
    QCOMPARE(count, 1);
}

void RuntimeCrashHandlerTests::parsesBoundedDiagnosticExitDelay()
{
    QCOMPARE(
        runtimeDiagnosticExitDelayMs(
            QStringList() << QStringLiteral("--diagnostic-exit-after-ms=1200")
        ),
        1200
    );
    QCOMPARE(
        runtimeDiagnosticExitDelayMs(
            QStringList() << QStringLiteral("--diagnostic-exit-after-ms=99")
        ),
        -1
    );
    QCOMPARE(
        runtimeDiagnosticExitDelayMs(
            QStringList() << QStringLiteral("--diagnostic-exit-after-ms=60001")
        ),
        -1
    );
    QCOMPARE(runtimeDiagnosticExitDelayMs(QStringList()), -1);
}

void RuntimeCrashHandlerTests::unhandledChildCrashCreatesDumpAndMetadata()
{
    verifyCrashChild(
        QStringLiteral("--crash-child=access-violation"),
        QStringLiteral("unhandled_exception"),
        QStringLiteral("0xC0000005"),
        true
    );
}

void RuntimeCrashHandlerTests::terminateChildCreatesDumpAndMetadata()
{
    verifyCrashChild(
        QStringLiteral("--crash-child=terminate"),
        QStringLiteral("std_terminate"),
        QStringLiteral("0x0"),
        false
    );
}

void RuntimeCrashHandlerTests::richDumpFailureFallsBackToNormalDump()
{
    verifyCrashChild(
        QStringLiteral("--crash-child=access-violation"),
        QStringLiteral("unhandled_exception"),
        QStringLiteral("0xC0000005"),
        true,
        true
    );
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QCoreApplication::arguments().contains(
            QStringLiteral("--crash-child=access-violation"))) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        beginRuntimeSession(QCoreApplication::arguments(), QStringLiteral("test"));
        installRuntimeCrashHandlers();
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        void *page = VirtualAlloc(
            nullptr,
            systemInfo.dwPageSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
        if (!page) {
            return 96;
        }
        volatile LONG *protectedValue = static_cast<volatile LONG *>(page);
        *protectedValue = 0;
        DWORD oldProtection = 0;
        if (!VirtualProtect(
                page,
                systemInfo.dwPageSize,
                PAGE_NOACCESS,
                &oldProtection)) {
            VirtualFree(page, 0, MEM_RELEASE);
            return 97;
        }
        *protectedValue = 42;
        return 98;
    }
    if (QCoreApplication::arguments().contains(
            QStringLiteral("--crash-child=terminate"))) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        beginRuntimeSession(QCoreApplication::arguments(), QStringLiteral("test"));
        installRuntimeCrashHandlers();
        std::terminate();
        return 99;
    }
#endif
    RuntimeCrashHandlerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "runtime_crash_handler_tests.moc"
