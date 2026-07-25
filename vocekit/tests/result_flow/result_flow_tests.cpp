#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/domain/voice_result_completion_executor.h"
#include "../../src/domain/voice_result_rerun_executor.h"
#include "../../src/domain/voice_result_stream_executor.h"
#include "../../src/domain/voice_function_execution_pipeline.h"
#include "../../src/domain/voice_input_processing_pipeline.h"
#include "../../src/result_flow_config.h"
#include "../../src/output/voice_result_popup_builder.h"
#include "../../src/output/voice_result_output_dispatcher.h"

class ResultFlowTests : public QObject
{
    Q_OBJECT

private slots:
    void executesFunctionInputThroughCompletion()
    {
        VoiceFunctionExecutionAccess access;
        access.selectedText = []() {
            return QStringLiteral("选中上下文");
        };
        access.networkPoliciesFor = [](const QString &modeId) {
            FunctionNetworkPolicies policies;
            policies.model = modeId == QStringLiteral("translate")
                ? QStringLiteral("direct")
                : QStringLiteral("inherit");
            return policies;
        };
        access.preCorrect = [](
            const QString &text,
            const QString &modeId,
            const QString &sourceLabel,
            bool hasVoiceInput
        ) {
            Q_UNUSED(modeId);
            Q_UNUSED(sourceLabel);
            Q_UNUSED(hasVoiceInput);
            return text == QStringLiteral("deep seek")
                ? QStringLiteral("DeepSeek")
                : text;
        };

        bool processingStarted = false;
        access.processingStarted = [&processingStarted](
            const QString &status,
            const VoiceRunContext &context
        ) {
            processingStarted =
                status == QStringLiteral("模型处理中")
                && context.modeId == QStringLiteral("translate");
        };
        access.completionHandlers = []() {
            VoiceResultCompletionHandlers handlers;
            handlers.runContext = [](
                const VoiceRunContext &context,
                QString *error
            ) {
                if (error) {
                    error->clear();
                }
                return context.voiceText + QStringLiteral("-raw");
            };
            handlers.finalizeOutput = [](
                const VoiceRunContext &context,
                const QString &output
            ) {
                Q_UNUSED(context);
                return output + QStringLiteral("-final");
            };
            return handlers;
        };

        bool contextUpdated = false;
        bool completed = false;
        access.contextUpdated = [&contextUpdated](
            const VoiceRunContext &context
        ) {
            contextUpdated =
                context.selectedText == QStringLiteral("选中上下文");
        };
        access.completed = [&completed](
            const VoiceRunContext &context,
            const QString &finalOutput
        ) {
            completed =
                context.networkPolicies.model == QStringLiteral("direct")
                && finalOutput == QStringLiteral("DeepSeek-raw-final");
        };

        VoiceFunctionExecutionPipeline pipeline(access);
        VoiceFunctionExecutionRequest request;
        request.modeId = QStringLiteral("translate");
        request.inputText = QStringLiteral("deep seek");
        request.kind = VoiceInputProcessingKind::Voice;
        request.sourceLabel = QStringLiteral("语音识别");
        request.failureStage = QStringLiteral("大模型");
        request.processingStatus = QStringLiteral("模型处理中");

        const VoiceFunctionExecutionResult result =
            pipeline.execute(request);

        QVERIFY(
            result.disposition
            == VoiceFunctionExecutionDisposition::Completed
        );
        QVERIFY(processingStarted);
        QVERIFY(contextUpdated);
        QVERIFY(completed);
        QCOMPARE(
            result.processing.context.voiceText,
            QStringLiteral("DeepSeek")
        );
    }

    void routesFunctionInputToStreamingTerminal()
    {
        VoiceFunctionExecutionAccess access;
        bool streamed = false;
        bool completionCalled = false;
        bool completed = false;
        access.shouldStream = [](const VoiceRunContext &context) {
            return context.modeId == QStringLiteral("ask");
        };
        access.stream = [&streamed](const VoiceRunContext &context) {
            streamed = context.textOnly
                && context.textOnlyInput == QStringLiteral("问题");
        };
        access.completionHandlers = [&completionCalled]() {
            VoiceResultCompletionHandlers handlers;
            handlers.runContext = [&completionCalled](
                const VoiceRunContext &context,
                QString *error
            ) {
                Q_UNUSED(context);
                Q_UNUSED(error);
                completionCalled = true;
                return QStringLiteral("unexpected");
            };
            return handlers;
        };
        access.completed = [&completed](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            Q_UNUSED(output);
            completed = true;
        };

        VoiceFunctionExecutionPipeline pipeline(access);
        VoiceFunctionExecutionRequest request;
        request.modeId = QStringLiteral("ask");
        request.inputText = QStringLiteral("问题");
        request.kind = VoiceInputProcessingKind::TextOnly;

        const VoiceFunctionExecutionResult result =
            pipeline.execute(request);

        QVERIFY(
            result.disposition
            == VoiceFunctionExecutionDisposition::Streamed
        );
        QVERIFY(streamed);
        QVERIFY(!completionCalled);
        QVERIFY(!completed);
    }

    void reportsFunctionExecutionFailure()
    {
        VoiceFunctionExecutionAccess access;
        access.completionHandlers = []() {
            VoiceResultCompletionHandlers handlers;
            handlers.runContext = [](
                const VoiceRunContext &context,
                QString *error
            ) {
                Q_UNUSED(context);
                if (error) {
                    *error = QStringLiteral("network failed");
                }
                return QString();
            };
            handlers.finalizeOutput = [](
                const VoiceRunContext &context,
                const QString &output
            ) {
                Q_UNUSED(context);
                return output;
            };
            return handlers;
        };

        bool failed = false;
        access.failed = [&failed](
            const VoiceRunContext &context,
            const VoiceResultCompletionResult &completion
        ) {
            failed =
                context.modeId == QStringLiteral("custom_1")
                && completion.error == QStringLiteral("network failed");
        };

        VoiceFunctionExecutionPipeline pipeline(access);
        VoiceFunctionExecutionRequest request;
        request.modeId = QStringLiteral("custom_1");
        request.inputText = QStringLiteral("input");
        request.kind = VoiceInputProcessingKind::TextOnly;
        request.failureStage = QStringLiteral("文本处理");

        const VoiceFunctionExecutionResult result =
            pipeline.execute(request);

        QVERIFY(
            result.disposition
            == VoiceFunctionExecutionDisposition::Failed
        );
        QVERIFY(failed);
        QCOMPARE(
            result.processing.completion.error,
            QStringLiteral("network failed")
        );
    }

    void processesVoiceInputThroughCompletion()
    {
        VoiceInputProcessingRequest request;
        request.modeId = QStringLiteral("dictate");
        request.selectedText = QStringLiteral("上下文");
        request.inputText = QStringLiteral("deep seek");
        request.sourceLabel = QStringLiteral("语音识别");
        request.kind = VoiceInputProcessingKind::Voice;
        request.failureStage = QStringLiteral("大模型");
        request.networkPolicies.model = QStringLiteral("direct");

        bool enriched = false;
        bool contextMatched = false;
        VoiceInputProcessingHandlers handlers;
        handlers.preCorrect = [](
            const QString &text,
            const QString &modeId,
            const QString &sourceLabel,
            bool hasVoiceInput
        ) {
            Q_UNUSED(modeId);
            Q_UNUSED(sourceLabel);
            Q_UNUSED(hasVoiceInput);
            return text == QStringLiteral("deep seek")
                ? QStringLiteral("DeepSeek")
                : text;
        };
        handlers.enrichContext = [&enriched](VoiceRunContext *context) {
            enriched = true;
            context->screenshotInput = true;
        };
        handlers.completion.runContext = [&contextMatched](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(error);
            contextMatched = context.modeId == QStringLiteral("dictate")
                && context.voiceText == QStringLiteral("DeepSeek")
                && context.selectedText == QStringLiteral("上下文")
                && !context.textOnly
                && context.screenshotInput
                && context.networkPolicies.model == QStringLiteral("direct");
            return QStringLiteral("raw");
        };
        handlers.completion.finalizeOutput = [](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            return output + QStringLiteral("-final");
        };

        const VoiceInputProcessingResult result =
            VoiceInputProcessingPipeline::run(request, handlers);
        QVERIFY(result.ok);
        QVERIFY(!result.streamed);
        QVERIFY(enriched);
        QVERIFY(contextMatched);
        QCOMPARE(result.correctedText, QStringLiteral("DeepSeek"));
        QCOMPARE(result.completion.finalOutput, QStringLiteral("raw-final"));
    }

    void routesVoiceInputToStreamingWithoutCompletion()
    {
        VoiceInputProcessingRequest request;
        request.modeId = QStringLiteral("translate");
        request.inputText = QStringLiteral("hello");
        request.kind = VoiceInputProcessingKind::Voice;

        bool streamed = false;
        bool completionCalled = false;
        VoiceInputProcessingHandlers handlers;
        handlers.shouldStream = [](const VoiceRunContext &context) {
            return context.modeId == QStringLiteral("translate");
        };
        handlers.stream = [&streamed](const VoiceRunContext &context) {
            streamed = context.voiceText == QStringLiteral("hello");
        };
        handlers.completion.runContext = [&completionCalled](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(context);
            Q_UNUSED(error);
            completionCalled = true;
            return QStringLiteral("unexpected");
        };

        const VoiceInputProcessingResult result =
            VoiceInputProcessingPipeline::run(request, handlers);
        QVERIFY(result.ok);
        QVERIFY(result.streamed);
        QVERIFY(streamed);
        QVERIFY(!completionCalled);
    }

    void buildsTextOnlyProcessingContext()
    {
        VoiceInputProcessingRequest request;
        request.modeId = QStringLiteral("custom_1");
        request.inputText = QStringLiteral("文本输入");
        request.kind = VoiceInputProcessingKind::TextOnly;

        bool contextMatched = false;
        VoiceInputProcessingHandlers handlers;
        handlers.completion.runContext = [&contextMatched](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(error);
            contextMatched = context.textOnly
                && context.textOnlyInput == QStringLiteral("文本输入")
                && context.voiceText.isEmpty();
            return QStringLiteral("result");
        };
        handlers.completion.finalizeOutput = [](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            return output;
        };

        const VoiceInputProcessingResult result =
            VoiceInputProcessingPipeline::run(request, handlers);
        QVERIFY(result.ok);
        QVERIFY(contextMatched);
    }

    void normalizesResultActions()
    {
        const QStringList normalized = normalizeResultActionIds(
            QStringList()
                << QStringLiteral("copy")
                << QStringLiteral("unknown")
                << QStringLiteral("copy")
                << QStringLiteral("write")
        );

        QCOMPARE(
            normalized,
            QStringList() << QStringLiteral("copy") << QStringLiteral("write")
        );
        QCOMPARE(
            normalizeResultActionIds(QStringList()),
            defaultResultActionIds()
        );
    }

    void normalizesNetworkPolicies()
    {
        QCOMPARE(
            normalizeNetworkPolicy(QStringLiteral("direct")),
            QStringLiteral("direct")
        );
        QCOMPARE(
            normalizeNetworkPolicy(QStringLiteral("systemProxy")),
            QStringLiteral("systemProxy")
        );
        QCOMPARE(
            normalizeNetworkPolicy(QStringLiteral("anything")),
            QStringLiteral("inherit")
        );
        QCOMPARE(
            resolveNetworkPolicy(QStringLiteral("inherit"), true),
            QStringLiteral("systemProxy")
        );
        QCOMPARE(
            resolveNetworkPolicy(QStringLiteral("inherit"), false),
            QStringLiteral("direct")
        );
    }

    void classifiesStreamFallbackErrors()
    {
        QVERIFY(shouldFallbackFromStreamFailure(
            QStringLiteral("网络请求超时。"),
            0,
            false
        ));
        QVERIFY(shouldFallbackFromStreamFailure(
            QStringLiteral("Connection closed"),
            0,
            false
        ));
        QVERIFY(!shouldFallbackFromStreamFailure(
            QStringLiteral("接口认证失败"),
            401,
            false
        ));
        QVERIFY(!shouldFallbackFromStreamFailure(
            QStringLiteral("Too many requests"),
            429,
            false
        ));
        QVERIFY(!shouldFallbackFromStreamFailure(
            QStringLiteral("CANCELLED"),
            0,
            true
        ));
    }

    void plansVoiceResultOutput()
    {
        VoiceResultOutputRequest request;
        request.modeId = QStringLiteral("translate");
        request.outputMode = outputModeAutoWrite();
        request.finalOutput = QStringLiteral("abcdef");
        request.hasSelectedText = true;

        const VoiceResultOutputDispatch autoWrite =
            VoiceResultOutputDispatcher::plan(request);

        QCOMPARE(
            autoWrite.routePlan.destination,
            ResultOutputDestination::AutoWrite
        );
        QVERIFY(autoWrite.routePlan.replaceSelectedText);
        QVERIFY(autoWrite.completionLogDetail.contains(QStringLiteral("translate")));
        QVERIFY(autoWrite.completionLogDetail.contains(QStringLiteral("6")));
        QVERIFY(autoWrite.autoWriteLogDetail.contains(QStringLiteral("6")));

        request.outputMode = outputModeScreenshotPanel();
        request.screenshotInput = true;
        request.hasSelectedText = false;

        const VoiceResultOutputDispatch screenshotPanel =
            VoiceResultOutputDispatcher::plan(request);

        QCOMPARE(
            screenshotPanel.routePlan.destination,
            ResultOutputDestination::ScreenshotPanel
        );
        QVERIFY(!screenshotPanel.routePlan.replaceSelectedText);
    }

    void buildsVoiceResultPopupPresentation()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("ask");
        context.selectedText = QStringLiteral("source text");
        context.voiceText = QStringLiteral("question");

        VoiceResultPopupBuildRequest request;
        request.context = context;
        request.output = QStringLiteral("answer");
        request.functionTitle = QStringLiteral("Ask");
        request.modelId = QStringLiteral("deepseek-v4-pro");
        request.modelTitle = QStringLiteral("DeepSeek");
        request.templateId = resultTemplateDetail();
        request.elapsedMs = 1234;
        request.timeoutMs = 5000;

        const VoiceResultPopupPresentation presentation =
            VoiceResultPopupBuilder::build(request);

        QCOMPARE(presentation.title, QStringLiteral("Ask"));
        QCOMPARE(presentation.currentModel, QStringLiteral("deepseek-v4-pro"));
        QVERIFY(presentation.hasSelectedText);
        QCOMPARE(presentation.timeoutMs, 5000);
        QVERIFY(presentation.text.contains(QStringLiteral("Ask")));
        QVERIFY(presentation.text.contains(QStringLiteral("DeepSeek")));
        QVERIFY(presentation.text.contains(QStringLiteral("answer")));
    }

    void rerunsVoiceResultWithCallbacks()
    {
        VoiceResultRerunRequest request;
        request.context.modeId = QStringLiteral("ask");
        request.defaultModel = QStringLiteral("default-model");
        request.extraInstruction = QStringLiteral("continue");

        bool finalized = false;
        VoiceResultRerunHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            const QString &modelOverride,
            const QString &extraInstruction,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            Q_UNUSED(modelOverride);
            if (error) {
                error->clear();
            }
            if (onDelta) {
                onDelta(QStringLiteral("delta"));
            }
            return QStringLiteral("raw-") + extraInstruction;
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            finalized = true;
            return context.modeId + QStringLiteral(":") + output;
        };

        int deltaCount = 0;
        request.onDelta = [&deltaCount](const QString &delta) {
            if (delta == QStringLiteral("delta")) {
                ++deltaCount;
            }
        };

        const VoiceResultRerunResult result =
            VoiceResultRerunExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QVERIFY(finalized);
        QCOMPARE(deltaCount, 1);
        QCOMPARE(result.finalModel, QStringLiteral("default-model"));
        QCOMPARE(result.rawOutput, QStringLiteral("raw-continue"));
        QCOMPARE(result.finalOutput, QStringLiteral("ask:raw-continue"));
        QVERIFY(result.logDetail.contains(QStringLiteral("ask")));
        QVERIFY(result.logDetail.contains(QStringLiteral("default-model")));
    }

    void reportsVoiceResultRerunFailure()
    {
        VoiceResultRerunRequest request;
        request.context.modeId = QStringLiteral("translate");
        request.modelOverride = QStringLiteral("retry-model");
        request.defaultModel = QStringLiteral("default-model");

        bool finalized = false;
        VoiceResultRerunHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            const QString &modelOverride,
            const QString &extraInstruction,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            Q_UNUSED(modelOverride);
            Q_UNUSED(extraInstruction);
            Q_UNUSED(onDelta);
            if (error) {
                *error = QStringLiteral("network failed");
            }
            return QString();
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            Q_UNUSED(output);
            finalized = true;
            return QStringLiteral("should-not-run");
        };

        const VoiceResultRerunResult result =
            VoiceResultRerunExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(!finalized);
        QCOMPARE(result.finalModel, QStringLiteral("retry-model"));
        QCOMPARE(result.error, QStringLiteral("network failed"));
        QVERIFY(result.logDetail.contains(QStringLiteral("translate")));
        QVERIFY(result.logDetail.contains(QStringLiteral("network failed")));
    }

    void treatsCancelledVoiceResultRerunSeparately()
    {
        VoiceResultRerunRequest request;
        request.context.modeId = QStringLiteral("translate");

        VoiceResultRerunHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            const QString &modelOverride,
            const QString &extraInstruction,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            Q_UNUSED(modelOverride);
            Q_UNUSED(extraInstruction);
            Q_UNUSED(onDelta);
            if (error) {
                *error = QStringLiteral("请求已取消。");
            }
            return QString();
        };
        handlers.wasCancelled = []() {
            return true;
        };
        handlers.finalizeOutput = [](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            return output;
        };

        const VoiceResultRerunResult result =
            VoiceResultRerunExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(result.cancelled);
        QVERIFY(result.logAction.contains(QStringLiteral("取消")));
    }

    void streamsVoiceResultWithCallbacks()
    {
        VoiceResultStreamRequest request;
        request.context.modeId = QStringLiteral("translate");

        int deltaCount = 0;
        request.onDelta = [&deltaCount](const QString &delta) {
            if (delta == QStringLiteral("piece")) {
                ++deltaCount;
            }
        };

        bool finalized = false;
        VoiceResultStreamHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            if (error) {
                error->clear();
            }
            if (onDelta) {
                onDelta(QStringLiteral("piece"));
            }
            return QStringLiteral("raw-output");
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            finalized = true;
            return context.modeId + QStringLiteral(":") + output;
        };

        const VoiceResultStreamResult result =
            VoiceResultStreamExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QVERIFY(finalized);
        QCOMPARE(deltaCount, 1);
        QCOMPARE(result.rawOutput, QStringLiteral("raw-output"));
        QCOMPARE(result.finalOutput, QStringLiteral("translate:raw-output"));
        QVERIFY(result.logDetail.contains(QStringLiteral("translate")));
        QVERIFY(result.logDetail.contains(QString::number(result.finalOutput.size())));
    }

    void reportsVoiceResultStreamFailure()
    {
        VoiceResultStreamRequest request;
        request.context.modeId = QStringLiteral("ask");

        bool finalized = false;
        VoiceResultStreamHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            Q_UNUSED(onDelta);
            if (error) {
                *error = QStringLiteral("stream failed");
            }
            return QString();
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            Q_UNUSED(output);
            finalized = true;
            return QStringLiteral("should-not-run");
        };

        const VoiceResultStreamResult result =
            VoiceResultStreamExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(!finalized);
        QCOMPARE(result.error, QStringLiteral("stream failed"));
        QVERIFY(result.logDetail.contains(QStringLiteral("ask")));
        QVERIFY(result.logDetail.contains(QStringLiteral("stream failed")));
    }

    void treatsCancelledVoiceResultStreamSeparately()
    {
        VoiceResultStreamRequest request;
        request.context.modeId = QStringLiteral("ask");

        VoiceResultStreamHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            Q_UNUSED(context);
            Q_UNUSED(onDelta);
            if (error) {
                *error = QStringLiteral("请求已取消。");
            }
            return QString();
        };
        handlers.wasCancelled = []() {
            return true;
        };
        handlers.finalizeOutput = [](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            return output;
        };

        const VoiceResultStreamResult result =
            VoiceResultStreamExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(result.cancelled);
        QVERIFY(result.logAction.contains(QStringLiteral("取消")));
    }

    void completesVoiceResultWithCallbacks()
    {
        VoiceResultCompletionRequest request;
        request.context.modeId = QStringLiteral("dictate");
        request.failureStage = QStringLiteral("model");

        bool finalized = false;
        VoiceResultCompletionHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(context);
            if (error) {
                error->clear();
            }
            return QStringLiteral("raw text");
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            finalized = true;
            return context.modeId + QStringLiteral(":") + output;
        };

        const VoiceResultCompletionResult result =
            VoiceResultCompletionExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QVERIFY(finalized);
        QCOMPARE(result.rawOutput, QStringLiteral("raw text"));
        QCOMPARE(result.finalOutput, QStringLiteral("dictate:raw text"));
        QVERIFY(result.logDetail.contains(QStringLiteral("dictate")));
        QVERIFY(result.logDetail.contains(QString::number(result.finalOutput.size())));
    }

    void reportsVoiceResultCompletionFailure()
    {
        VoiceResultCompletionRequest request;
        request.context.modeId = QStringLiteral("translate");
        request.failureStage = QStringLiteral("text");

        bool finalized = false;
        VoiceResultCompletionHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(context);
            if (error) {
                *error = QStringLiteral("model failed");
            }
            return QString();
        };
        handlers.finalizeOutput = [&finalized](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            Q_UNUSED(output);
            finalized = true;
            return QStringLiteral("should-not-run");
        };

        const VoiceResultCompletionResult result =
            VoiceResultCompletionExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(!finalized);
        QCOMPARE(result.error, QStringLiteral("model failed"));
        QVERIFY(result.logDetail.contains(QStringLiteral("translate")));
        QVERIFY(result.logDetail.contains(QStringLiteral("model failed")));
    }

    void treatsCancelledVoiceResultCompletionSeparately()
    {
        VoiceResultCompletionRequest request;
        request.context.modeId = QStringLiteral("dictate");
        request.failureStage = QStringLiteral("model");

        VoiceResultCompletionHandlers handlers;
        handlers.runContext = [](
            const VoiceRunContext &context,
            QString *error
        ) {
            Q_UNUSED(context);
            if (error) {
                *error = QStringLiteral("请求已取消。");
            }
            return QString();
        };
        handlers.wasCancelled = []() {
            return true;
        };
        handlers.finalizeOutput = [](
            const VoiceRunContext &context,
            const QString &output
        ) {
            Q_UNUSED(context);
            return output;
        };

        const VoiceResultCompletionResult result =
            VoiceResultCompletionExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(result.cancelled);
        QVERIFY(result.logAction.contains(QStringLiteral("取消")));
    }

    void roundTripsRecoveryState()
    {
        ResultRecoveryState state;
        state.modeId = QStringLiteral("translate");
        state.functionTitle = QStringLiteral("翻译");
        state.selectedText = QStringLiteral("hello");
        state.generatedText = QStringLiteral("你好");
        state.model = QStringLiteral("deepseek-v4-flash");
        state.promptId = QStringLiteral("translate");
        state.stage = QStringLiteral("resultPending");
        state.networkPolicies.speech = QStringLiteral("direct");
        state.networkPolicies.ocr = QStringLiteral("systemProxy");
        state.networkPolicies.model = QStringLiteral("inherit");
        state.createdAt = QDateTime::fromString(
            QStringLiteral("2026-06-23T10:00:00.000"),
            Qt::ISODateWithMs
        );
        state.updatedAt = state.createdAt;

        const ResultRecoveryState restored =
            resultRecoveryStateFromJson(resultRecoveryStateToJson(state));

        QVERIFY(restored.valid);
        QCOMPARE(restored.modeId, state.modeId);
        QCOMPARE(restored.generatedText, state.generatedText);
        QCOMPARE(restored.networkPolicies.speech, QStringLiteral("direct"));
        QCOMPARE(restored.networkPolicies.ocr, QStringLiteral("systemProxy"));
        QCOMPARE(restored.networkPolicies.model, QStringLiteral("inherit"));
    }

    void savesLoadsAndClearsRecoveryFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("recovery.json"));

        ResultRecoveryState state;
        state.valid = true;
        state.modeId = QStringLiteral("ask");
        state.functionTitle = QStringLiteral("问答");
        state.generatedText = QStringLiteral("未完成内容");
        state.stage = QStringLiteral("streaming");
        state.createdAt = QDateTime::currentDateTime();
        state.updatedAt = state.createdAt;

        QString error;
        QVERIFY2(saveResultRecoveryState(path, state, &error), qPrintable(error));

        ResultRecoveryState loaded;
        QVERIFY2(loadResultRecoveryState(path, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.generatedText, state.generatedText);

        QVERIFY2(clearResultRecoveryState(path, &error), qPrintable(error));
        QVERIFY(!QFileInfo::exists(path));
    }
};

QTEST_MAIN(ResultFlowTests)
#include "result_flow_tests.moc"
