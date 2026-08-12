#include <QtTest>

#include "../../src/providers/windows_streaming_speech_session.h"

#include <QFile>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString fakeHelperPath()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/fake_windows_speech_helper.exe");
}

WindowsStreamingSpeechSession::Timing fastTiming()
{
    WindowsStreamingSpeechSession::Timing timing;
    timing.startupTimeoutMs = 200;
    timing.finalTimeoutMs = 200;
    timing.killTimeoutMs = 50;
    timing.writeChunkBytes = 2;
    timing.queueLimitBytes = 64000;
    return timing;
}

StreamingSpeechSessionRequest requestFor(
    const QString &runId = QStringLiteral("stream-1"))
{
    StreamingSpeechSessionRequest request;
    request.provider = QStringLiteral("windows-local");
    request.language = QStringLiteral("zh-CN");
    request.runId = runId;
    return request;
}

QStringList scenarioArguments(
    const QString &scenario,
    const QStringList &extra = QStringList())
{
    return QStringList() << QStringLiteral("--scenario") << scenario
                         << extra;
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

qint64 readPid(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    bool ok = false;
    const qint64 pid = file.readAll().trimmed().toLongLong(&ok);
    return ok ? pid : 0;
}

} // namespace

class WindowsStreamingSpeechSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(QFile::exists(fakeHelperPath()), qPrintable(fakeHelperPath()));
    }

    void defaultsToEightSecondFinalDeadline()
    {
        const WindowsStreamingSpeechSession::Timing timing;
        QCOMPARE(timing.finalTimeoutMs, 8000);
    }

    void startsAsynchronouslyAndReachesReady()
    {
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("delayed-ready")),
            requestFor(), StreamingSpeechCallbacks(), fastTiming()
        );
        QString error;
        QVERIFY(session.start(&error));
        QVERIFY(error.isEmpty());
        QCOMPARE(session.state(), StreamingSpeechState::Connecting);
        QTRY_COMPARE(session.state(), StreamingSpeechState::Streaming);
        session.cancel();
    }

    void buffersPcmBeforeReadyAndHandlesPartialWrites()
    {
        QStringList completed;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &text) {
            completed.append(text);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("partial-write")),
            requestFor(QStringLiteral("partial-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        const QByteArray pcm = QByteArray::fromHex("0102030405060708");
        QVERIFY(session.pushAudio(pcm));
        session.finish();
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(completed.first(), QStringLiteral("bytes:8"));
    }

    void passesRequestLanguageAndRunIdToHelper()
    {
        QStringList completed;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &text) {
            completed.append(text);
        };
        StreamingSpeechSessionRequest request = requestFor(
            QStringLiteral("argument-run-id")
        );
        request.language = QStringLiteral("en-US");
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("echo-language")),
            request, callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        session.finish();
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(completed.first(), QStringLiteral("en-US"));
    }

    void replacesHypothesesCommitsAndCompletesOnce()
    {
        QVector<StreamingTranscriptSnapshot> snapshots;
        QStringList completed;
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &s) {
            snapshots.append(s);
        };
        callbacks.completed = [&](const QString &text) {
            completed.append(text);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(),
            scenarioArguments(QStringLiteral("hypothesis-replacement")),
            requestFor(QStringLiteral("snapshot-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QVERIFY(session.pushAudio(QByteArray::fromHex("0102")));
        session.finish();
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY(snapshots.size() >= 3);
        QCOMPARE(snapshots.at(0).provisionalText, QString::fromUtf8("你"));
        QCOMPARE(snapshots.at(1).provisionalText, QString::fromUtf8("你好"));
        QCOMPARE(snapshots.at(2).committedText, QString::fromUtf8("你好"));
        QVERIFY(snapshots.at(2).provisionalText.isEmpty());
        QCOMPARE(completed.first(), QString::fromUtf8("你好 世界"));
    }

    void duplicateFinalStillCompletesExactlyOnce()
    {
        QStringList completed;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &text) {
            completed.append(text);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("duplicate-final")),
            requestFor(QStringLiteral("duplicate-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        session.finish();
        QTRY_COMPARE(completed.size(), 1);
        QTest::qWait(100);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(session.state(), StreamingSpeechState::Completed);
    }

    void finishClosesInputAfterQueueDrains()
    {
        QStringList completed;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &text) {
            completed.append(text);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("partial-write")),
            requestFor(QStringLiteral("eof-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QVERIFY(session.pushAudio(QByteArray(100, 'p')));
        session.finish();
        QCOMPARE(session.state(), StreamingSpeechState::Finalizing);
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(completed.first(), QStringLiteral("bytes:100"));
    }

    void rejectsQueueOverflowAndDegradesAfterReady()
    {
        QStringList degraded;
        StreamingSpeechCallbacks callbacks;
        callbacks.degraded = [&](const QString &message) {
            degraded.append(message);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("slow-read")),
            requestFor(QStringLiteral("overflow-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QTRY_COMPARE(session.state(), StreamingSpeechState::Streaming);
        QVERIFY(!session.pushAudio(QByteArray(64001, 'p')));
        QTRY_COMPARE(degraded.size(), 1);
        QCOMPARE(session.state(), StreamingSpeechState::Degraded);
    }

    void badJsonAndWrongRunIdDegradeAfterReady()
    {
        const QStringList scenarios = QStringList()
            << QStringLiteral("invalid-json")
            << QStringLiteral("wrong-after-ready");
        for (const QString &scenario : scenarios) {
            QStringList degraded;
            StreamingSpeechCallbacks callbacks;
            callbacks.degraded = [&](const QString &message) {
                degraded.append(message);
            };
            WindowsStreamingSpeechSession session(
                fakeHelperPath(), scenarioArguments(scenario),
                requestFor(scenario), callbacks, fastTiming()
            );
            QVERIFY(session.start(nullptr));
            QTRY_COMPARE(degraded.size(), 1);
            QCOMPARE(session.state(), StreamingSpeechState::Degraded);
        }
    }

    void startupFailuresAreStructuredAndNeverDegraded()
    {
        struct Case {
            QString scenario;
            QString helperCode;
            QString expectedCode;
        };
        const QVector<Case> cases = {
            {QStringLiteral("startup-error"),
             QStringLiteral("RECOGNIZER_MISSING"),
             QStringLiteral("speech.windows.recognizer_missing")},
            {QStringLiteral("startup-error"),
             QStringLiteral("SYSTEM_SPEECH_UNAVAILABLE"),
             QStringLiteral("speech.windows.runtime_missing")},
            {QStringLiteral("startup-error"),
             QStringLiteral("GRAMMAR_LOAD_FAILED"),
             QStringLiteral("speech.windows.grammar_load_failed")},
            {QStringLiteral("timeout"), QString(),
             QStringLiteral("speech.windows.local")},
            {QStringLiteral("wrong-run-id"), QString(),
             QStringLiteral("speech.windows.local")},
            {QStringLiteral("startup-invalid-json"), QString(),
             QStringLiteral("speech.windows.local")}
        };
        for (const Case &item : cases) {
            QStringList configurationCodes;
            QStringList degraded;
            StreamingSpeechCallbacks callbacks;
            callbacks.configurationFailed = [&](const QString &code,
                                                  const QString &) {
                configurationCodes.append(code);
            };
            callbacks.degraded = [&](const QString &message) {
                degraded.append(message);
            };
            WindowsStreamingSpeechSession session(
                fakeHelperPath(),
                scenarioArguments(
                    item.scenario,
                    item.helperCode.isEmpty()
                        ? QStringList()
                        : QStringList()
                            << QStringLiteral("--error-code")
                            << item.helperCode
                ),
                requestFor(item.scenario), callbacks, fastTiming()
            );
            QVERIFY(session.start(nullptr));
            QTRY_COMPARE(configurationCodes.size(), 1);
            QCOMPARE(configurationCodes.first(), item.expectedCode);
            QVERIFY(degraded.isEmpty());
        }
    }

    void missingProgramReportsProgramMissingExactlyOnce()
    {
        QStringList codes;
        QStringList degraded;
        StreamingSpeechCallbacks callbacks;
        callbacks.configurationFailed = [&](const QString &code,
                                              const QString &) {
            codes.append(code);
        };
        callbacks.degraded = [&](const QString &message) {
            degraded.append(message);
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath() + QStringLiteral(".missing"), QStringList(),
            requestFor(QStringLiteral("missing-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QTRY_COMPARE(codes.size(), 1);
        QCOMPARE(codes.first(), QStringLiteral("speech.windows.program_missing"));
        QVERIFY(degraded.isEmpty());
    }

    void existingButUnstartableProgramIsConfigurationFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path() + QStringLiteral("/invalid.exe");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not executable"), qint64(14));
        file.close();

        QStringList codes;
        QStringList degraded;
        StreamingSpeechCallbacks callbacks;
        callbacks.configurationFailed = [&](const QString &code,
                                              const QString &) {
            codes.append(code);
        };
        callbacks.degraded = [&](const QString &message) {
            degraded.append(message);
        };
        WindowsStreamingSpeechSession session(
            path, QStringList(), requestFor(QStringLiteral("start-failed-1")),
            callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QTRY_COMPARE(codes.size(), 1);
        QCOMPARE(codes.first(), QStringLiteral("speech.windows.local"));
        QVERIFY(degraded.isEmpty());
    }

    void finalTimeoutAndCrashAfterReadyDegrade()
    {
        const QStringList scenarios = QStringList()
            << QStringLiteral("final-timeout")
            << QStringLiteral("ready-crash");
        for (const QString &scenario : scenarios) {
            QStringList degraded;
            StreamingSpeechCallbacks callbacks;
            callbacks.degraded = [&](const QString &message) {
                degraded.append(message);
            };
            WindowsStreamingSpeechSession session(
                fakeHelperPath(), scenarioArguments(scenario),
                requestFor(scenario), callbacks, fastTiming()
            );
            QVERIFY(session.start(nullptr));
            QTRY_COMPARE(session.state(), StreamingSpeechState::Streaming);
            session.finish();
            QTRY_COMPARE(degraded.size(), 1);
            QCOMPARE(session.state(), StreamingSpeechState::Degraded);
        }
    }

    void cancelSuppressesCallbacksAndReapsProcess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString pidPath = directory.path() + QStringLiteral("/pid.txt");
        int callbackCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &) { ++callbackCount; };
        callbacks.degraded = [&](const QString &) { ++callbackCount; };
        callbacks.configurationFailed = [&](const QString &, const QString &) {
            ++callbackCount;
        };
        WindowsStreamingSpeechSession session(
            fakeHelperPath(),
            scenarioArguments(QStringLiteral("slow-read"),
                              QStringList() << QStringLiteral("--pid-file")
                                            << pidPath),
            requestFor(QStringLiteral("cancel-1")), callbacks, fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QTRY_VERIFY(QFile::exists(pidPath));
        const qint64 pid = readPid(pidPath);
        QVERIFY(pid > 0);
        session.cancel();
        QCOMPARE(session.state(), StreamingSpeechState::Cancelled);
        QTRY_VERIFY(!processIsRunning(pid));
        QCOMPARE(callbackCount, 0);
    }

    void destructorSuppressesCallbacksAndReapsProcess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString pidPath = directory.path() + QStringLiteral("/pid.txt");
        int callbackCount = 0;
        qint64 pid = 0;
        {
            StreamingSpeechCallbacks callbacks;
            callbacks.completed = [&](const QString &) { ++callbackCount; };
            callbacks.degraded = [&](const QString &) { ++callbackCount; };
            callbacks.configurationFailed = [&](const QString &, const QString &) {
                ++callbackCount;
            };
            WindowsStreamingSpeechSession session(
                fakeHelperPath(),
                scenarioArguments(QStringLiteral("slow-read"),
                                  QStringList() << QStringLiteral("--pid-file")
                                                << pidPath),
                requestFor(QStringLiteral("destroy-1")), callbacks, fastTiming()
            );
            QVERIFY(session.start(nullptr));
            QTRY_VERIFY(QFile::exists(pidPath));
            pid = readPid(pidPath);
            QVERIFY(pid > 0);
        }
        QTRY_VERIFY(!processIsRunning(pid));
        QCOMPARE(callbackCount, 0);
    }

    void rejectsInvalidStateAndRequestSynchronously()
    {
        WindowsStreamingSpeechSession missingRunId(
            fakeHelperPath(), scenarioArguments(QStringLiteral("ready-final")),
            requestFor(QString()), StreamingSpeechCallbacks(), fastTiming()
        );
        QString error;
        QVERIFY(!missingRunId.start(&error));
        QVERIFY(!error.isEmpty());

        WindowsStreamingSpeechSession session(
            fakeHelperPath(), scenarioArguments(QStringLiteral("ready-final")),
            requestFor(QStringLiteral("state-1")),
            StreamingSpeechCallbacks(), fastTiming()
        );
        QVERIFY(session.start(nullptr));
        QVERIFY(!session.start(&error));
        session.cancel();
    }
};

QTEST_GUILESS_MAIN(WindowsStreamingSpeechSessionTests)

#include "windows_streaming_speech_session_tests.moc"
