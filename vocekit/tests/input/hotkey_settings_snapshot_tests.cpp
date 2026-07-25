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
};

QTEST_MAIN(HotkeySettingsSnapshotTests)
#include "hotkey_settings_snapshot_tests.moc"
