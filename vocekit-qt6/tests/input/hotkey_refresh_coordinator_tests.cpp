#include <QtTest>

#include "../../src/capture/screenshot_types.h"
#include "../../src/domain/function_settings.h"
#include "../../src/input/hold_to_talk.h"
#include "../../src/input/hotkey_refresh_coordinator.h"

#include <QFile>

namespace {

GlobalHotkeySettingsSnapshot snapshotWithShortcut(
    const QString &shortcut)
{
    GlobalHotkeySettingsSnapshot snapshot;
    GlobalHotkeyFunction function;
    function.id = QStringLiteral("ask");
    function.shortcut = shortcut;
    snapshot.functions.append(function);
    return snapshot;
}

QString shortcutFromSnapshot(
    const GlobalHotkeySettingsSnapshot &snapshot)
{
    return snapshot.functions.isEmpty()
        ? QString()
        : snapshot.functions.first().shortcut;
}

} // namespace

class HotkeyRefreshCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void appliesImmediatelyWithoutPressedHold()
    {
        QStringList applied;
        HotkeyRefreshCoordinator coordinator(
            [&applied](const GlobalHotkeySettingsSnapshot &snapshot) {
                applied.append(shortcutFromSnapshot(snapshot));
            }
        );

        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+1")),
            false
        );

        QCOMPARE(applied, QStringList() << QStringLiteral("Alt+1"));
    }

    void defersRefreshUntilTrackedHoldIsReleased()
    {
        QStringList applied;
        HotkeyRefreshCoordinator coordinator(
            [&applied](const GlobalHotkeySettingsSnapshot &snapshot) {
                applied.append(shortcutFromSnapshot(snapshot));
            }
        );

        coordinator.holdPressed(QStringLiteral("ask"));
        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+2")),
            true
        );
        QVERIFY(applied.isEmpty());

        coordinator.holdReleased(QStringLiteral("ask"), false);
        QCOMPARE(applied, QStringList() << QStringLiteral("Alt+2"));
    }

    void latestDeferredRefreshWins()
    {
        QStringList applied;
        HotkeyRefreshCoordinator coordinator(
            [&applied](const GlobalHotkeySettingsSnapshot &snapshot) {
                applied.append(shortcutFromSnapshot(snapshot));
            }
        );

        coordinator.holdPressed(QStringLiteral("ask"));
        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+2")),
            true
        );
        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+3")),
            true
        );
        coordinator.holdReleased(QStringLiteral("ask"), false);

        QCOMPARE(applied, QStringList() << QStringLiteral("Alt+3"));
    }

    void multipleHoldsApplyOnlyLatestAfterTheLastRelease()
    {
        QStringList applied;
        HotkeyRefreshCoordinator coordinator(
            [&applied](const GlobalHotkeySettingsSnapshot &snapshot) {
                applied.append(shortcutFromSnapshot(snapshot));
            }
        );

        coordinator.holdPressed(QStringLiteral("a"));
        coordinator.holdPressed(QStringLiteral("b"));
        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+5")),
            true
        );
        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+6")),
            true
        );

        coordinator.holdReleased(QStringLiteral("a"), false);
        QVERIFY(applied.isEmpty());
        coordinator.holdReleased(QStringLiteral("b"), false);
        QCOMPARE(applied, QStringList() << QStringLiteral("Alt+6"));
    }

    void physicalPressDefersBeforePressedCallbackRuns()
    {
        QStringList applied;
        HotkeyRefreshCoordinator coordinator(
            [&applied](const GlobalHotkeySettingsSnapshot &snapshot) {
                applied.append(shortcutFromSnapshot(snapshot));
            }
        );

        coordinator.requestRefresh(
            snapshotWithShortcut(QStringLiteral("Alt+4")),
            true
        );
        QVERIFY(applied.isEmpty());

        coordinator.holdPressed(QStringLiteral("ask"));
        coordinator.holdReleased(QStringLiteral("ask"), false);
        QCOMPARE(applied, QStringList() << QStringLiteral("Alt+4"));
    }

    void matcherExposesPhysicalPressedState()
    {
        HoldShortcutMatcher matcher;
        QVERIFY(matcher.configure(QStringLiteral("Ctrl+A")));
        QVERIFY(!matcher.isPressed());

        QCOMPARE(
            matcher.process(
                'A',
                HoldModifierControl,
                true,
                false
            ),
            HoldShortcutTransition::Pressed
        );
        QVERIFY(matcher.isPressed());

        QCOMPARE(
            matcher.process(
                'A',
                HoldModifierControl,
                false,
                false
            ),
            HoldShortcutTransition::Released
        );
        QVERIFY(!matcher.isPressed());
    }

    void registeredHoldDispatchFreezesOwnerBeforeStarting()
    {
        QStringList events;
        FunctionExecutionMode mode = FunctionExecutionMode::Classic;
        FunctionExecutionMode frozenMode =
            FunctionExecutionMode::Classic;

        // 低级 Pressed 后、主 WM_HOTKEY 前发生模式切换。
        mode = FunctionExecutionMode::Canvas;
        dispatchRegisteredHotkeyPress(
            QStringLiteral(" ask "),
            QSet<QString>() << QStringLiteral("ask"),
            [&events](const QString &id) {
                events.append(QStringLiteral("track:") + id);
            },
            [&events, &mode, &frozenMode](const QString &id) {
                events.append(QStringLiteral("freeze:") + id);
                frozenMode = mode;
            },
            [&events](const QString &id) {
                events.append(QStringLiteral("start:") + id);
            }
        );

        QCOMPARE(
            events,
            QStringList()
                << QStringLiteral("track:ask")
                << QStringLiteral("freeze:ask")
                << QStringLiteral("start:ask")
        );
        QCOMPARE(frozenMode, FunctionExecutionMode::Canvas);
    }

    void nonHoldAndScreenshotDispatchNeverFreezeOwner()
    {
        QStringList frozen;
        QStringList started;
        const QSet<QString> activeHolds =
            QSet<QString>() << QStringLiteral("ask");
        const QString screenshot =
            screenshotHotkeyLogicalId(QStringLiteral("ask"));

        dispatchRegisteredHotkeyPress(
            QStringLiteral("missing"),
            activeHolds,
            [](const QString &) {},
            [&frozen](const QString &id) {
                frozen.append(id);
            },
            [&started](const QString &id) {
                started.append(id);
            }
        );
        dispatchRegisteredHotkeyPress(
            screenshot,
            activeHolds,
            [](const QString &) {},
            [&frozen](const QString &id) {
                frozen.append(id);
            },
            [&started](const QString &id) {
                started.append(id);
            }
        );

        QVERIFY(frozen.isEmpty());
        QCOMPARE(
            started,
            QStringList()
                << QStringLiteral("missing")
                << screenshot
        );
    }

    void runtimeWiresPressedOwnerAndDeferredRefresh()
    {
        const QString path = QFINDTESTDATA(
            "../../src/app/vocekit_application_runtime.cpp"
        );
        QVERIFY2(!path.isEmpty(), "找不到应用运行时源码");
        QFile source(path);
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QByteArray contents = source.readAll();

        const int mainCallback = contents.indexOf(
            "hotkeys.setCallback("
        );
        const int holdCallback = contents.indexOf(
            "hotkeys.setHoldCallback("
        );
        QVERIFY(mainCallback >= 0);
        QVERIFY(holdCallback > mainCallback);
        const QByteArray mainBlock = contents.mid(
            mainCallback,
            holdCallback - mainCallback
        );
        QVERIFY(mainBlock.contains(
            "dispatchRegisteredHotkeyPress("
        ));

        const int filterInstall = contents.indexOf(
            "installNativeEventFilter",
            holdCallback
        );
        QVERIFY(filterInstall > holdCallback);
        const QByteArray holdBlock = contents.mid(
            holdCallback,
            filterInstall - holdCallback
        );
        QVERIFY(holdBlock.contains(
            "hotkeyRefreshCoordinator.holdPressed(id)"
        ));
        QVERIFY(!holdBlock.contains("handleHotkeyPressed"));
        QVERIFY(contents.contains(
            "hotkeyRefreshCoordinator.requestRefresh("
        ));
        const int releaseOwner = contents.indexOf(
            "controllerGuard->handleHotkeyReleased(id)"
        );
        const int releaseRefresh = contents.indexOf(
            "hotkeyRefreshCoordinator.holdReleased("
        );
        QVERIFY(releaseOwner >= 0);
        QVERIFY(releaseRefresh > releaseOwner);

        const int eventLoopExit = contents.indexOf(
            "const int exitCode = app.exec();"
        );
        const int holdCallbackClear = contents.indexOf(
            "hotkeys.setHoldCallback(std::function<void(const QString &, "
            "HoldShortcutTransition)>());",
            eventLoopExit
        );
        const int filterRemove = contents.indexOf(
            "qApp->removeNativeEventFilter(&hotkeys);",
            eventLoopExit
        );
        QVERIFY(eventLoopExit > filterInstall);
        QVERIFY(holdCallbackClear > eventLoopExit);
        QVERIFY(filterRemove > holdCallbackClear);
    }
};

QTEST_APPLESS_MAIN(HotkeyRefreshCoordinatorTests)

#include "hotkey_refresh_coordinator_tests.moc"
