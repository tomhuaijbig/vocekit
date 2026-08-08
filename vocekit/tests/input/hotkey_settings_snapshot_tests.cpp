#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/input/hotkey_settings_snapshot.h"

namespace {

GlobalHotkeyFunction functionById(
    const GlobalHotkeySettingsSnapshot &snapshot,
    const QString &id)
{
    for (const GlobalHotkeyFunction &function : snapshot.functions) {
        if (function.id == id) {
            return function;
        }
    }
    return GlobalHotkeyFunction();
}

} // namespace

class HotkeySettingsSnapshotTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsBuiltInAndCustomFunctionsFromTypedSettings()
    {
        AppSettingsData settings;
        settings.applicationHotkeys.insert(
            QStringLiteral("dictate"),
            QStringLiteral("Alt+X")
        );
        settings.applicationHotkeys.insert(
            QStringLiteral("hub"),
            QStringLiteral("Alt+H")
        );

        FunctionSettings dictate;
        dictate.id = QStringLiteral("dictate");
        dictate.name = QStringLiteral("Dictate override");
        dictate.builtIn = true;
        dictate.input.useVoice = true;
        dictate.recording.triggerMode = QStringLiteral("hold");
        settings.functions.append(dictate);

        FunctionSettings custom;
        custom.id = QStringLiteral("custom-1");
        custom.name = QStringLiteral("Custom action");
        custom.shortcut = QStringLiteral("Alt+1");
        custom.input.useVoice = true;
        custom.input.useScreenshot = true;
        custom.input.screenshotTriggerMode = QStringLiteral("separate");
        custom.input.screenshotShortcut = QStringLiteral("Alt+Shift+1");
        settings.functions.append(custom);

        const GlobalHotkeySettingsSnapshot snapshot =
            globalHotkeySnapshotFromData(settings);

        QCOMPARE(snapshot.functions.size(), 6);
        const GlobalHotkeyFunction dictateHotkey =
            functionById(snapshot, QStringLiteral("dictate"));
        QCOMPARE(dictateHotkey.shortcut, QStringLiteral("Alt+X"));
        QCOMPARE(dictateHotkey.recordingTriggerMode, QStringLiteral("hold"));
        QCOMPARE(dictateHotkey.useVoice, true);

        const GlobalHotkeyFunction hub =
            functionById(snapshot, QStringLiteral("hub"));
        QCOMPARE(hub.shortcut, QStringLiteral("Alt+H"));
        QCOMPARE(hub.useVoice, false);

        const GlobalHotkeyFunction customHotkey =
            functionById(snapshot, QStringLiteral("custom-1"));
        QCOMPARE(customHotkey.title, QStringLiteral("Custom action"));
        QCOMPARE(customHotkey.shortcut, QStringLiteral("Alt+1"));
        QCOMPARE(customHotkey.useScreenshot, true);
        QCOMPARE(
            customHotkey.screenshotShortcut,
            QStringLiteral("Alt+Shift+1")
        );
    }

    void fallsBackToDefaultUtilityShortcuts()
    {
        const AppSettingsData settings;
        const GlobalHotkeySettingsSnapshot snapshot =
            globalHotkeySnapshotFromData(settings);

        const GlobalHotkeyFunction hub =
            functionById(snapshot, QStringLiteral("hub"));
        QCOMPARE(hub.shortcut, QStringLiteral("Ctrl+Alt+S"));
    }

    void canvasModeClearsClassicScreenshotEntrancesWithoutPublishedTrigger()
    {
        AppSettingsData settings;
        FunctionSettings custom;
        custom.id = QStringLiteral("custom-1");
        custom.name = QStringLiteral("Flow");
        custom.shortcut = QStringLiteral("Alt+1");
        custom.executionMode = FunctionExecutionMode::Canvas;
        custom.input.useVoice = true;
        custom.recording.triggerMode = QStringLiteral("toggle");
        custom.input.useScreenshot = true;
        custom.input.screenshotTriggerMode =
            QStringLiteral("separateAndLauncher");
        custom.input.screenshotShortcut =
            QStringLiteral("Alt+Shift+9");
        settings.functions.append(custom);

        QSharedPointer<FunctionFlowExecutionPlan> plan(
            new FunctionFlowExecutionPlan
        );
        FunctionFlowCompiledNode voice;
        voice.nodeId = QStringLiteral("voice");
        voice.type = FunctionFlowNodeType::VoiceSource;
        plan->nodes.insert(voice.nodeId, voice);
        FunctionFlowTriggerPlan main;
        main.available = true;
        main.activeSourceNodeIds << voice.nodeId;
        main.usesHoldToTalk = true;
        plan->triggers.insert(
            FunctionFlowTrigger::MainHotkey,
            main
        );

        int providerCalls = 0;
        const GlobalHotkeySettingsSnapshot snapshot =
            globalHotkeySnapshotFromData(
                settings,
                [&providerCalls, plan](const QString &id) {
                    ++providerCalls;
                    return id == QStringLiteral("custom-1")
                        ? QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >(plan)
                        : QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >();
                }
            );

        QCOMPARE(providerCalls, 1);
        const GlobalHotkeyFunction flow =
            functionById(snapshot, QStringLiteral("custom-1"));
        QVERIFY(flow.useHoldToTalk);
        QVERIFY(!flow.registerScreenshotHotkey);
        QVERIFY(!flow.useScreenshot);
        QVERIFY(flow.screenshotShortcut.isEmpty());
        QVERIFY(!functionUsesScreenshotLauncher(custom, plan));
    }

    void classicModeIgnoresPublishedMainHotkeyProfile()
    {
        AppSettingsData settings;
        FunctionSettings custom;
        custom.id = QStringLiteral("custom-2");
        custom.name = QStringLiteral("Classic voice");
        custom.shortcut = QStringLiteral("Alt+2");
        custom.executionMode = FunctionExecutionMode::Classic;
        custom.input.useVoice = true;
        custom.recording.triggerMode = QStringLiteral("toggle");
        settings.functions.append(custom);

        QSharedPointer<FunctionFlowExecutionPlan> plan(
            new FunctionFlowExecutionPlan
        );
        FunctionFlowCompiledNode voice;
        voice.nodeId = QStringLiteral("voice");
        voice.type = FunctionFlowNodeType::VoiceSource;
        plan->nodes.insert(voice.nodeId, voice);
        FunctionFlowTriggerPlan main;
        main.available = true;
        main.activeSourceNodeIds << voice.nodeId;
        main.usesHoldToTalk = true;
        plan->triggers.insert(
            FunctionFlowTrigger::MainHotkey,
            main
        );

        int providerCalls = 0;
        const GlobalHotkeySettingsSnapshot snapshot =
            globalHotkeySnapshotFromData(
                settings,
                [&providerCalls, plan](const QString &id) {
                    ++providerCalls;
                    return id == QStringLiteral("custom-2")
                        ? QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >(plan)
                        : QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >();
                }
            );
        const GlobalHotkeyFunction flow =
            functionById(snapshot, QStringLiteral("custom-2"));
        QCOMPARE(providerCalls, 0);
        QVERIFY(!flow.useHoldToTalk);
        QCOMPARE(
            flow.recordingTriggerMode,
            QStringLiteral("toggle")
        );
    }

    void canvasModeUsesPublishedScreenshotHotkeyProfile()
    {
        AppSettingsData settings;
        FunctionSettings custom;
        custom.id = QStringLiteral("custom-screenshot");
        custom.name = QStringLiteral("Canvas screenshot");
        custom.shortcut = QStringLiteral("Alt+3");
        custom.executionMode = FunctionExecutionMode::Canvas;
        custom.input.useScreenshot = true;
        custom.input.screenshotTriggerMode =
            QStringLiteral("separate");
        custom.input.screenshotShortcut =
            QStringLiteral("Alt+Shift+3");
        settings.functions.append(custom);

        QSharedPointer<FunctionFlowExecutionPlan> plan(
            new FunctionFlowExecutionPlan
        );
        FunctionFlowCompiledNode screenshot;
        screenshot.nodeId = QStringLiteral("screenshot");
        screenshot.type = FunctionFlowNodeType::ScreenshotSource;
        screenshot.config.screenshot.separateShortcut =
            QStringLiteral("Ctrl+Shift+8");
        plan->nodes.insert(screenshot.nodeId, screenshot);

        FunctionFlowTriggerPlan trigger;
        trigger.available = true;
        trigger.activeSourceNodeIds << screenshot.nodeId;
        plan->triggers.insert(
            FunctionFlowTrigger::ScreenshotHotkey,
            trigger
        );

        int providerCalls = 0;
        const GlobalHotkeySettingsSnapshot snapshot =
            globalHotkeySnapshotFromData(
                settings,
                [&providerCalls, plan](const QString &id) {
                    ++providerCalls;
                    return id
                            == QStringLiteral(
                                "custom-screenshot"
                            )
                        ? QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >(plan)
                        : QSharedPointer<
                            const FunctionFlowExecutionPlan
                          >();
                }
            );

        QCOMPARE(providerCalls, 1);
        const GlobalHotkeyFunction flow =
            functionById(
                snapshot,
                QStringLiteral("custom-screenshot")
            );
        QVERIFY(flow.registerScreenshotHotkey);
        QVERIFY(flow.useScreenshot);
        QCOMPARE(
            flow.screenshotTriggerMode,
            QStringLiteral("separate")
        );
        QCOMPARE(
            flow.screenshotShortcut,
            QStringLiteral("Ctrl+Shift+8")
        );
    }

    void screenshotLauncherHelperUsesOnlyCanvasPublishedTrigger()
    {
        FunctionSettings function;
        function.executionMode = FunctionExecutionMode::Canvas;
        function.input.useScreenshot = true;
        function.input.screenshotTriggerMode =
            QStringLiteral("separateAndLauncher");

        QSharedPointer<FunctionFlowExecutionPlan> plan(
            new FunctionFlowExecutionPlan
        );
        FunctionFlowTriggerPlan launcher;
        launcher.available = true;
        plan->triggers.insert(
            FunctionFlowTrigger::ScreenshotLauncher,
            launcher
        );

        QVERIFY(functionUsesScreenshotLauncher(function, plan));
        plan->triggers.remove(
            FunctionFlowTrigger::ScreenshotLauncher
        );
        QVERIFY(!functionUsesScreenshotLauncher(function, plan));
        QVERIFY(!functionUsesScreenshotLauncher(
            function,
            QSharedPointer<const FunctionFlowExecutionPlan>()
        ));
    }

    void screenshotLauncherHelperUsesOnlyClassicSettingsInClassicMode()
    {
        FunctionSettings function;
        function.executionMode = FunctionExecutionMode::Classic;
        function.input.useScreenshot = true;
        function.input.screenshotTriggerMode =
            QStringLiteral("separateAndLauncher");

        QSharedPointer<FunctionFlowExecutionPlan> plan(
            new FunctionFlowExecutionPlan
        );
        QVERIFY(functionUsesScreenshotLauncher(function, plan));

        function.input.useScreenshot = false;
        FunctionFlowTriggerPlan launcher;
        launcher.available = true;
        plan->triggers.insert(
            FunctionFlowTrigger::ScreenshotLauncher,
            launcher
        );
        QVERIFY(!functionUsesScreenshotLauncher(function, plan));
    }
};

QTEST_MAIN(HotkeySettingsSnapshotTests)
#include "hotkey_settings_snapshot_tests.moc"
