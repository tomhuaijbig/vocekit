#include <QtTest>

#include "../../src/capture/screenshot_types.h"
#include "../../src/controllers/function_command_controller.h"

class FunctionFlowFallbackTests : public QObject
{
    Q_OBJECT

private slots:
    void classicModeNeverCallsFlow();
    void canvasModeNeverCallsClassic();
    void canvasLauncherNeverUsesClassicScreenshot();
    void canvasWithoutRuntimeServiceFailsClosed();
    void classicReleaseNeverCallsFlow();
    void canvasReleaseNeverCallsClassic();
    void unknownMainHotkeysFailClosed();
    void unknownScreenshotHotkeysFailClosed();
    void knownScreenshotHotkeyStopsAtActiveRecordingGuard();
    void unknownLaunchersFailClosed();
    void unknownReleasesFailClosed();
    void launcherUsesTrimmedKnownFunctionId();
    void currentFlowIsNotReportedAsClassicBusy();
};

namespace {

FunctionSettings configuredFunction(
    const QString &id,
    FunctionExecutionMode mode,
    bool useSelection = false)
{
    FunctionSettings function;
    function.id = id;
    function.name = id;
    function.executionMode = mode;
    function.input.useSelection = useSelection;
    function.input.order = QStringList()
        << functionInputSelectionId();
    return normalizeFunctionSettings(function);
}

AppSettingsData settingsWithFunction(
    const QString &id,
    FunctionExecutionMode mode,
    bool useSelection = false)
{
    AppSettingsData settings;
    settings.functions.append(
        configuredFunction(id, mode, useSelection)
    );
    return settings;
}

} // namespace

void FunctionFlowFallbackTests::classicModeNeverCallsFlow()
{
    int flowCalls = 0;
    int classicCalls = 0;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return reinterpret_cast<void *>(quintptr(91));
    };
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.screenshotActive = []() { return false; };
    access.processing = []() { return false; };
    access.recordingBusy = []() { return false; };
    access.readSelectedText =
        [](const SelectedTextWorkflowRequest &) {
            SelectedTextWorkflowResult result;
            result.text = QStringLiteral("classic input");
            return result;
        };
    access.processText =
        [&](const QString &, const QString &) {
            ++classicCalls;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        true
    ));
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("custom_1")),
        FunctionCommandOutcome::TextSubmitted
    );
    QCOMPARE(classicCalls, 1);
    QCOMPARE(flowCalls, 0);
}

void FunctionFlowFallbackTests::canvasModeNeverCallsClassic()
{
    int flowCalls = 0;
    int classicCalls = 0;
    QString error;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::NotAvailable;
        };
    access.showError = [&error](const QString &message) {
        error = message;
    };
    access.readSelectedText =
        [](const SelectedTextWorkflowRequest &) {
            SelectedTextWorkflowResult result;
            result.text = QStringLiteral("classic input");
            return result;
        };
    access.processText =
        [&classicCalls](const QString &, const QString &) {
            ++classicCalls;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        true
    ));
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("custom_1")),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(flowCalls, 1);
    QCOMPARE(classicCalls, 0);
    QCOMPARE(error, QStringLiteral("当前画布未配置此入口。"));
}

void FunctionFlowFallbackTests::
canvasLauncherNeverUsesClassicScreenshot()
{
    int flowCalls = 0;
    int classicScreenshots = 0;
    QString error;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::NotAvailable;
        };
    access.showError = [&error](const QString &message) {
        error = message;
    };
    access.beginScreenshot =
        [&classicScreenshots](const QString &, bool, bool) {
            ++classicScreenshots;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas
    ));
    QCOMPARE(
        controller.handleScreenshotLauncherTrigger(
            QStringLiteral("custom_1"),
            reinterpret_cast<void *>(quintptr(123))
        ),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(flowCalls, 1);
    QCOMPARE(classicScreenshots, 0);
    QCOMPARE(error, QStringLiteral("当前画布未配置此入口。"));
}

void FunctionFlowFallbackTests::
canvasWithoutRuntimeServiceFailsClosed()
{
    int classicCalls = 0;
    QString error;
    FunctionCommandAccess access;
    access.showError = [&error](const QString &message) {
        error = message;
    };
    access.processText =
        [&classicCalls](const QString &, const QString &) {
            ++classicCalls;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        true
    ));
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("custom_1")),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(classicCalls, 0);
    QCOMPARE(error, QStringLiteral("画布运行服务尚未初始化。"));
}

void FunctionFlowFallbackTests::
classicReleaseNeverCallsFlow()
{
    int flowReleases = 0;
    int classicReleases = 0;
    bool classicHandled = false;
    FunctionCommandAccess access;
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };
    access.recordingConsumesRelease =
        [&classicReleases, &classicHandled](const QString &) {
            ++classicReleases;
            return classicHandled;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    ));
    controller.handleHotkeyPressed(QStringLiteral("custom_1"));
    QCOMPARE(
        controller.handleHotkeyReleased(
            QStringLiteral("custom_1")
        ),
        FunctionCommandOutcome::NoAction
    );
    classicHandled = true;
    controller.handleHotkeyPressed(QStringLiteral("custom_1"));
    QCOMPARE(
        controller.handleHotkeyReleased(
            QStringLiteral("custom_1")
        ),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(flowReleases, 0);
    QCOMPARE(classicReleases, 2);
}

void FunctionFlowFallbackTests::
canvasReleaseNeverCallsClassic()
{
    int flowReleases = 0;
    int classicReleases = 0;
    bool flowHandled = false;
    FunctionCommandAccess access;
    access.releasePublishedFlowHold =
        [&flowReleases, &flowHandled](const QString &) {
            ++flowReleases;
            return flowHandled;
        };
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas
    ));
    controller.handleHotkeyPressed(QStringLiteral("custom_1"));
    QCOMPARE(
        controller.handleHotkeyReleased(
            QStringLiteral("custom_1")
        ),
        FunctionCommandOutcome::NoAction
    );
    flowHandled = true;
    controller.handleHotkeyPressed(QStringLiteral("custom_1"));
    QCOMPARE(
        controller.handleHotkeyReleased(
            QStringLiteral("custom_1")
        ),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(flowReleases, 2);
    QCOMPARE(classicReleases, 0);
}

void FunctionFlowFallbackTests::
unknownMainHotkeysFailClosed()
{
    int flowCalls = 0;
    int classicCalls = 0;
    int ownerChecks = 0;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.recordingConsumesPress =
        [&classicCalls](const QString &) {
            ++classicCalls;
            return true;
        };
    access.recordingOwnsPress =
        [&ownerChecks](const QString &id) {
            ++ownerChecks;
            return id == QStringLiteral("custom_1");
        };
    access.beginAction = [&classicCalls]() {
        ++classicCalls;
    };
    access.processText =
        [&classicCalls](const QString &, const QString &) {
            ++classicCalls;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        true
    ));
    const QStringList invalidIds = QStringList()
        << QStringLiteral("deleted_function")
        << QString();
    for (const QString &id : invalidIds) {
        QCOMPARE(
            controller.handleHotkey(id),
            FunctionCommandOutcome::NoAction
        );
    }
    QCOMPARE(flowCalls, 0);
    QCOMPARE(classicCalls, 0);
    QCOMPARE(ownerChecks, 1);
}

void FunctionFlowFallbackTests::
unknownScreenshotHotkeysFailClosed()
{
    int flowCalls = 0;
    int classicScreenshots = 0;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.beginScreenshot =
        [&classicScreenshots](const QString &, bool, bool) {
            ++classicScreenshots;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    ));
    const QStringList invalidIds = QStringList()
        << QStringLiteral("deleted_function")
        << QString();
    for (const QString &id : invalidIds) {
        QCOMPARE(
            controller.handleScreenshotTrigger(id),
            FunctionCommandOutcome::NoAction
        );
    }
    QCOMPARE(flowCalls, 0);
    QCOMPARE(classicScreenshots, 0);
}

void FunctionFlowFallbackTests::
knownScreenshotHotkeyStopsAtActiveRecordingGuard()
{
    int flowCalls = 0;
    int recordingCalls = 0;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.recordingConsumesPress =
        [&recordingCalls](const QString &) {
            ++recordingCalls;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas
    ));
    QCOMPARE(
        controller.handleHotkey(
            screenshotHotkeyLogicalId(
                QStringLiteral("custom_1")
            )
        ),
        FunctionCommandOutcome::RecordingHandled
    );
    QCOMPARE(recordingCalls, 1);
    QCOMPARE(flowCalls, 0);
}

void FunctionFlowFallbackTests::
unknownLaunchersFailClosed()
{
    int flowCalls = 0;
    int classicScreenshots = 0;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.beginScreenshot =
        [&classicScreenshots](const QString &, bool, bool) {
            ++classicScreenshots;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    ));
    const QStringList invalidIds = QStringList()
        << QStringLiteral("deleted_function")
        << QString();
    for (const QString &id : invalidIds) {
        QCOMPARE(
            controller.handleScreenshotLauncherTrigger(
                id,
                reinterpret_cast<void *>(quintptr(123))
            ),
            FunctionCommandOutcome::NoAction
        );
    }
    QCOMPARE(flowCalls, 0);
    QCOMPARE(classicScreenshots, 0);
}

void FunctionFlowFallbackTests::
unknownReleasesFailClosed()
{
    int flowReleases = 0;
    int classicReleases = 0;
    FunctionCommandAccess access;
    access.releasePublishedFlowHold =
        [&flowReleases](const QString &) {
            ++flowReleases;
            return true;
        };
    access.recordingConsumesRelease =
        [&classicReleases](const QString &) {
            ++classicReleases;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    ));
    const QStringList invalidIds = QStringList()
        << QStringLiteral("deleted_function")
        << QString();
    for (const QString &id : invalidIds) {
        QCOMPARE(
            controller.handleHotkeyReleased(id),
            FunctionCommandOutcome::NoAction
        );
    }
    QCOMPARE(flowReleases, 0);
    QCOMPARE(classicReleases, 0);
}

void FunctionFlowFallbackTests::
launcherUsesTrimmedKnownFunctionId()
{
    QString screenshotFunctionId;
    FunctionCommandAccess access;
    access.beginScreenshot =
        [&screenshotFunctionId](
            const QString &functionId,
            bool,
            bool) {
            screenshotFunctionId = functionId;
            return true;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    ));
    QCOMPARE(
        controller.handleScreenshotLauncherTrigger(
            QStringLiteral("  custom_1  "),
            reinterpret_cast<void *>(quintptr(123))
        ),
        FunctionCommandOutcome::ScreenshotStarted
    );
    QCOMPARE(
        screenshotFunctionId,
        QStringLiteral("custom_1")
    );
}

void FunctionFlowFallbackTests::
currentFlowIsNotReportedAsClassicBusy()
{
    bool observedClassicBusy = true;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return reinterpret_cast<void *>(quintptr(42));
    };
    access.processing = []() { return true; };
    access.classicProcessing = []() { return false; };
    access.screenshotActive = []() { return false; };
    access.recordingBusy = []() { return false; };
    access.startPublishedFlow =
        [&](const FunctionFlowTriggerRequest &request) {
            observedClassicBusy = request.classicWorkflowBusy;
            return FunctionFlowStartOutcome::Busy;
        };

    FunctionCommandController controller(access);
    controller.updateConfiguration(settingsWithFunction(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas
    ));
    QCOMPARE(
        controller.handleHotkey(QStringLiteral("custom_1")),
        FunctionCommandOutcome::FlowBusy
    );
    QVERIFY(!observedClassicBusy);
}

QTEST_MAIN(FunctionFlowFallbackTests)

#include "function_flow_fallback_tests.moc"
