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
    void defersSelectionWhenVoiceComesFirst();
    void runsScreenshotBeforeVoiceWhenConfigured();
    void runsVoiceBeforeScreenshotWhenConfigured();
    void routesPrimaryAndDedicatedScreenshotCommands();
    void preservesTargetForScreenshotContinuation();
    void flowStartRunsBeforeClassicBusyGuard();
    void canvasNotAvailableNeverFallsBackToClassic();
    void launcherKeepsItsOwnTriggerAndRememberedTarget();
    void classicHoldReleaseKeepsOwnerAcrossCanvasSwitch();
    void canvasHoldReleaseKeepsOwnerAcrossClassicSwitch();
    void deletedFunctionStillReleasesFrozenHoldOwner();
    void unknownHoldReleaseFailsClosed();
    void classicToggleStopsBeforeCanvasRoutingAfterModeSwitch();
    void canvasToggleStopsBeforeClassicRoutingAfterModeSwitch();
    void deletedClassicToggleStillStopsActiveRecording();
    void differentKnownFunctionKeepsActiveRecordingGuard();
    void holdOwnerUsesModeAtRegisteredHotkeyDispatch();
    void voiceControllerOnlyDelegatesCommandRouting();
    void resolvesFloatingStyleOnlyForRunsThatActuallyStart();
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
    function.executionMode = FunctionExecutionMode::Classic;
    function.input.useSelection = useSelection;
    function.input.useVoice = useVoice;
    function.input.useScreenshot = useScreenshot;
    function.input.screenshotTriggerMode = screenshotTrigger;
    function.input.order = QStringList()
        << QStringLiteral("selection")
        << QStringLiteral("voice")
        << QStringLiteral("screenshot");
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
    AppSettingsData settings;
    settings.functions.append(normalizeFunctionSettings(
        functionSettings(
            QStringLiteral("translate"),
            true,
            false
        )
    ));
    controller.updateConfiguration(settings);

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
    int flowCalls = 0;
    bool floatingEnabled = false;
    int floatingMsec = -1;

    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = [capturedTarget]() {
        return capturedTarget;
    };
    access.prepareFloatingBar = [&](
        bool enabled,
        int autoHideMsec,
        const QString &
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
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.floatingBarEnabled = true;
    settings.strongSelectionEnabled = true;
    settings.functions.append(
        normalizeFunctionSettings(functionSettings(
            QStringLiteral("translate"),
            true,
            false
        ))
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
    QCOMPARE(flowCalls, 0);
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
    access.prepareFloatingBar = [](bool, int, const QString &) {};
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

void FunctionCommandControllerTests::defersSelectionWhenVoiceComesFirst()
{
    QStringList actions;
    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.prepareFloatingBar = [](bool, int, const QString &) {};
    access.readSelectedText = [&actions](
        const SelectedTextWorkflowRequest &
    ) {
        actions.append(QStringLiteral("selection"));
        SelectedTextWorkflowResult result;
        result.text = QStringLiteral("context");
        return result;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };
    access.beginRecording = [&actions](const QString &) {
        actions.append(QStringLiteral("voice"));
    };
    access.processVoice = [&actions](
        const QString &,
        const QString &text
    ) {
        actions.append(QStringLiteral("model:") + text);
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings ask =
        functionSettings(QStringLiteral("ask"), true, true);
    ask.input.order = QStringList()
        << QStringLiteral("voice")
        << QStringLiteral("selection")
        << QStringLiteral("screenshot");
    settings.functions.append(ask);
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingStarted
    );
    QCOMPARE(actions, QStringList() << QStringLiteral("voice"));
    controller.processRecognizedVoice(
        QStringLiteral("ask"),
        QStringLiteral("question")
    );
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("voice")
            << QStringLiteral("selection")
            << QStringLiteral("model:question")
    );
    QCOMPARE(controller.selectedText(), QStringLiteral("context"));
}

void FunctionCommandControllerTests::
runsScreenshotBeforeVoiceWhenConfigured()
{
    QStringList actions;
    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.prepareFloatingBar = [](bool, int, const QString &) {};
    access.beginScreenshot = [&actions](
        const QString &,
        bool,
        bool
    ) {
        actions.append(QStringLiteral("screenshot"));
        return true;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };
    access.beginRecording = [&actions](const QString &) {
        actions.append(QStringLiteral("voice"));
    };
    access.processVoice = [&actions](
        const QString &,
        const QString &text
    ) {
        actions.append(QStringLiteral("model:") + text);
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function = functionSettings(
        QStringLiteral("ask"),
        false,
        true,
        true,
        screenshotTriggerPrimary()
    );
    function.input.order = QStringList()
        << QStringLiteral("screenshot")
        << QStringLiteral("voice")
        << QStringLiteral("selection");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::ScreenshotStarted
    );
    QCOMPARE(actions, QStringList() << QStringLiteral("screenshot"));

    controller.processScreenshotText(
        QStringLiteral("ask"),
        QStringLiteral("ocr")
    );
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("screenshot")
            << QStringLiteral("voice")
    );

    controller.processRecognizedVoice(
        QStringLiteral("ask"),
        QStringLiteral("question")
    );
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("screenshot")
            << QStringLiteral("voice")
            << QStringLiteral("model:question")
    );
}

void FunctionCommandControllerTests::
runsVoiceBeforeScreenshotWhenConfigured()
{
    QStringList actions;
    bool screenshotSawExternalBusy = true;
    bool processing = false;
    FunctionCommandAccess access;
    access.beginAction = []() {};
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.processing = [&processing]() {
        return processing;
    };
    access.prepareFloatingBar = [](bool, int, const QString &) {};
    access.beginScreenshot = [&](
        const QString &,
        bool,
        bool externalBusy
    ) {
        actions.append(QStringLiteral("screenshot"));
        screenshotSawExternalBusy = externalBusy;
        return true;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };
    access.beginRecording = [&actions](const QString &) {
        actions.append(QStringLiteral("voice"));
    };
    access.processVoice = [&actions](
        const QString &,
        const QString &text
    ) {
        actions.append(QStringLiteral("model:") + text);
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function = functionSettings(
        QStringLiteral("ask"),
        false,
        true,
        true,
        screenshotTriggerPrimary()
    );
    function.input.order = QStringList()
        << QStringLiteral("voice")
        << QStringLiteral("screenshot")
        << QStringLiteral("selection");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingStarted
    );
    QCOMPARE(actions, QStringList() << QStringLiteral("voice"));

    processing = true;
    controller.processRecognizedVoice(
        QStringLiteral("ask"),
        QStringLiteral("question")
    );
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("voice")
            << QStringLiteral("screenshot")
    );
    QVERIFY(!screenshotSawExternalBusy);

    controller.processScreenshotText(
        QStringLiteral("ask"),
        QStringLiteral("ocr")
    );
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("voice")
            << QStringLiteral("screenshot")
            << QStringLiteral("model:question")
    );
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
    access.prepareFloatingBar = [](bool, int, const QString &) {};
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
        return true;
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
    QVERIFY(calls.at(1).targetRemembered);
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
flowStartRunsBeforeClassicBusyGuard()
{
    const FunctionCommandWindowHandle target =
        reinterpret_cast<FunctionCommandWindowHandle>(
            quintptr(0x9292)
        );
    int flowCalls = 0;
    int classicCalls = 0;
    FunctionFlowTriggerRequest captured;
    FunctionCommandAccess access;
    access.captureTargetWindow = [target]() {
        return target;
    };
    access.processing = []() {
        return true;
    };
    access.startPublishedFlow =
        [&flowCalls, &captured](
            const FunctionFlowTriggerRequest &request) {
            ++flowCalls;
            captured = request;
            return FunctionFlowStartOutcome::Started;
        };
    access.processText = [&classicCalls](
        const QString &,
        const QString &) {
        ++classicCalls;
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function =
        functionSettings(QStringLiteral("translate"), true, false);
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(normalizeFunctionSettings(function));
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("translate")),
        FunctionCommandOutcome::FlowStarted
    );
    QCOMPARE(flowCalls, 1);
    QCOMPARE(classicCalls, 0);
    QCOMPARE(captured.functionId, QStringLiteral("translate"));
    QCOMPARE(captured.trigger, FunctionFlowTrigger::MainHotkey);
    QCOMPARE(captured.targetWindow, target);
    QVERIFY(captured.classicWorkflowBusy);
}

void FunctionCommandControllerTests::
canvasNotAvailableNeverFallsBackToClassic()
{
    int flowCalls = 0;
    QString error;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return reinterpret_cast<FunctionCommandWindowHandle>(
            quintptr(0x8181)
        );
    };
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::NotAvailable;
        };
    access.showError = [&error](const QString &message) {
        error = message;
    };
    int classicScreenshots = 0;
    access.beginScreenshot =
        [&classicScreenshots](
            const QString &,
            bool remembered,
            bool) {
            ++classicScreenshots;
            return remembered;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function = functionSettings(
        QStringLiteral("translate"),
        false,
        false,
        true,
        screenshotTriggerPrimary()
    );
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(normalizeFunctionSettings(function));
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleScreenshotTrigger(
            QStringLiteral("translate")
        ),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(flowCalls, 1);
    QCOMPARE(classicScreenshots, 0);
    QCOMPARE(error, QStringLiteral("当前画布未配置此入口。"));

    access.startPublishedFlow =
        [](const FunctionFlowTriggerRequest &) {
            return FunctionFlowStartOutcome::ConfigurationError;
        };
    FunctionCommandController blocked(access);
    blocked.updateConfiguration(settings);
    QCOMPARE(
        blocked.handleScreenshotTrigger(
            QStringLiteral("translate")
        ),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(classicScreenshots, 0);
}

void FunctionCommandControllerTests::
launcherKeepsItsOwnTriggerAndRememberedTarget()
{
    const FunctionCommandWindowHandle remembered =
        reinterpret_cast<FunctionCommandWindowHandle>(
            quintptr(0x7373)
        );
    int captureCalls = 0;
    FunctionFlowTriggerRequest captured;
    FunctionCommandAccess access;
    access.captureTargetWindow = [&captureCalls]() {
        ++captureCalls;
        return reinterpret_cast<FunctionCommandWindowHandle>(
            quintptr(0x9999)
        );
    };
    access.startPublishedFlow =
        [&captured](const FunctionFlowTriggerRequest &request) {
            captured = request;
            return FunctionFlowStartOutcome::Started;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function =
        functionSettings(QStringLiteral("ask"), false, false);
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(normalizeFunctionSettings(function));
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleScreenshotLauncherTrigger(
            QStringLiteral("ask"),
            remembered
        ),
        FunctionCommandOutcome::FlowStarted
    );
    QCOMPARE(captureCalls, 0);
    QCOMPARE(
        captured.trigger,
        FunctionFlowTrigger::ScreenshotLauncher
    );
    QCOMPARE(captured.targetWindow, remembered);
}

void FunctionCommandControllerTests::
classicHoldReleaseKeepsOwnerAcrossCanvasSwitch()
{
    int classicReleases = 0;
    int flowReleases = 0;
    FunctionCommandAccess access;
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function =
        functionSettings(QStringLiteral("ask"), false, true);
    settings.functions.append(function);
    controller.updateConfiguration(settings);
    controller.handleHotkeyPressed(QStringLiteral("ask"));

    settings.functions[0].executionMode =
        FunctionExecutionMode::Canvas;
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkeyReleased(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(classicReleases, 1);
    QCOMPARE(flowReleases, 0);
}

void FunctionCommandControllerTests::
canvasHoldReleaseKeepsOwnerAcrossClassicSwitch()
{
    int classicReleases = 0;
    int flowReleases = 0;
    FunctionCommandAccess access;
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function =
        functionSettings(QStringLiteral("ask"), false, true);
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(function);
    controller.updateConfiguration(settings);
    controller.handleHotkeyPressed(QStringLiteral("ask"));

    settings.functions[0].executionMode =
        FunctionExecutionMode::Classic;
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkeyReleased(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(classicReleases, 0);
    QCOMPARE(flowReleases, 1);
}

void FunctionCommandControllerTests::
deletedFunctionStillReleasesFrozenHoldOwner()
{
    int classicReleases = 0;
    FunctionCommandAccess access;
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.functions.append(functionSettings(
        QStringLiteral("ask"),
        false,
        true
    ));
    controller.updateConfiguration(settings);
    controller.handleHotkeyPressed(QStringLiteral("ask"));

    settings.functions.clear();
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkeyReleased(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(classicReleases, 1);
}

void FunctionCommandControllerTests::
unknownHoldReleaseFailsClosed()
{
    int classicReleases = 0;
    int flowReleases = 0;
    FunctionCommandAccess access;
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };

    FunctionCommandController controller(access);
    QCOMPARE(
        controller.handleHotkeyReleased(QStringLiteral("missing")),
        FunctionCommandOutcome::NoAction
    );
    QCOMPARE(classicReleases, 0);
    QCOMPARE(flowReleases, 0);
}

void FunctionCommandControllerTests::
classicToggleStopsBeforeCanvasRoutingAfterModeSwitch()
{
    bool recordingActive = false;
    int stopCount = 0;
    int flowStartCalls = 0;
    FunctionCommandAccess access;
    access.recordingOwnsPress =
        [&recordingActive](const QString &id) {
            return recordingActive
                && id == QStringLiteral("ask");
        };
    access.recordingConsumesPress =
        [&recordingActive, &stopCount](const QString &) {
            if (!recordingActive) {
                return false;
            }
            recordingActive = false;
            ++stopCount;
            return true;
        };
    access.beginRecording = [&recordingActive](const QString &) {
        recordingActive = true;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };
    access.startPublishedFlow =
        [&flowStartCalls](const FunctionFlowTriggerRequest &) {
            ++flowStartCalls;
            return FunctionFlowStartOutcome::Started;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    settings.functions.append(functionSettings(
        QStringLiteral("ask"),
        false,
        true
    ));
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingStarted
    );
    QVERIFY(recordingActive);

    settings.functions[0].executionMode =
        FunctionExecutionMode::Canvas;
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(stopCount, 1);
    QCOMPARE(flowStartCalls, 0);
}

void FunctionCommandControllerTests::
canvasToggleStopsBeforeClassicRoutingAfterModeSwitch()
{
    bool flowVoiceActive = false;
    int stopCount = 0;
    int flowStartCalls = 0;
    int classicStarts = 0;
    FunctionCommandAccess access;
    access.recordingOwnsPress =
        [&flowVoiceActive](const QString &id) {
            return flowVoiceActive
                && id == QStringLiteral("ask");
        };
    access.recordingConsumesPress =
        [&flowVoiceActive, &stopCount](const QString &) {
            if (!flowVoiceActive) {
                return false;
            }
            flowVoiceActive = false;
            ++stopCount;
            return true;
        };
    access.startPublishedFlow =
        [&flowVoiceActive, &flowStartCalls](
            const FunctionFlowTriggerRequest &) {
            flowVoiceActive = true;
            ++flowStartCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.beginRecording = [&classicStarts](const QString &) {
        ++classicStarts;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function =
        functionSettings(QStringLiteral("ask"), false, true);
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::FlowStarted
    );
    QVERIFY(flowVoiceActive);

    settings.functions[0].executionMode =
        FunctionExecutionMode::Classic;
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(stopCount, 1);
    QCOMPARE(flowStartCalls, 1);
    QCOMPARE(classicStarts, 0);
}

void FunctionCommandControllerTests::
deletedClassicToggleStillStopsActiveRecording()
{
    bool recordingActive = false;
    int stopCount = 0;
    FunctionCommandAccess access;
    access.recordingOwnsPress =
        [&recordingActive](const QString &id) {
            return recordingActive
                && id == QStringLiteral("ask");
        };
    access.recordingConsumesPress =
        [&recordingActive, &stopCount](const QString &id) {
            if (!recordingActive || id != QStringLiteral("ask")) {
                return false;
            }
            recordingActive = false;
            ++stopCount;
            return true;
        };
    access.beginRecording = [&recordingActive](const QString &) {
        recordingActive = true;
    };
    access.speechConfigurationError = [](const QString &) {
        return QString();
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    settings.functions.append(functionSettings(
        QStringLiteral("ask"),
        false,
        true
    ));
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingStarted
    );

    settings.functions.clear();
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(stopCount, 1);
}

void FunctionCommandControllerTests::
differentKnownFunctionKeepsActiveRecordingGuard()
{
    bool recordingActive = true;
    int consumeCalls = 0;
    int classicStarts = 0;
    FunctionCommandAccess access;
    access.recordingOwnsPress =
        [&recordingActive](const QString &id) {
            return recordingActive
                && id == QStringLiteral("active");
        };
    access.recordingConsumesPress =
        [&consumeCalls](const QString &) {
            ++consumeCalls;
            return true;
        };
    access.beginAction = [&classicStarts]() {
        ++classicStarts;
    };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    settings.functions.append(functionSettings(
        QStringLiteral("active"),
        false,
        true
    ));
    settings.functions.append(functionSettings(
        QStringLiteral("other"),
        true,
        false
    ));
    controller.updateConfiguration(settings);

    QCOMPARE(
        controller.handleHotkey(QStringLiteral("other")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(consumeCalls, 1);
    QCOMPARE(classicStarts, 0);
}

void FunctionCommandControllerTests::
holdOwnerUsesModeAtRegisteredHotkeyDispatch()
{
    int classicReleases = 0;
    int flowReleases = 0;
    FunctionCommandAccess access;
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };

    FunctionCommandController controller(access);
    AppSettingsData settings;
    FunctionSettings function =
        functionSettings(QStringLiteral("ask"), false, true);
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    // 物理 Pressed 尚未派发主 WM_HOTKEY，此时切换到 Canvas。
    settings.functions[0].executionMode =
        FunctionExecutionMode::Canvas;
    controller.updateConfiguration(settings);
    // 主 WM_HOTKEY 同步片段在真正启动前冻结当前 Canvas owner。
    controller.handleHotkeyPressed(QStringLiteral("ask"));

    settings.functions[0].executionMode =
        FunctionExecutionMode::Classic;
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkeyReleased(QStringLiteral("ask")),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(classicReleases, 0);
    QCOMPARE(flowReleases, 1);
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
    QVERIFY(contents.contains("handleHotkeyPressed"));
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

void FunctionCommandControllerTests::
resolvesFloatingStyleOnlyForRunsThatActuallyStart()
{
    QStringList styles;
    int flowCalls = 0;
    FunctionFlowStartOutcome nextFlowOutcome =
        FunctionFlowStartOutcome::Started;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return FunctionCommandWindowHandle(nullptr);
    };
    access.beginAction = []() {};
    access.prepareFloatingBar = [&styles](
        bool,
        int,
        const QString &style) {
        styles.append(style);
    };
    access.startPublishedFlow = [&](const FunctionFlowTriggerRequest &) {
        ++flowCalls;
        return nextFlowOutcome;
    };
    access.readSelectedText = [](
        const SelectedTextWorkflowRequest &) {
        SelectedTextWorkflowResult result;
        result.text = QStringLiteral("text");
        return result;
    };

    AppSettingsData settings;
    settings.floatingBarStyle = QStringLiteral("liveTranscriptCard");
    FunctionSettings inherited = functionSettings(
        QStringLiteral("custom_inherit"), true, false);
    inherited.output.floatingBarStyleOverride = QStringLiteral("inherit");
    FunctionSettings overridden = functionSettings(
        QStringLiteral("custom_override"), true, false);
    overridden.output.floatingBarStyleOverride = QStringLiteral("statusPill");
    FunctionSettings builtIn = functionSettings(
        QStringLiteral("dictate"), true, false);
    builtIn.builtIn = true;
    builtIn.output.floatingBarStyleOverride =
        QStringLiteral("liveTranscriptCard");
    FunctionSettings canvas = functionSettings(
        QStringLiteral("custom_canvas"), false, false, true,
        screenshotTriggerSeparate());
    canvas.executionMode = FunctionExecutionMode::Canvas;
    settings.functions << inherited << overridden << builtIn << canvas;

    FunctionCommandController controller(access);
    controller.updateConfiguration(settings);
    QCOMPARE(controller.handleHotkey(inherited.id),
             FunctionCommandOutcome::TextSubmitted);
    QCOMPARE(styles.takeLast(), QStringLiteral("liveTranscriptCard"));
    QCOMPARE(controller.handleHotkey(overridden.id),
             FunctionCommandOutcome::TextSubmitted);
    QCOMPARE(styles.takeLast(), QStringLiteral("statusPill"));

    settings.floatingBarStyle = QStringLiteral("invalid-global");
    controller.updateConfiguration(settings);
    QCOMPARE(controller.handleHotkey(builtIn.id),
             FunctionCommandOutcome::TextSubmitted);
    QCOMPARE(styles.takeLast(), QStringLiteral("statusPill"));

    nextFlowOutcome = FunctionFlowStartOutcome::Started;
    QCOMPARE(controller.handleScreenshotTrigger(canvas.id),
             FunctionCommandOutcome::FlowStarted);
    QCOMPARE(styles.takeLast(), QStringLiteral("statusPill"));
    const int preparedAfterStart = styles.size();
    nextFlowOutcome = FunctionFlowStartOutcome::Busy;
    QCOMPARE(controller.handleScreenshotTrigger(canvas.id),
             FunctionCommandOutcome::FlowBusy);
    QCOMPARE(styles.size(), preparedAfterStart);
    QCOMPARE(controller.handleHotkey(QStringLiteral("unknown")),
             FunctionCommandOutcome::NoAction);
    QCOMPARE(styles.size(), preparedAfterStart);
    QCOMPARE(flowCalls, 2);
}

QTEST_APPLESS_MAIN(FunctionCommandControllerTests)

#include "function_command_controller_tests.moc"
