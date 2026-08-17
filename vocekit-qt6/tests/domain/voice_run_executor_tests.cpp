#include <QtTest>

#include "../../src/domain/voice_run_executor.h"

class VoiceRunExecutorTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTranslateModelRequest()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("translate");
        context.selectedText = QStringLiteral("Hello");
        context.voiceText = QStringLiteral("使用正式语气");

        VoiceRunExecutionRequest request;
        request.context = context;
        request.modelOverride = QStringLiteral("model-b");
        request.extraInstruction = QStringLiteral("保留格式");
        CancellationSource cancellation;
        request.cancellation = cancellation.token();

        VoiceModelProcessingRequest captured;
        bool modelCalled = false;
        bool metricsRecorded = false;
        VoiceRunExecutionHandlers handlers;
        handlers.runtimeSettings = [](const QString &modeId) {
            VoiceModelRuntimeSettings runtime;
            runtime.defaultModel = QStringLiteral("model-a");
            runtime.systemPrompt = modeId + QStringLiteral("-prompt");
            runtime.useSystemProxy = true;
            return runtime;
        };
        handlers.vocabularyPromptBlockBuilder = [](
            const QString &modeId,
            const QString &userText,
            bool hasVoiceInput
        ) {
            return modeId + QStringLiteral("|")
                + userText + QStringLiteral("|")
                + (hasVoiceInput ? QStringLiteral("voice")
                                 : QStringLiteral("text"));
        };
        handlers.processModelRequest =
            [&captured, &modelCalled](
                const VoiceModelProcessingRequest &modelRequest
            ) {
                captured = modelRequest;
                modelCalled = true;
                VoiceModelProcessingResult result;
                result.text = QStringLiteral("translated");
                result.durationMs = 42;
                result.promptVersion = QStringLiteral("prompt-v2");
                return result;
            };
        handlers.modelResultRecorded =
            [&metricsRecorded](qint64 durationMs, const QString &version) {
                metricsRecorded =
                    durationMs == 42
                    && version == QStringLiteral("prompt-v2");
            };

        QString error;
        const QString output =
            VoiceRunExecutor::run(request, handlers, &error);

        QVERIFY(error.isEmpty());
        QCOMPARE(output, QStringLiteral("translated"));
        QVERIFY(modelCalled);
        QVERIFY(metricsRecorded);
        QCOMPARE(captured.modeId, QStringLiteral("translate"));
        QCOMPARE(captured.primaryText, QStringLiteral("Hello"));
        QCOMPARE(captured.selectedText, QStringLiteral("Hello"));
        QCOMPARE(
            captured.voiceInstruction,
            QStringLiteral("使用正式语气")
        );
        QCOMPARE(captured.modelOverride, QStringLiteral("model-b"));
        QCOMPARE(captured.extraInstruction, QStringLiteral("保留格式"));
        QVERIFY(captured.cancellation.isValid());
        QCOMPARE(
            captured.cancellation.executionId(),
            cancellation.executionId()
        );
        QVERIFY(!captured.cancellation.isCancellationRequested());
        QVERIFY(captured.hasVoiceInput);
        QCOMPARE(
            captured.runtime.systemPrompt,
            QStringLiteral("translate-prompt")
        );
        QVERIFY(captured.runtime.useSystemProxy);
        QVERIFY(captured.vocabularyPromptBlockBuilder);
        QCOMPARE(
            captured.vocabularyPromptBlockBuilder(
                captured.modeId,
                QStringLiteral("payload"),
                captured.hasVoiceInput
            ),
            QStringLiteral("translate|payload|voice")
        );
    }

    void buildsCustomTextOnlyModelRequest()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("custom_1");
        context.selectedText = QStringLiteral("原文");
        context.textOnly = true;
        context.textOnlyInput = QStringLiteral("用户要求");

        VoiceRunExecutionRequest request;
        request.context = context;

        VoiceModelProcessingRequest captured;
        VoiceRunExecutionHandlers handlers;
        handlers.runtimeSettings = [](const QString &modeId) {
            VoiceModelRuntimeSettings runtime;
            runtime.defaultModel = modeId + QStringLiteral("-model");
            return runtime;
        };
        handlers.processModelRequest =
            [&captured](const VoiceModelProcessingRequest &modelRequest) {
                captured = modelRequest;
                VoiceModelProcessingResult result;
                result.text = QStringLiteral("custom-result");
                return result;
            };

        QString error;
        QCOMPARE(
            VoiceRunExecutor::run(request, handlers, &error),
            QStringLiteral("custom-result")
        );
        QVERIFY(error.isEmpty());
        QCOMPARE(captured.modeId, QStringLiteral("custom_1"));
        QCOMPARE(captured.primaryText, QString());
        QCOMPARE(captured.selectedText, QStringLiteral("原文"));
        QVERIFY(!captured.hasVoiceInput);
        QCOMPARE(
            captured.runtime.defaultModel,
            QStringLiteral("custom_1-model")
        );
    }

    void propagatesModelErrorAndRecordsMetrics()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("ask");
        context.voiceText = QStringLiteral("问题");

        VoiceRunExecutionRequest request;
        request.context = context;

        qint64 recordedDuration = -1;
        QString recordedVersion;
        VoiceRunExecutionHandlers handlers;
        handlers.runtimeSettings = [](const QString &) {
            return VoiceModelRuntimeSettings();
        };
        handlers.processModelRequest =
            [](const VoiceModelProcessingRequest &) {
                VoiceModelProcessingResult result;
                result.errorMessage = QStringLiteral("network error");
                result.durationMs = 19;
                result.promptVersion = QStringLiteral("prompt-v3");
                return result;
            };
        handlers.modelResultRecorded =
            [&recordedDuration, &recordedVersion](
                qint64 durationMs,
                const QString &version
            ) {
                recordedDuration = durationMs;
                recordedVersion = version;
            };

        QString error;
        QVERIFY(VoiceRunExecutor::run(request, handlers, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("network error"));
        QCOMPARE(recordedDuration, qint64(19));
        QCOMPARE(recordedVersion, QStringLiteral("prompt-v3"));
    }

    void rejectsMissingModelProcessor()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("dictate");
        context.voiceText = QStringLiteral("hello");

        VoiceRunExecutionRequest request;
        request.context = context;

        QString error;
        QVERIFY(
            VoiceRunExecutor::run(
                request,
                VoiceRunExecutionHandlers(),
                &error
            ).isEmpty()
        );
        QCOMPARE(error, QString::fromUtf8("功能执行器未配置。"));
    }
};

QTEST_APPLESS_MAIN(VoiceRunExecutorTests)

#include "voice_run_executor_tests.moc"
