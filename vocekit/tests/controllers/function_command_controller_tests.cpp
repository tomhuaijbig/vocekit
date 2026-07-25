#include <QtTest>

#include "../../src/capture/screenshot_types.h"
#include "../../src/controllers/function_command_controller.h"

#include <QFile>

class FunctionCommandControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void opensHubBeforeBusyGuards();
    void blocksCommandsWhileScreenshotIsActive();
    void routesVocabularyWithCapturedTarget();
    void collectsSelectionAndProcessesText();
    void startsVoiceAfterSelection();
    void routesPrimaryAndDedicatedScreenshotCommands();
    void preservesTargetForScreenshotContinuation();
    void voiceControllerOnlyDelegatesCommandRouting();
};

namespace {

FunctionSettings functionSettings(
    const QString &id,
    bool useSelection,
    bool useVoice,
    bool useScreenshot = false,
    const QString &screenshotTrigger = QString()
)
{
    FunctionSettings function;
    function.id = id;
    function.input.useSelection = useSelection;
    function.input.useVoice = useVoice;
    function.input.useScreenshot = useScreenshot;
    function.input.screenshotTriggerMode = screenshotTrigger;
    function.output.floatingBarSeconds = 4;
    return function;
}

} // namespace

void FunctionCommandControllerTests::opensHubBeforeBusyGuards()
{
    int hubCount = 0;
    int statusCount = 0;
    FunctionCommandAccess access;
    access.showHub = [&hubCount]() {
        ++hubCount;
    };
    access.screenshotActive = []() {
        return true;
    };
    access.processing = []() {
        return true;
    };
    access.setStatus = [&statusCount](const QString &, const QString &) {
        ++statusCount;
    };

    FunctionCommandController controller(access);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("hub")),
        FunctionCommandOutcome::HubOpened
    );
    QCOMPARE(hubCount, 1);
    QCOMPARE(statusCount, 0);
}

void FunctionCommandControllerTests::
blocksCommandsWhileScreenshotIsActive()
{
    QString statusTitle;
    int actionCount = 0;
    FunctionCommandAccess access;
    access.screenshotActive = []() {
        return true;
    };
    access.setStatus = [&statusTitle](
        const QString &title,
        const QString &
    ) {
        statusTitle = title;
    };
    access.beginAction = [&actionCount]() {
        ++actionCount;
    };

    FunctionCommandController controller(access);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("translate")),
        FunctionCommandOutcome::ScreenshotBusy
    );
    QVERIFY(!statusTitle.isEmpty());
    QCOMPARE(actionCount, 0);
}

void FunctionCommandControllerTests::
routesVocabularyWithCapturedTarget()
{
    int restartCount = 0;
    FunctionCommandWindowHandle capturedTarget =
        reinterpret_cast<FunctionCommandWindowHandle>(quintptr(0x1234));
    FunctionCommandWindowHandle receivedTarget = nullptr;
    bool receivedBusy = false;

    FunctionCommandAccess access;
    access.restartTimer = [&restartCount]() {
        ++restartCount;
    };
    access.captureTargetWindow = [capturedTarget]() {
        return capturedTarget;
    };
    access.recordingBusy = []() {
        return true;
    };
    access.addVocabulary = [&](
        FunctionCommandWindowHandle target,
        bool recordingBusy
    ) {
        receivedTarget = target;
        receivedBusy = recordingBusy;
    };

    FunctionCommandController controller(access);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("vocabulary_add")),
        FunctionCommandOutcome::VocabularyHandled
    );
    QCOMPARE(restartCount, 1);
    QCOMPARE(receivedTarget, capturedTarget);
    QVERIFY(receivedBusy);
    QCOMPARE(controller.targetWindow(), capturedTarget);
}

void FunctionCommandControllerTests::
collectsSelectionAndProcessesText()
{
    const FunctionCommandWindowHandle capturedTarget =
        reinterpret_cast<FunctionCommandWindowHandle>(quintptr(0x42));
    SelectedTextWorkflowRequest selectedRequest;
    QString processedMode;
    QString processedText;
    bool floatingEnabled = false;
    int floatingMsec = -1;

    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = [capturedTarget]() {
        return capturedTarget;
    };
    access.prepareFloatingBar = [&](
        bool enabled,
        int autoHideMsec
    ) {
        floatingEnabled = enabled;
        floatingMsec = autoHideMsec;
    };
    access.readSelectedText = [&](
        const SelectedTextWorkflowRequest &request
    ) {
        selectedRequest = request;
        SelectedTextWorkflowResult result;
        result.text = QStringLiteral("selected");
        return result;
    };
    access.processText = [&](
        const QString &modeId,
        const QString &text
    ) {
        processedMode = modeId;
        processedText = text;
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.floatingBarEnabled = true;
    settings.strongSelectionEnabled = true;
    settings.functions.append(
        functionSettings(QStringLiteral("translate"), true, false)
    );
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("translate")),
        FunctionCommandOutcome::TextSubmitted
    );
    QCOMPARE(selectedRequest.modeId, QStringLiteral("translate"));
    QVERIFY(selectedRequest.strongSelectionEnabled);
    QVERIFY(!selectedRequest.useVoice);
    QCOMPARE(selectedRequest.targetWindow, capturedTarget);
    QCOMPARE(controller.selectedText(), QStringLiteral("selected"));
    QCOMPARE(processedMode, QStringLiteral("translate"));
    QCOMPARE(processedText, QStringLiteral("selected"));
    QVERIFY(floatingEnabled);
    QCOMPARE(floatingMsec, 4000);
}

void FunctionCommandControllerTests::startsVoiceAfterSelection()
{
    QString recordingMode;
    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.prepareFloatingBar = [](bool, int) {};
    access.readSelectedText = [](
        const SelectedTextWorkflowRequest &
    ) {
        SelectedTextWorkflowResult result;
        result.text = QStringLiteral("context");
        return result;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };
    access.beginRecording = [&recordingMode](const QString &modeId) {
        recordingMode = modeId;
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    settings.functions.append(
        functionSettings(QStringLiteral("ask"), true, true)
    );
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingStarted
    );
    QCOMPARE(controller.selectedText(), QStringLiteral("context"));
    QCOMPARE(recordingMode, QStringLiteral("ask"));
}

void FunctionCommandControllerTests::
routesPrimaryAndDedicatedScreenshotCommands()
{
    struct ScreenshotCall
    {
        QString functionId;
        bool targetRemembered = false;
        bool externalBusy = false;
    };
    QVector<ScreenshotCall> calls;

    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.prepareFloatingBar = [](bool, int) {};
    access.beginScreenshot = [&calls](
        const QString &functionId,
        bool targetRemembered,
        bool externalBusy
    ) {
        ScreenshotCall call;
        call.functionId = functionId;
        call.targetRemembered = targetRemembered;
        call.externalBusy = externalBusy;
        calls.append(call);
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.functions.append(functionSettings(
        QStringLiteral("translate"),
        false,
        false,
        true,
        screenshotTriggerPrimary()
    ));
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("translate")),
        FunctionCommandOutcome::ScreenshotStarted
    );
    QCOMPARE(calls.size(), 1);
    QCOMPARE(calls.at(0).functionId, QStringLiteral("translate"));
    QVERIFY(calls.at(0).targetRemembered);

    QCOMPARE(
        controller.handleScreenshotTrigger(QStringLiteral("translate")),
        FunctionCommandOutcome::ScreenshotStarted
    );
    QCOMPARE(calls.size(), 2);
    QCOMPARE(calls.at(1).functionId, QStringLiteral("translate"));
    QVERIFY(!calls.at(1).targetRemembered);
}

void FunctionCommandControllerTests::
preservesTargetForScreenshotContinuation()
{
    int captureCount = 0;
    QString processedText;
    const FunctionCommandWindowHandle target =
        reinterpret_cast<FunctionCommandWindowHandle>(quintptr(0x77));

    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = [&captureCount, target]() {
        ++captureCount;
        return target;
    };
    access.processText = [&processedText](
        const QString &,
        const QString &text
    ) {
        processedText = text;
    };

    FunctionCommandController controller(access);
    controller.prepareScreenshotRun(false);
    QCOMPARE(captureCount, 1);
    QCOMPARE(controller.targetWindow(), target);

    controller.prepareScreenshotRun(true);
    QCOMPARE(captureCount, 1);
    QCOMPARE(controller.targetWindow(), target);

    controller.processScreenshotText(
        QStringLiteral("translate"),
        QStringLiteral("ocr text")
    );
    QCOMPARE(controller.selectedText(), QStringLiteral("ocr text"));
    QCOMPARE(processedText, QStringLiteral("ocr text"));
}

void FunctionCommandControllerTests::
voiceControllerOnlyDelegatesCommandRouting()
{
    const QString path = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    QVERIFY2(!path.isEmpty(), "找不到 VoiceController 源文件");
    QFile source(path);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("FunctionCommandController"));
    QVERIFY(!contents.contains("void rememberTargetWindow("));
    QVERIFY(!contents.contains("QString m_selectedText;"));
    QVERIFY(!contents.contains("ClipboardWindowHandle m_targetWindow"));
    QVERIFY(!contents.contains(
        "if (id == QStringLiteral(\"vocabulary_add\"))"
    ));
    QVERIFY(!contents.contains(
        "parseScreenshotHotkeyLogicalId(id"
    ));
}

QTEST_APPLESS_MAIN(FunctionCommandControllerTests)

#include "function_command_controller_tests.moc"
