#include <QtTest>

#include "../../src/domain/execution_types.h"
#include "../../src/domain/function_settings.h"
#include "../../src/domain/history_record_builder.h"
#include "../../src/domain/history_types.h"
#include "../../src/domain/voice_run_formatter.h"
#include "../../src/domain/voice_run_context.h"
#include "../../src/domain/voice_run_planner.h"
#include "../../src/output/result_output_router.h"

#include <QJsonArray>

class DomainTypesTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizesFunctionSettings()
    {
        FunctionSettings settings;
        settings.id = QStringLiteral(" dictate ");
        settings.name = QStringLiteral(" 听写 ");
        settings.recording.segmentSeconds = 2;
        settings.recording.maximumMinutes = 100;
        settings.recording.countdownSeconds = -4;
        settings.output.floatingBarSeconds = -1;
        settings.output.resultPopupSeconds = -3;
        settings.network.speech = QStringLiteral("unexpected");
        settings.network.ocr = QStringLiteral("direct");
        settings.network.model = QStringLiteral("systemProxy");

        const FunctionSettings normalized =
            normalizeFunctionSettings(settings);

        QCOMPARE(normalized.id, QStringLiteral("dictate"));
        QCOMPARE(normalized.name, QStringLiteral("听写"));
        QCOMPARE(normalized.recording.segmentSeconds, 20);
        QCOMPARE(normalized.recording.maximumMinutes, 30);
        QCOMPARE(normalized.recording.countdownSeconds, 0);
        QCOMPARE(normalized.output.floatingBarSeconds, 0);
        QCOMPARE(normalized.output.resultPopupSeconds, 0);
        QCOMPARE(normalized.network.speech, QStringLiteral("inherit"));
        QCOMPARE(normalized.network.ocr, QStringLiteral("direct"));
        QCOMPARE(normalized.network.model, QStringLiteral("systemProxy"));
    }

    void usesSafeFunctionDefaults()
    {
        const FunctionSettings settings;

        QCOMPARE(settings.input.useSelection, false);
        QCOMPARE(settings.input.useVoice, false);
        QCOMPARE(settings.input.useScreenshot, false);
        QCOMPARE(
            settings.recording.triggerMode,
            QStringLiteral("toggle")
        );
        QCOMPARE(settings.recording.segmentSeconds, 55);
        QCOMPARE(settings.recording.maximumMinutes, 30);
        QCOMPARE(
            settings.output.outputMode,
            QStringLiteral("resultPopup")
        );
        QCOMPARE(
            settings.output.resultTemplate,
            QStringLiteral("simple")
        );
    }

    void operationErrorReportsWhetherItIsEmpty()
    {
        OperationError error;
        QVERIFY(error.isEmpty());
        error.code = QStringLiteral("network.timeout");
        QVERIFY(!error.isEmpty());
        error.code.clear();
        error.message = QStringLiteral("网络请求超时");
        QVERIFY(!error.isEmpty());
    }

    void executionTimingCalculatesTotal()
    {
        ExecutionTiming timing;
        timing.inputMs = 10;
        timing.speechMs = 20;
        timing.ocrMs = 30;
        timing.modelMs = 40;
        timing.writeMs = 50;
        QCOMPARE(timing.totalFromStages(), qint64(150));
    }

    void historyTypesUseSafeDefaults()
    {
        const HistoryEntry entry;
        QCOMPARE(entry.elapsedMs, qint64(-1));
        QCOMPARE(entry.speechElapsedMs, qint64(-1));
        QCOMPARE(entry.modelElapsedMs, qint64(-1));
        QCOMPARE(entry.ocrElapsedMs, qint64(-1));
        QCOMPARE(entry.longRecording, false);
        QCOMPARE(entry.favorite, false);
        QCOMPARE(entry.draft, false);

        const HistoryQuery query;
        QCOMPARE(query.offset, 0);
        QCOMPARE(query.limit, 25);

        const HistoryQueryResult result;
        QCOMPARE(result.total, 0);
        QVERIFY(result.records.isEmpty());
    }

    void voiceRunContextReportsAvailableInputs()
    {
        VoiceRunContext context;
        QVERIFY(!context.hasSelectedText());
        QVERIFY(!context.hasVoiceText());
        QVERIFY(!context.hasTextOnlyInput());
        QVERIFY(!context.hasScreenshotText());

        context.selectedText = QStringLiteral(" selected ");
        context.voiceText = QStringLiteral(" voice ");
        context.textOnlyInput = QStringLiteral(" text ");
        context.screenshotRecognizedText = QStringLiteral(" ocr ");
        QVERIFY(context.hasSelectedText());
        QVERIFY(context.hasVoiceText());
        QVERIFY(context.hasTextOnlyInput());
        QVERIFY(!context.hasScreenshotText());

        context.screenshotInput = true;
        QVERIFY(context.hasScreenshotText());
    }

    void voiceRunFormatterBuildsHistoryInput()
    {
        VoiceRunContext context;
        context.selectedText = QString::fromUtf8("原文");
        context.voiceText = QString::fromUtf8("补充要求");

        const QString text = VoiceRunFormatter::historyInput(context);

        QVERIFY(text.contains(QString::fromUtf8("选中文字")));
        QVERIFY(text.contains(QString::fromUtf8("原文")));
        QVERIFY(text.contains(QString::fromUtf8("语音输入")));
        QVERIFY(text.contains(QString::fromUtf8("补充要求")));
    }

    void voiceRunFormatterBuildsScreenshotHistoryInput()
    {
        VoiceRunContext context;
        context.screenshotInput = true;
        context.screenshotRecognizedText = QString::fromUtf8("截图文字");
        context.voiceText = QString::fromUtf8("翻译一下");

        const QString text = VoiceRunFormatter::historyInput(context);

        QVERIFY(text.contains(QString::fromUtf8("截图识别文字")));
        QVERIFY(text.contains(QString::fromUtf8("截图文字")));
        QVERIFY(text.contains(QString::fromUtf8("语音补充要求")));
    }

    void voiceRunFormatterBuildsDetailPopupText()
    {
        VoiceRunContext context;
        context.textOnly = true;
        context.textOnlyInput = QString::fromUtf8("输入内容");

        ResultPopupFormatRequest request;
        request.context = context;
        request.output = QString::fromUtf8("输出内容");
        request.templateId = QStringLiteral("detail");
        request.functionTitle = QString::fromUtf8("翻译");
        request.modelTitle = QStringLiteral("deepseek-v4-flash");
        request.elapsedMs = 123;

        const QString text = VoiceRunFormatter::resultPopupText(request);

        QVERIFY(text.contains(QString::fromUtf8("功能：翻译")));
        QVERIFY(text.contains(QString::fromUtf8("模型：deepseek-v4-flash")));
        QVERIFY(text.contains(QString::fromUtf8("输入内容")));
        QVERIFY(text.contains(QString::fromUtf8("输出内容")));
        QVERIFY(text.contains(QStringLiteral("123 ms")));
    }

    void voiceRunPlannerBuildsVoiceTranslatePlan()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("translate");
        context.selectedText = QStringLiteral("selected text");
        context.voiceText = QStringLiteral("translate politely");

        const VoiceRunModelPlan plan =
            VoiceRunPlanner::plan(context, QStringLiteral("extra"));

        QCOMPARE(plan.operation, VoiceRunOperation::Translate);
        QCOMPARE(plan.primaryText, QStringLiteral("selected text"));
        QCOMPARE(plan.voiceInstruction, QStringLiteral("translate politely"));
        QCOMPARE(plan.extraInstruction, QStringLiteral("extra"));
        QCOMPARE(plan.hasVoiceInput, true);
    }

    void voiceRunPlannerBuildsTextOnlyAskPlan()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("ask");
        context.textOnly = true;
        context.textOnlyInput = QStringLiteral("selected text");
        context.voiceText = QStringLiteral("ignored");

        const VoiceRunModelPlan plan = VoiceRunPlanner::plan(context);

        QCOMPARE(plan.operation, VoiceRunOperation::Ask);
        QVERIFY(plan.question.isEmpty());
        QCOMPARE(plan.hasVoiceInput, false);
    }

    void voiceRunPlannerBuildsCustomVoicePlan()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("custom-1");
        context.voiceText = QStringLiteral("rewrite it");

        const VoiceRunModelPlan plan = VoiceRunPlanner::plan(context);

        QCOMPARE(plan.operation, VoiceRunOperation::Custom);
        QCOMPARE(plan.customVoiceText, QStringLiteral("rewrite it"));
        QCOMPARE(plan.hasVoiceInput, true);
    }

    void voiceRunPlannerAddsScreenshotTranslateInstruction()
    {
        VoiceRunContext context;
        context.modeId = QStringLiteral("translate");
        context.screenshotInput = true;
        context.screenshotRecognizedText = QStringLiteral("hello");
        OcrTextBlock block;
        block.text = QStringLiteral("hello");
        context.screenshotBlocks.append(block);

        const VoiceRunModelPlan plan =
            VoiceRunPlanner::plan(context, QStringLiteral("keep style"));

        QCOMPARE(plan.operation, VoiceRunOperation::Translate);
        QVERIFY(plan.extraInstruction.contains(QString::fromUtf8("逐行翻译")));
        QVERIFY(plan.extraInstruction.contains(QStringLiteral("keep style")));
    }

    void historyRecordBuilderBuildsScreenshotAndSegments()
    {
        HistoryRecordMetadataRequest request;
        request.input = QString::fromUtf8("输入");
        request.output = QString::fromUtf8("输出");
        request.model = QStringLiteral("deepseek-v4-flash");
        request.elapsedMs = 300;
        request.speechElapsedMs = 100;
        request.modelElapsedMs = 200;
        request.promptVersion = QStringLiteral("prompt-v1");
        request.actionHadRecording = true;
        request.recordingTriggerMode = QStringLiteral("hold");
        request.longRecording = true;
        request.runContext.modeId = QStringLiteral("translate");
        request.runContext.screenshotInput = true;
        request.runContext.screenshotOcrEngine = OcrEngine::WindowsOcr;
        request.runContext.screenshotOcrElapsedMs = 45;
        request.runContext.screenshotOcrUsedFallback = true;
        request.runContext.screenshotRect = QRect(10, 20, 300, 120);

        RecordingSegment first;
        first.index = 1;
        first.wavPath = QStringLiteral("one.wav");
        first.text = QString::fromUtf8("第一段");
        first.recognitionElapsedMs = 30;
        first.attempts = 1;
        RecordingSegment second;
        second.index = 2;
        second.wavPath = QStringLiteral("two.wav");
        second.error = QString::fromUtf8("失败");
        second.recognitionElapsedMs = 40;
        second.attempts = 2;
        request.recordingSegments = QVector<RecordingSegment>() << first << second;

        const QJsonObject item =
            HistoryRecordBuilder::buildMetadata(request);

        QCOMPARE(item.value(QStringLiteral("input")).toString(), QString::fromUtf8("输入"));
        QCOMPARE(item.value(QStringLiteral("output")).toString(), QString::fromUtf8("输出"));
        QCOMPARE(item.value(QStringLiteral("model")).toString(), QStringLiteral("deepseek-v4-flash"));
        QCOMPARE(item.value(QStringLiteral("inputSource")).toString(), QStringLiteral("screenshot"));
        QCOMPARE(item.value(QStringLiteral("ocrEngine")).toString(), QStringLiteral("Windows OCR"));
        QCOMPARE(static_cast<int>(item.value(QStringLiteral("ocrElapsedMs")).toDouble()), 45);
        QCOMPARE(item.value(QStringLiteral("ocrUsedFallback")).toBool(), true);
        QCOMPARE(item.value(QStringLiteral("recordingTriggerMode")).toString(), QStringLiteral("hold"));
        QCOMPARE(item.value(QStringLiteral("longRecording")).toBool(), true);
        QCOMPARE(item.value(QStringLiteral("segmentCount")).toInt(), 2);
        QCOMPARE(item.value(QStringLiteral("failedSegmentCount")).toInt(), 1);

        const QJsonObject rect =
            item.value(QStringLiteral("screenshotRect")).toObject();
        QCOMPARE(rect.value(QStringLiteral("x")).toInt(), 10);
        QCOMPARE(rect.value(QStringLiteral("height")).toInt(), 120);

        const QJsonArray failedSegments =
            item.value(QStringLiteral("failedSegments")).toArray();
        QCOMPARE(failedSegments.size(), 1);
        QCOMPARE(failedSegments.first().toInt(), 2);

        const QJsonArray segments =
            item.value(QStringLiteral("segments")).toArray();
        QCOMPARE(segments.size(), 2);
        QCOMPARE(
            segments.first().toObject().value(QStringLiteral("text")).toString(),
            QString::fromUtf8("第一段")
        );
    }

    void historyRecordBuilderBuildsOcrPageMetadata()
    {
        OcrPageHistoryMetadataRequest request;
        request.imagePath = QStringLiteral("C:/temp/sample.png");
        request.languages = QStringList()
            << QStringLiteral("zh-Hans")
            << QStringLiteral("en");
        request.result.ok = false;
        request.result.text = QStringLiteral("recognized text");
        request.result.errorCode = QStringLiteral("OCR001");
        request.result.errorMessage = QStringLiteral("bad image");
        request.result.engine = OcrEngine::CustomCloud;
        request.result.elapsedMs = 1234;
        request.result.usedFallback = true;

        const QJsonObject item =
            HistoryRecordBuilder::buildOcrPageMetadata(request);

        QCOMPARE(item.value(QStringLiteral("input")).toString(), QStringLiteral("recognized text"));
        QCOMPARE(item.value(QStringLiteral("output")).toString(), QString());
        QVERIFY(item.value(QStringLiteral("error")).toString().contains(QStringLiteral("OCR001")));
        QCOMPARE(item.value(QStringLiteral("model")).toString(), QString());
        QCOMPARE(static_cast<int>(item.value(QStringLiteral("elapsedMs")).toDouble()), 1234);
        QCOMPARE(static_cast<int>(item.value(QStringLiteral("speechElapsedMs")).toDouble()), -1);
        QCOMPARE(static_cast<int>(item.value(QStringLiteral("modelElapsedMs")).toDouble()), -1);
        QCOMPARE(item.value(QStringLiteral("ocrElapsedMs")).toInt(), 1234);
        QCOMPARE(item.value(QStringLiteral("ocrUsedFallback")).toBool(), true);
        QCOMPARE(item.value(QStringLiteral("imageFileName")).toString(), QStringLiteral("sample.png"));

        const QJsonArray languages =
            item.value(QStringLiteral("ocrLanguages")).toArray();
        QCOMPARE(languages.size(), 2);
        QCOMPARE(languages.first().toString(), QStringLiteral("zh-Hans"));
    }

    void resultOutputRouterChoosesAutoWrite()
    {
        ResultOutputRouteRequest request;
        request.outputMode = QStringLiteral("autoWrite");
        request.screenshotInput = true;
        request.hasSelectedText = true;

        QCOMPARE(
            ResultOutputRouter::route(request),
            ResultOutputDestination::AutoWrite
        );

        const ResultOutputPlan plan =
            ResultOutputRouter::plan(request);
        QCOMPARE(plan.destination, ResultOutputDestination::AutoWrite);
        QCOMPARE(plan.replaceSelectedText, true);
        QVERIFY(plan.progressTitle.contains(QString::fromUtf8("写入")));
        QVERIFY(plan.doneMessage.contains(QString::fromUtf8("粘贴")));
    }

    void resultOutputRouterChoosesScreenshotPanelOnlyForScreenshots()
    {
        ResultOutputRouteRequest request;
        request.outputMode = QStringLiteral("screenshotPanel");
        request.screenshotInput = true;

        QCOMPARE(
            ResultOutputRouter::route(request),
            ResultOutputDestination::ScreenshotPanel
        );

        request.screenshotInput = false;
        QCOMPARE(
            ResultOutputRouter::route(request),
            ResultOutputDestination::ResultPopup
        );
    }

    void resultOutputRouterFallsBackToPopup()
    {
        ResultOutputRouteRequest request;
        request.outputMode = QStringLiteral("unexpected");

        QCOMPARE(
            ResultOutputRouter::route(request),
            ResultOutputDestination::ResultPopup
        );
    }
};

QTEST_MAIN(DomainTypesTests)
#include "domain_types_tests.moc"
