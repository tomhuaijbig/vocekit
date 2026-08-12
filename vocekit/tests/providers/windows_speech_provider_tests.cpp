#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/providers/windows_speech_provider.h"

class WindowsSpeechProviderTests : public QObject
{
    Q_OBJECT

private slots:
    void speechRequestDefaultsToFollowingWindows()
    {
        const SpeechRecognitionRequest request;
        QCOMPARE(
            request.language,
            windowsSpeechLanguageFollowWindows()
        );
    }

    void exposesStableProviderId()
    {
        WindowsSpeechProvider provider{
            WindowsSpeechProvider::BatchFunction(),
            WindowsSpeechProvider::ProbeFunction()
        };
        QCOMPARE(provider.id(), speechProviderWindowsLocal());
    }

    void recognizePassesPcmAndLanguageToLocalHelper()
    {
        WindowsSpeechBatchRequest captured;
        WindowsSpeechProvider provider(
            [&captured](const WindowsSpeechBatchRequest &request) {
                captured = request;
                WindowsSpeechHelperResult result;
                result.ok = true;
                result.text = QStringLiteral("local result");
                return result;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = QByteArray::fromHex("01020304");
        request.audioFormat = QStringLiteral("pcm");
        request.sampleRate = 16000;
        request.language = windowsSpeechLanguageChinese();

        const SpeechRecognitionResult result = provider.recognize(
            request, CancellationToken()
        );

        QVERIFY(result.isSuccess());
        QCOMPARE(captured.pcm, request.audioData);
        QCOMPARE(captured.language, windowsSpeechLanguageChinese());
        QVERIFY(!captured.runId.trimmed().isEmpty());
        QVERIFY(captured.timeoutMs >= 15000);
    }

    void usesFreshRunIdForEveryRecognition()
    {
        QStringList runIds;
        WindowsSpeechProvider provider(
            [&runIds](const WindowsSpeechBatchRequest &request) {
                runIds.append(request.runId);
                WindowsSpeechHelperResult result;
                result.ok = true;
                result.text = QStringLiteral("ok");
                return result;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = QByteArray::fromHex("0102");
        request.audioFormat = QStringLiteral("pcm");
        request.sampleRate = 16000;

        QVERIFY(provider.recognize(request, CancellationToken()).isSuccess());
        QVERIFY(provider.recognize(request, CancellationToken()).isSuccess());
        QCOMPARE(runIds.size(), 2);
        QVERIFY(runIds.at(0) != runIds.at(1));
    }

    void probesFollowWindowsAndReportsLanguages()
    {
        WindowsSpeechProbeRequest captured;
        WindowsSpeechProvider provider(
            WindowsSpeechProvider::BatchFunction(),
            [&captured](const WindowsSpeechProbeRequest &request) {
                captured = request;
                WindowsSpeechHelperResult result;
                result.ok = true;
                result.resolvedLanguage = windowsSpeechLanguageChinese();
                result.installedLanguages = QStringList()
                    << windowsSpeechLanguageChinese()
                    << windowsSpeechLanguageEnglish();
                return result;
            }
        );

        const ProviderCheckResult result = provider.checkConfiguration();

        QVERIFY(result.available);
        QCOMPARE(captured.language, windowsSpeechLanguageFollowWindows());
        QVERIFY(!captured.runId.trimmed().isEmpty());
        QVERIFY(result.message.contains(windowsSpeechLanguageChinese()));
        QVERIFY(result.message.contains(windowsSpeechLanguageEnglish()));
    }

    void mapsProbeErrorsWithoutExposingHelperOutput()
    {
        WindowsSpeechProvider provider(
            WindowsSpeechProvider::BatchFunction(),
            [](const WindowsSpeechProbeRequest &) {
                WindowsSpeechHelperResult result;
                result.errorCode = QStringLiteral("RECOGNIZER_MISSING");
                result.errorMessage = QStringLiteral("sensitive helper detail");
                return result;
            }
        );

        const ProviderCheckResult result = provider.checkConfiguration();

        QVERIFY(!result.available);
        QCOMPARE(
            result.error.code,
            QStringLiteral("speech.windows.recognizer_missing")
        );
        QVERIFY(result.error.message != QStringLiteral("sensitive helper detail"));
        QCOMPARE(result.error.detail, QStringLiteral("sensitive helper detail"));
    }

    void mapsHelperErrors_data()
    {
        QTest::addColumn<QString>("helperCode");
        QTest::addColumn<QString>("operationCode");

        QTest::newRow("program") << QStringLiteral("PROGRAM_MISSING")
            << QStringLiteral("speech.windows.program_missing");
        QTest::newRow("recognizer") << QStringLiteral("RECOGNIZER_MISSING")
            << QStringLiteral("speech.windows.recognizer_missing");
        QTest::newRow("runtime") << QStringLiteral("SYSTEM_SPEECH_UNAVAILABLE")
            << QStringLiteral("speech.windows.runtime_missing");
        QTest::newRow("grammar") << QStringLiteral("GRAMMAR_LOAD_FAILED")
            << QStringLiteral("speech.windows.grammar_load_failed");
        QTest::newRow("no-speech") << QStringLiteral("NO_SPEECH")
            << QStringLiteral("speech.empty_result");
        QTest::newRow("cancel") << QStringLiteral("CANCELLED")
            << QStringLiteral("operation.cancelled");
        QTest::newRow("local") << QStringLiteral("WRITE_FAILED")
            << QStringLiteral("speech.windows.local");
    }

    void mapsHelperErrors()
    {
        QFETCH(QString, helperCode);
        QFETCH(QString, operationCode);
        WindowsSpeechProvider provider(
            [helperCode](const WindowsSpeechBatchRequest &) {
                WindowsSpeechHelperResult result;
                result.errorCode = helperCode;
                result.errorMessage = QStringLiteral("helper failed");
                return result;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = QByteArray::fromHex("0102");
        request.audioFormat = QStringLiteral("pcm");
        request.sampleRate = 16000;

        const SpeechRecognitionResult result = provider.recognize(
            request, CancellationToken()
        );

        QCOMPARE(result.error.code, operationCode);
        QVERIFY(!result.error.message.trimmed().isEmpty());
        QVERIFY(result.error.message != QStringLiteral("helper failed"));
        QCOMPARE(result.error.detail, QStringLiteral("helper failed"));
    }

    void rejectsInvalidPcmWithoutCallingHelper_data()
    {
        QTest::addColumn<QByteArray>("audio");
        QTest::addColumn<QString>("format");
        QTest::addColumn<int>("sampleRate");

        QTest::newRow("empty") << QByteArray() << QStringLiteral("pcm") << 16000;
        QTest::newRow("odd-16-bit") << QByteArray::fromHex("01") << QStringLiteral("pcm") << 16000;
        QTest::newRow("wav") << QByteArray::fromHex("0102") << QStringLiteral("wav") << 16000;
        QTest::newRow("wrong-rate") << QByteArray::fromHex("0102") << QStringLiteral("pcm") << 8000;
    }

    void rejectsInvalidPcmWithoutCallingHelper()
    {
        QFETCH(QByteArray, audio);
        QFETCH(QString, format);
        QFETCH(int, sampleRate);
        int calls = 0;
        WindowsSpeechProvider provider(
            [&calls](const WindowsSpeechBatchRequest &) {
                ++calls;
                return WindowsSpeechHelperResult();
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = audio;
        request.audioFormat = format;
        request.sampleRate = sampleRate;

        const SpeechRecognitionResult result = provider.recognize(
            request, CancellationToken()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.invalid_audio"));
        QCOMPARE(calls, 0);
    }

    void mapsCancellationBeforeAndAfterHelper()
    {
        CancellationSource before;
        before.cancel();
        int calls = 0;
        WindowsSpeechProvider provider(
            [&calls](const WindowsSpeechBatchRequest &) {
                ++calls;
                WindowsSpeechHelperResult result;
                result.ok = true;
                result.text = QStringLiteral("late");
                return result;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = QByteArray::fromHex("0102");
        request.audioFormat = QStringLiteral("pcm");
        request.sampleRate = 16000;

        SpeechRecognitionResult result = provider.recognize(request, before.token());
        QCOMPARE(result.error.code, QStringLiteral("operation.cancelled"));
        QCOMPARE(calls, 0);

        CancellationSource after;
        WindowsSpeechProvider cancellingProvider(
            [&after](const WindowsSpeechBatchRequest &) {
                after.cancel();
                WindowsSpeechHelperResult helper;
                helper.ok = true;
                helper.text = QStringLiteral("must not escape");
                return helper;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        result = cancellingProvider.recognize(request, after.token());
        QCOMPARE(result.error.code, QStringLiteral("operation.cancelled"));
        QVERIFY(result.text.isEmpty());
    }

    void rejectsEmptySuccessfulFinal()
    {
        WindowsSpeechProvider provider(
            [](const WindowsSpeechBatchRequest &) {
                WindowsSpeechHelperResult result;
                result.ok = true;
                return result;
            },
            WindowsSpeechProvider::ProbeFunction()
        );
        SpeechRecognitionRequest request;
        request.audioData = QByteArray::fromHex("0102");
        request.audioFormat = QStringLiteral("pcm");
        request.sampleRate = 16000;

        const SpeechRecognitionResult result = provider.recognize(
            request, CancellationToken()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_result"));
        QVERIFY(result.text.isEmpty());
    }
};

QTEST_MAIN(WindowsSpeechProviderTests)
#include "windows_speech_provider_tests.moc"
