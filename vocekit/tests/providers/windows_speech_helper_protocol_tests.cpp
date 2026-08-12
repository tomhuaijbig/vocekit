#include <QtTest>

#include "../../src/providers/windows_speech_helper_protocol.h"

class WindowsSpeechHelperProtocolTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsRuntimePathWithoutStrippingReleaseOrDebug()
    {
        QCOMPARE(
            windowsSpeechHelperPathForApplicationDir(
                QStringLiteral("C:/VoceKit/release")
            ),
            QStringLiteral(
                "C:/VoceKit/release/speech/windows/"
                "vocekit-windows-speech.exe"
            )
        );
        QCOMPARE(
            windowsSpeechHelperPathForApplicationDir(
                QStringLiteral("C:/VoceKit/debug")
            ),
            QStringLiteral(
                "C:/VoceKit/debug/speech/windows/"
                "vocekit-windows-speech.exe"
            )
        );
    }

    void buildsExactRecognitionArguments()
    {
        QCOMPARE(
            windowsSpeechHelperArguments(
                QStringLiteral("batch"),
                QStringLiteral("run-1"),
                QStringLiteral("zh-CN"),
                16000,
                1,
                16
            ),
            QStringList()
                << QStringLiteral("--mode")
                << QStringLiteral("batch")
                << QStringLiteral("--run-id")
                << QStringLiteral("run-1")
                << QStringLiteral("--language")
                << QStringLiteral("zh-CN")
                << QStringLiteral("--sample-rate")
                << QStringLiteral("16000")
                << QStringLiteral("--channels")
                << QStringLiteral("1")
                << QStringLiteral("--bits")
                << QStringLiteral("16")
        );
    }

    void parsesEverySupportedEvent()
    {
        struct Example {
            const char *json;
            WindowsSpeechHelperEventType type;
        };
        const Example examples[] = {
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"ready\",\"ok\":true,\"resolvedLanguage\":\"zh-CN\",\"mode\":\"batch\"}", WindowsSpeechHelperEventType::Ready},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"hypothesis\",\"ok\":true,\"text\":\"h\"}", WindowsSpeechHelperEventType::Hypothesis},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"recognized\",\"ok\":true,\"text\":\"hello\",\"confidence\":0.75}", WindowsSpeechHelperEventType::Recognized},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"final\",\"ok\":true,\"text\":\"hello\",\"inputStreamEnded\":true,\"pcmBytes\":42}", WindowsSpeechHelperEventType::Final},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"probe\",\"ok\":true,\"resolvedLanguage\":\"en-US\",\"installedLanguages\":[\"en-US\",\"zh-CN\"]}", WindowsSpeechHelperEventType::Probe},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"error\",\"ok\":false,\"errorCode\":\"NO_SPEECH\",\"message\":\"No speech.\",\"inputStreamEnded\":true}", WindowsSpeechHelperEventType::Error},
            {"{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"self-test\",\"ok\":true,\"tests\":[\"arguments-and-protocol\"]}", WindowsSpeechHelperEventType::SelfTest}
        };

        for (const Example &example : examples) {
            const WindowsSpeechHelperEvent event =
                parseWindowsSpeechHelperEvent(QByteArray(example.json));
            QVERIFY2(event.valid, example.json);
            QVERIFY(event.type == example.type);
            QCOMPARE(event.runId, QStringLiteral("r"));
        }

        const WindowsSpeechHelperEvent finalEvent =
            parseWindowsSpeechHelperEvent(QByteArray(examples[3].json));
        QCOMPARE(finalEvent.text, QStringLiteral("hello"));
        QVERIFY(finalEvent.inputStreamEnded);
        QCOMPARE(finalEvent.pcmBytesObserved, qint64(42));

        const WindowsSpeechHelperEvent probeEvent =
            parseWindowsSpeechHelperEvent(QByteArray(examples[4].json));
        QCOMPARE(probeEvent.resolvedLanguage, QStringLiteral("en-US"));
        QCOMPARE(
            probeEvent.installedLanguages,
            QStringList() << QStringLiteral("en-US")
                          << QStringLiteral("zh-CN")
        );

        const WindowsSpeechHelperEvent errorEvent =
            parseWindowsSpeechHelperEvent(QByteArray(examples[5].json));
        QCOMPARE(errorEvent.errorCode, QStringLiteral("NO_SPEECH"));
        QCOMPARE(errorEvent.errorMessage, QStringLiteral("No speech."));
    }

    void acceptsUtf8TranscriptAndEmptyFinalForClientValidation()
    {
        const WindowsSpeechHelperEvent utf8 = parseWindowsSpeechHelperEvent(
            QByteArray("{\"protocolVersion\":1,\"runId\":\"r1\","
                       "\"type\":\"final\",\"text\":\"")
                + QString::fromUtf8("你好").toUtf8()
                + QByteArray("\"}")
        );
        QVERIFY(utf8.valid);
        QCOMPARE(utf8.text, QString::fromUtf8("你好"));

        const WindowsSpeechHelperEvent empty = parseWindowsSpeechHelperEvent(
            QByteArray("{\"protocolVersion\":1,\"runId\":\"r1\","
                       "\"type\":\"final\",\"text\":\"\"}")
        );
        QVERIFY(empty.valid);
        QVERIFY(empty.text.isEmpty());
    }

    void rejectsEnvelopeBoundaries()
    {
        const QList<QByteArray> invalid = QList<QByteArray>()
            << QByteArray()
            << QByteArray("null")
            << QByteArray("[]")
            << QByteArray("not-json")
            << QByteArray("{\"protocolVersion\":2,\"runId\":\"r\",\"type\":\"ready\"}")
            << QByteArray("{\"protocolVersion\":\"1\",\"runId\":\"r\",\"type\":\"ready\"}")
            << QByteArray("{\"protocolVersion\":1,\"type\":\"ready\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"  \",\"type\":\"ready\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"future\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"ready\"}\n")
            << QByteArray("{\"protocolVersion\":1,\r\"runId\":\"r\",\"type\":\"ready\"}");

        for (const QByteArray &line : invalid) {
            QVERIFY2(!parseWindowsSpeechHelperEvent(line).valid, line.constData());
        }

        QByteArray invalidUtf8(
            "{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"ready\","
            "\"extra\":\""
        );
        invalidUtf8.append(char(0xc0));
        invalidUtf8.append(char(0xaf));
        invalidUtf8.append("\"}");
        QVERIFY(!parseWindowsSpeechHelperEvent(invalidUtf8).valid);

        QByteArray oversized(64 * 1024 + 1, ' ');
        QVERIFY(!parseWindowsSpeechHelperEvent(oversized).valid);
    }

    void acceptsLineAtMaximumSize()
    {
        QByteArray line(
            "{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"ready\","
            "\"padding\":\""
        );
        line.append(QByteArray(64 * 1024 - line.size() - 2, 'x'));
        line.append("\"}");
        QCOMPARE(line.size(), 64 * 1024);
        QVERIFY(parseWindowsSpeechHelperEvent(line).valid);
    }

    void rejectsMissingOrWrongEventFields()
    {
        const QList<QByteArray> invalid = QList<QByteArray>()
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"hypothesis\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"recognized\",\"text\":4}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"final\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"final\",\"text\":\"x\",\"inputStreamEnded\":\"yes\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"error\",\"message\":\"bad\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"error\",\"errorCode\":\"E\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"error\",\"errorCode\":3,\"message\":\"bad\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"probe\",\"installedLanguages\":[]}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"probe\",\"resolvedLanguage\":\"en-US\"}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"probe\",\"resolvedLanguage\":\"en-US\",\"installedLanguages\":[1]}")
            << QByteArray("{\"protocolVersion\":1,\"runId\":\"r\",\"type\":\"final\",\"text\":\"x\",\"pcmBytes\":\"42\"}");

        for (const QByteArray &line : invalid) {
            QVERIFY2(!parseWindowsSpeechHelperEvent(line).valid, line.constData());
        }
    }

    void mapsStableOperationErrors()
    {
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("PROGRAM_MISSING")), QStringLiteral("speech.windows.program_missing"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("RECOGNIZER_MISSING")), QStringLiteral("speech.windows.recognizer_missing"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("SYSTEM_SPEECH_UNAVAILABLE")), QStringLiteral("speech.windows.runtime_missing"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("GRAMMAR_LOAD_FAILED")), QStringLiteral("speech.windows.grammar_load_failed"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("NO_SPEECH")), QStringLiteral("speech.empty_result"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("CANCELLED")), QStringLiteral("operation.cancelled"));
        QCOMPARE(windowsSpeechOperationErrorCode(QStringLiteral("LOCAL_FAILURE")), QStringLiteral("speech.windows.local"));
        QCOMPARE(windowsSpeechOperationErrorCode(QString()), QStringLiteral("speech.windows.local"));
    }

    void identifiesOnlyConfigurationErrors()
    {
        const QStringList configCodes = QStringList()
            << QStringLiteral("speech.windows.program_missing")
            << QStringLiteral("speech.windows.recognizer_missing")
            << QStringLiteral("speech.windows.runtime_missing")
            << QStringLiteral("speech.windows.grammar_load_failed");
        for (const QString &code : configCodes) {
            QVERIFY(isWindowsSpeechConfigurationErrorCode(code));
        }
        QVERIFY(!isWindowsSpeechConfigurationErrorCode(QStringLiteral("speech.empty_result")));
        QVERIFY(!isWindowsSpeechConfigurationErrorCode(QStringLiteral("operation.cancelled")));
        QVERIFY(!isWindowsSpeechConfigurationErrorCode(QStringLiteral("speech.windows.local")));
    }

    void joinsRecognizedSegmentsWithoutDamagingLanguageBoundaries()
    {
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QStringLiteral("hello"), QStringLiteral("world")
            ),
            QStringLiteral("hello world")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QStringLiteral("version2"), QStringLiteral("beta3")
            ),
            QStringLiteral("version2 beta3")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QString::fromUtf8("你好"), QString::fromUtf8("世界")
            ),
            QString::fromUtf8("你好世界")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QStringLiteral("hello "), QStringLiteral("world")
            ),
            QStringLiteral("hello world")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QStringLiteral("hello"), QStringLiteral(" world")
            ),
            QStringLiteral("hello world")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QString(), QStringLiteral("first")
            ),
            QStringLiteral("first")
        );
        QCOMPARE(
            appendWindowsSpeechRecognizedSegment(
                QStringLiteral("keep"), QString()
            ),
            QStringLiteral("keep")
        );
    }
};

QTEST_APPLESS_MAIN(WindowsSpeechHelperProtocolTests)

#include "windows_speech_helper_protocol_tests.moc"
