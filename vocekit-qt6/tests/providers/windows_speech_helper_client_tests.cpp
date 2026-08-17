#include <QtTest>

#include "../../src/providers/windows_speech_helper_client.h"
#include "../../src/tasks/cancellation_token.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <thread>

namespace {

QString fakeHelperPath()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/fake_windows_speech_helper.exe");
}

WindowsSpeechBatchRequest batchRequest(
    const QString &runId = QStringLiteral("batch-1"))
{
    WindowsSpeechBatchRequest request;
    request.runId = runId;
    request.language = QStringLiteral("zh-CN");
    request.pcm = QByteArray::fromHex("010203040506");
    request.timeoutMs = 3000;
    return request;
}

WindowsSpeechHelperClient clientFor(const QString &scenario)
{
    return WindowsSpeechHelperClient(
        fakeHelperPath(),
        QStringList() << QStringLiteral("--scenario") << scenario
    );
}

bool processIsRunning(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid)
    );
    if (!process) {
        return false;
    }
    DWORD exitCode = 0;
    const bool running = GetExitCodeProcess(process, &exitCode)
        && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return running;
#else
    Q_UNUSED(pid);
    return false;
#endif
}

} // namespace

class WindowsSpeechHelperClientTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(QFile::exists(fakeHelperPath()), qPrintable(fakeHelperPath()));
    }

    void computesBoundedDefaultBatchTimeout()
    {
        QCOMPARE(windowsSpeechBatchTimeoutMs(0), 15000);
        QCOMPARE(windowsSpeechBatchTimeoutMs(32000), 15000);
        QCOMPARE(windowsSpeechBatchTimeoutMs(32000LL * 600), 910000);
        QCOMPARE(windowsSpeechBatchTimeoutMs(32000LL * 1800), 2100000);
        QCOMPARE(windowsSpeechBatchTimeoutMs(32000LL * 7200), 2100000);
    }

    void probesInstalledLanguages()
    {
        WindowsSpeechProbeRequest request;
        request.runId = QStringLiteral("probe-1");
        request.language = QStringLiteral("follow-windows");
        request.timeoutMs = 3000;

        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("probe")).probe(request);

        QVERIFY2(result.ok, qPrintable(result.errorCode));
        QCOMPARE(result.resolvedLanguage, QStringLiteral("zh-CN"));
        QCOMPARE(
            result.installedLanguages,
            QStringList() << QStringLiteral("zh-CN")
                          << QStringLiteral("en-US")
        );
    }

    void recognizesBatchAndWritesExactPcm()
    {
        const WindowsSpeechBatchRequest request = batchRequest();
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("echo-pcm-size")).recognize(request);

        QVERIFY2(result.ok, qPrintable(result.errorCode));
        QCOMPARE(result.text, QString::fromUtf8("你好"));
        QCOMPARE(result.pcmBytesObserved, qint64(request.pcm.size()));
        QVERIFY(result.maximumBytesQueued < 64 * 1024);
    }

    void acceptsLinesSplitAcrossReads()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("split-lines")).recognize(
                batchRequest(QStringLiteral("split-1"))
            );
        QVERIFY2(result.ok, qPrintable(result.errorCode));
        QCOMPARE(result.text, QString::fromUtf8("你好"));
    }

    void returnsProgramMissingBeforeStart()
    {
        WindowsSpeechHelperClient client(
            fakeHelperPath() + QStringLiteral(".missing")
        );
        const WindowsSpeechHelperResult result = client.recognize(
            batchRequest(QStringLiteral("missing-1"))
        );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("PROGRAM_MISSING"));
    }

    void distinguishesExistingButUnstartableProgram()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path() + QStringLiteral("/invalid.exe");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not an executable"), qint64(17));
        file.close();

        WindowsSpeechHelperClient client(path);
        WindowsSpeechBatchRequest request = batchRequest(
            QStringLiteral("start-failure-1")
        );
        request.timeoutMs = 1000;
        const WindowsSpeechHelperResult result = client.recognize(request);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("START_FAILED"));
    }

    void rejectsInvalidJsonWithoutEchoingIt()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("invalid-json")).recognize(
                batchRequest(QStringLiteral("invalid-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
        QVERIFY(!result.errorMessage.contains(QStringLiteral("not-json")));
    }

    void rejectsMismatchedRunId()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("wrong-run-id")).recognize(
                batchRequest(QStringLiteral("wrong-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("RUN_ID_MISMATCH"));
    }

    void capsCombinedOutputIndependently()
    {
        WindowsSpeechBatchRequest request = batchRequest(
            QStringLiteral("oversize-1")
        );
        request.timeoutMs = 3000;
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("oversize-output")).recognize(request);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("OUTPUT_TOO_LARGE"));
    }

    void capsStandardErrorIndependently()
    {
        WindowsSpeechBatchRequest request = batchRequest(
            QStringLiteral("oversize-stderr-1")
        );
        request.timeoutMs = 3000;
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("oversize-stderr")).recognize(request);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("OUTPUT_TOO_LARGE"));
    }

    void reportsProcessCrash()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("crash")).recognize(
                batchRequest(QStringLiteral("crash-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("PROCESS_CRASHED"));
    }

    void timesOutWithinExplicitDeadline()
    {
        WindowsSpeechBatchRequest request = batchRequest(
            QStringLiteral("timeout-1")
        );
        request.timeoutMs = 150;
        QElapsedTimer elapsed;
        elapsed.start();
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("timeout")).recognize(request);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("TIMEOUT"));
        QVERIFY(elapsed.elapsed() < 1500);
    }

    void rejectsFinalBeforeReady()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("no-ready")).recognize(
                batchRequest(QStringLiteral("no-ready-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
    }

    void rejectsExitWithoutTerminalEvent()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("no-final")).recognize(
                batchRequest(QStringLiteral("no-final-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
    }

    void rejectsDuplicateTerminalEvents()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("duplicate-final")).recognize(
                batchRequest(QStringLiteral("duplicate-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
    }

    void rejectsEmptyFinalText()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("empty-final")).recognize(
                batchRequest(QStringLiteral("empty-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("EMPTY_TEXT"));
    }

    void rejectsFinalWithoutHelperEof()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("final-input-not-ended")).recognize(
                batchRequest(QStringLiteral("input-not-ended-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
    }

    void passesHelperErrorThroughUnchanged()
    {
        const WindowsSpeechHelperResult result =
            clientFor(QStringLiteral("error")).recognize(
                batchRequest(QStringLiteral("error-1"))
            );
        QVERIFY(!result.ok);
        QCOMPARE(
            result.errorCode,
            QStringLiteral("RECOGNIZER_MISSING")
        );
        QCOMPARE(
            result.errorMessage,
            QStringLiteral("Recognizer missing.")
        );
    }

    void cancellationDuringBackpressureReapsChildProcess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString pidPath = directory.path() + QStringLiteral("/pid.txt");
        WindowsSpeechHelperClient client(
            fakeHelperPath(),
            QStringList()
                << QStringLiteral("--scenario")
                << QStringLiteral("slow-read")
                << QStringLiteral("--pid-file")
                << pidPath
        );

        CancellationSource cancellation;
        WindowsSpeechBatchRequest request = batchRequest(
            QStringLiteral("cancel-1")
        );
        request.pcm = QByteArray(4 * 1024 * 1024, 'p');
        request.timeoutMs = 5000;
        request.cancellation = cancellation.token();

        std::thread canceller([&cancellation]() {
            QThread::msleep(150);
            cancellation.cancel();
        });
        QElapsedTimer elapsed;
        elapsed.start();
        const WindowsSpeechHelperResult result = client.recognize(request);
        canceller.join();

        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, QStringLiteral("CANCELLED"));
        QVERIFY(elapsed.elapsed() < 2000);
        QVERIFY(result.maximumBytesQueued < 64 * 1024);

        QFile pidFile(pidPath);
        QVERIFY(pidFile.open(QIODevice::ReadOnly));
        bool ok = false;
        const qint64 pid = pidFile.readAll().trimmed().toLongLong(&ok);
        QVERIFY(ok);
        QVERIFY(!processIsRunning(pid));
    }
};

QTEST_GUILESS_MAIN(WindowsSpeechHelperClientTests)

#include "windows_speech_helper_client_tests.moc"
