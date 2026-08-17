#include <QtTest>

#include "../../src/controllers/voice_run_lifecycle_controller.h"
#include "../../src/domain/voice_run_session.h"

#include <QFile>

class VoiceRunLifecycleControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void runsModelAndRecordsTiming();
    void cancelsActiveModelAndStartsNextRunFresh();
    void finalizesOutputThroughConfiguredCorrection();
    void savesCompleteHistorySnapshotAndNotifies();
    void dictateWithoutPolishOmitsModelMetadata();
    void voiceControllerOnlyDelegatesRunLifecycle();
};

void VoiceRunLifecycleControllerTests::runsModelAndRecordsTiming()
{
    VoiceRunSession session;
    session.beginAction();

    VoiceModelProcessingRequest captured;
    VoiceRunLifecycleAccess access;
    access.promptSnapshot = []() {
        PromptRuntimeSnapshot snapshot;
        return snapshot;
    };
    access.runtimeSettings = [](
        const AppSettingsData &,
        const PromptRuntimeSnapshot &,
        const QString &modeId
    ) {
        VoiceModelRuntimeSettings runtime;
        runtime.defaultModel = QStringLiteral("test-model");
        runtime.systemPrompt = modeId + QStringLiteral("-prompt");
        return runtime;
    };
    access.vocabularyPromptBlock = [](
        const AppSettingsData &,
        const QString &,
        const QString &,
        bool
    ) {
        return QStringLiteral("terms");
    };
    access.processModelRequest =
        [&captured](const VoiceModelProcessingRequest &request) {
            captured = request;
            VoiceModelProcessingResult result;
            result.text = QStringLiteral("translated");
            result.durationMs = 321;
            result.promptVersion = QStringLiteral("prompt-v2");
            return result;
        };

    VoiceRunLifecycleController controller(access, &session);
    AppSettingsData settings;
    controller.updateConfiguration(settings);

    VoiceRunContext context;
    context.modeId = QStringLiteral("translate");
    context.selectedText = QStringLiteral("hello");

    QString error;
    const QString output = controller.runModel(
        context,
        QStringLiteral("override-model"),
        QStringLiteral("keep-format"),
        &error
    );

    QCOMPARE(output, QStringLiteral("translated"));
    QVERIFY(error.isEmpty());
    QCOMPARE(captured.modeId, QStringLiteral("translate"));
    QCOMPARE(captured.selectedText, QStringLiteral("hello"));
    QCOMPARE(captured.modelOverride, QStringLiteral("override-model"));
    QCOMPARE(captured.extraInstruction, QStringLiteral("keep-format"));
    QCOMPARE(
        captured.runtime.systemPrompt,
        QStringLiteral("translate-prompt")
    );
    QCOMPARE(session.snapshot().modelElapsedMs, qint64(321));
    QCOMPARE(
        session.snapshot().promptVersion,
        QStringLiteral("prompt-v2")
    );
}

void VoiceRunLifecycleControllerTests::
cancelsActiveModelAndStartsNextRunFresh()
{
    VoiceRunSession session;
    session.beginAction();

    VoiceRunLifecycleController *controllerPointer = nullptr;
    int callCount = 0;
    ExecutionId firstExecutionId;
    bool firstSawCancellation = false;
    bool secondStartedFresh = false;

    VoiceRunLifecycleAccess access;
    access.runtimeSettings = [](
        const AppSettingsData &,
        const PromptRuntimeSnapshot &,
        const QString &
    ) {
        return VoiceModelRuntimeSettings();
    };
    access.processModelRequest = [&](
        const VoiceModelProcessingRequest &request
    ) {
        ++callCount;
        VoiceModelProcessingResult result;
        if (callCount == 1) {
            firstExecutionId = request.cancellation.executionId();
            controllerPointer->cancelActiveModel();
            firstSawCancellation =
                request.cancellation.isCancellationRequested();
            result.cancelled = firstSawCancellation;
            result.errorMessage = QStringLiteral("请求已取消。");
            return result;
        }

        secondStartedFresh =
            request.cancellation.isValid()
            && !request.cancellation.isCancellationRequested()
            && request.cancellation.executionId() != firstExecutionId;
        result.text = QStringLiteral("second-result");
        return result;
    };

    VoiceRunLifecycleController controller(access, &session);
    controllerPointer = &controller;
    controller.updateConfiguration(AppSettingsData());

    VoiceRunContext context;
    context.modeId = QStringLiteral("ask");
    context.voiceText = QStringLiteral("question");

    QString error;
    QVERIFY(controller.runModel(
        context,
        QString(),
        QString(),
        &error
    ).isEmpty());
    QVERIFY(firstSawCancellation);
    QVERIFY(controller.lastModelRunCancelled());

    error.clear();
    QCOMPARE(
        controller.runModel(
            context,
            QString(),
            QString(),
            &error
        ),
        QStringLiteral("second-result")
    );
    QVERIFY(error.isEmpty());
    QVERIFY(secondStartedFresh);
    QVERIFY(!controller.lastModelRunCancelled());
}

void VoiceRunLifecycleControllerTests::
finalizesOutputThroughConfiguredCorrection()
{
    bool hasVoiceInput = false;
    QString capturedMode;

    VoiceRunLifecycleAccess access;
    access.postCorrectOutput = [&](
        const AppSettingsData &,
        const QString &output,
        const QString &modeId,
        bool hasVoice
    ) {
        capturedMode = modeId;
        hasVoiceInput = hasVoice;
        return output == QStringLiteral("deepseep")
            ? QStringLiteral("DeepSeek")
            : output;
    };

    VoiceRunSession session;
    VoiceRunLifecycleController controller(access, &session);

    VoiceRunContext context;
    context.modeId = QStringLiteral("dictate");
    context.voiceText = QStringLiteral("voice");
    context.textOnly = false;

    QCOMPARE(
        controller.finalizeOutput(context, QStringLiteral("deepseep")),
        QStringLiteral("DeepSeek")
    );
    QCOMPARE(capturedMode, QStringLiteral("dictate"));
    QVERIFY(hasVoiceInput);
}

void VoiceRunLifecycleControllerTests::
savesCompleteHistorySnapshotAndNotifies()
{
    VoiceRunSession session;
    session.beginAction();
    session.setActionHadRecording(true);
    session.setRecordingAudioPath(QStringLiteral("captured.wav"));
    session.setRecordingTriggerMode(QStringLiteral("toggle"));
    session.setLongRecording(true);
    session.setSpeechElapsedMs(100);
    session.setModelResult(200, QStringLiteral("prompt-v3"));

    VoiceRunContext runContext;
    runContext.modeId = QStringLiteral("translate");
    runContext.selectedText = QStringLiteral("source");
    session.setRunContext(runContext);

    VoiceHistorySaveRequest captured;
    QString notifiedPath;
    QString loggedAction;
    qint64 loggedElapsedMs = -1;
    VoiceRunLifecycleAccess access;
    access.resolveHistoryRoot = [](const QString &path) {
        return QStringLiteral("resolved/") + path;
    };
    access.modeTitle = [](
        const AppSettingsData &,
        const QString &modeId
    ) {
        return modeId == QStringLiteral("translate")
            ? QString::fromUtf8("翻译")
            : QString::fromUtf8("功能");
    };
    access.fallbackAudioPath = []() {
        return QStringLiteral("fallback.wav");
    };
    access.elapsedMs = []() {
        return qint64(777);
    };
    access.persistHistory =
        [&captured](const VoiceHistorySaveRequest &request) {
            captured = request;
            VoiceHistorySaveResult result;
            result.saved.ok = true;
            result.saved.modeDetailPath =
                QStringLiteral("detail.json");
            result.logAction = QString::fromUtf8("保存");
            result.logDetail = QStringLiteral("saved");
            return result;
        };
    access.historySaved = [&](const QString &path) {
        notifiedPath = path;
    };
    access.historyLogged = [&](
        const QString &action,
        const QString &,
        qint64 elapsedMs
    ) {
        loggedAction = action;
        loggedElapsedMs = elapsedMs;
    };

    VoiceRunLifecycleController controller(access, &session);
    AppSettingsData settings;
    settings.recordDirectory = QStringLiteral("records");
    FunctionSettings function;
    function.id = QStringLiteral("translate");
    function.modelId = QStringLiteral("default-model");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    VoiceRunLifecycleHistoryRequest request;
    request.modeId = QStringLiteral("translate");
    request.input = QStringLiteral("input");
    request.output = QStringLiteral("output");
    request.modelOverride = QStringLiteral("override-model");
    controller.saveHistory(request);

    QCOMPARE(captured.recordDirectory, QStringLiteral("resolved/records"));
    QCOMPARE(captured.modeTitle, QString::fromUtf8("翻译"));
    QCOMPARE(captured.sourceAudioPath, QStringLiteral("captured.wav"));
    QCOMPARE(captured.input, QStringLiteral("input"));
    QCOMPARE(captured.output, QStringLiteral("output"));
    QCOMPARE(captured.model, QStringLiteral("override-model"));
    QVERIFY(captured.usedModel);
    QCOMPARE(captured.speechElapsedMs, qint64(100));
    QCOMPARE(captured.modelElapsedMs, qint64(200));
    QCOMPARE(captured.promptVersion, QStringLiteral("prompt-v3"));
    QVERIFY(captured.actionHadRecording);
    QVERIFY(captured.longRecording);
    QCOMPARE(captured.recordingTriggerMode, QStringLiteral("toggle"));
    QCOMPARE(notifiedPath, QStringLiteral("detail.json"));
    QCOMPARE(loggedAction, QString::fromUtf8("保存"));
    QCOMPARE(loggedElapsedMs, qint64(777));
}

void VoiceRunLifecycleControllerTests::
dictateWithoutPolishOmitsModelMetadata()
{
    VoiceRunSession session;
    session.beginAction();
    session.setModelResult(90, QStringLiteral("unused-prompt"));

    VoiceHistorySaveRequest captured;
    VoiceRunLifecycleAccess access;
    access.resolveHistoryRoot = [](const QString &path) {
        return path;
    };
    access.modeTitle = [](
        const AppSettingsData &,
        const QString &
    ) {
        return QString::fromUtf8("听写");
    };
    access.persistHistory =
        [&captured](const VoiceHistorySaveRequest &request) {
            captured = request;
            VoiceHistorySaveResult result;
            result.saved.ok = true;
            return result;
        };

    VoiceRunLifecycleController controller(access, &session);
    AppSettingsData settings;
    settings.dictatePolishEnabled = false;
    FunctionSettings function;
    function.id = QStringLiteral("dictate");
    function.modelId = QStringLiteral("configured-model");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    VoiceRunLifecycleHistoryRequest request;
    request.modeId = QStringLiteral("dictate");
    controller.saveHistory(request);

    QVERIFY(!captured.usedModel);
    QCOMPARE(captured.model, QStringLiteral("configured-model"));
}

void VoiceRunLifecycleControllerTests::
voiceControllerOnlyDelegatesRunLifecycle()
{
    const QString path = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    QVERIFY2(!path.isEmpty(), "找不到 VoiceController 源文件");
    QFile source(path);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("VoiceRunLifecycleController"));
    QVERIFY(!contents.contains("QString runContext("));
    QVERIFY(!contents.contains("QString finalOutputForContext("));
    QVERIFY(!contents.contains("void saveHistory("));
    QVERIFY(!contents.contains("VoiceHistorySaveRequest request;"));
}

QTEST_APPLESS_MAIN(VoiceRunLifecycleControllerTests)

#include "voice_run_lifecycle_controller_tests.moc"
