#include "../../src/runtime_crash_handler.h"
#include "../../src/runtime_session.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class RuntimeCrashHandlerTests : public QObject
{
    Q_OBJECT

private slots:
    void repeatedRecentCrashesTriggerSafeMode();
    void recoveryAndOldCrashesDoNotTriggerSafeMode();
    void parsesBoundedDiagnosticExitDelay();
    void unhandledChildCrashCreatesDumpAndMetadata();
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
#ifndef Q_OS_WIN
    QSKIP("Windows minidump regression test");
#else
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QProcess child;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("VOCEKIT_ENABLE_TEST_HOOKS"), QStringLiteral("1"));
    environment.insert(
        QStringLiteral("VOCEKIT_RUNTIME_LOG_DIR_FOR_TESTS"),
        QDir::toNativeSeparators(temp.path())
    );
    child.setProcessEnvironment(environment);
    child.start(
        QCoreApplication::applicationFilePath(),
        QStringList() << QStringLiteral("--crash-child")
    );
    QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));
    QVERIFY2(child.waitForFinished(15000), qPrintable(child.errorString()));
    QVERIFY(child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0);

    const QDir crashDir(temp.filePath(QStringLiteral("crashes")));
    const QStringList dumps = crashDir.entryList(
        QStringList() << QStringLiteral("*.dmp"),
        QDir::Files
    );
    const QStringList metadata = crashDir.entryList(
        QStringList() << QStringLiteral("crash-*.json"),
        QDir::Files
    );
    QCOMPARE(dumps.size(), 1);
    QCOMPARE(metadata.size(), 1);
    QVERIFY(QFileInfo(crashDir.filePath(dumps.constFirst())).size() > 0);

    QFile file(crashDir.filePath(metadata.constFirst()));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(!object.value(QStringLiteral("session_id")).toString().isEmpty());
    QCOMPARE(object.value(QStringLiteral("dump_file")).toString(), dumps.constFirst());
    QCOMPARE(object.value(QStringLiteral("kind")).toString(), QStringLiteral("unhandled_exception"));
#endif
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QCoreApplication::arguments().contains(QStringLiteral("--crash-child"))) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        beginRuntimeSession(QCoreApplication::arguments(), QStringLiteral("test"));
        installRuntimeCrashHandlers();
        RaiseException(0xE0424242, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        return 99;
    }
#endif
    RuntimeCrashHandlerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "runtime_crash_handler_tests.moc"
