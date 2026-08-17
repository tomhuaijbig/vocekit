#include <QtTest>

#include "../../src/controllers/selection_context_coordinator.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QSet>

namespace {

SelectedTextNativeWindowHandle windowHandle(quintptr value)
{
    return reinterpret_cast<SelectedTextNativeWindowHandle>(value);
}

SelectionObservation observation(
    SelectionObservationReason reason,
    SelectedTextNativeWindowHandle window = windowHandle(1))
{
    SelectionObservation value;
    value.reason = reason;
    value.targetWindow = window;
    value.cursorPhysicalPosition = QPoint(100, 120);
    return value;
}

SelectionSnapshot snapshot(
    const QString &text,
    SelectedTextNativeWindowHandle window = windowHandle(1),
    const QRect &anchor = QRect(100, 100, 80, 20))
{
    SelectionSnapshot value;
    value.text = text;
    value.targetWindow = window;
    value.targetProcessId = 7;
    value.targetExecutable = QStringLiteral("notepad.exe");
    value.anchorRect = anchor;
    value.rectangles.append(anchor);
    return value;
}

struct ProbeStart
{
    SelectionProbeRequest request;
    bool strong = false;
    quint64 generation = 0;
    SelectionProbeRunnerCallbacks callbacks;
};

struct Harness
{
    SelectionContextSettings settings;
    QVector<ProbeStart> starts;
    QVector<SelectionSnapshot> shown;
    QVector<bool> keyboardModes;
    QVector<SelectionContextEligibility> manualFailures;
    QStringList logs;
    QSet<SelectedTextNativeWindowHandle> validWindows;
    QSet<SelectedTextNativeWindowHandle> ownedWindows;
    SelectedTextNativeWindowHandle foreground = windowHandle(1);
    int cancelProbe = 0;
    int hideToolbar = 0;
    int closeResult = 0;
    int cancelAction = 0;
    bool strongSelection = false;

    Harness()
    {
        settings.enabled = true;
        settings.keyboardSelectionEnabled = true;
        settings.minimumTextLength = 2;
        validWindows.insert(windowHandle(1));
        validWindows.insert(windowHandle(2));
    }

    SelectionContextCoordinatorAccess access()
    {
        SelectionContextCoordinatorAccess value;
        value.settingsSnapshot = [this]() { return settings; };
        value.strongSelectionEnabled = [this]() {
            return strongSelection;
        };
        value.startProbe = [this](
            const SelectionProbeRequest &request,
            bool strong,
            quint64 generation,
            const SelectionProbeRunnerCallbacks &callbacks) {
            ProbeStart start;
            start.request = request;
            start.strong = strong;
            start.generation = generation;
            start.callbacks = callbacks;
            starts.append(start);
        };
        value.cancelProbe = [this]() { ++cancelProbe; };
        value.currentProcessId = []() { return quint32(42); };
        value.targetWindowValid = [this](
            SelectedTextNativeWindowHandle window) {
            return validWindows.contains(window);
        };
        value.currentForegroundWindow = [this]() {
            return foreground;
        };
        value.showToolbar = [this](
            const SelectionSnapshot &value,
            bool keyboardMode) {
            shown.append(value);
            keyboardModes.append(keyboardMode);
        };
        value.hideToolbar = [this]() { ++hideToolbar; };
        value.closeUnpinnedResult = [this]() { ++closeResult; };
        value.cancelActiveAction = [this]() { ++cancelAction; };
        value.ownsSurfaceWindow = [this](
            SelectedTextNativeWindowHandle window) {
            return ownedWindows.contains(window);
        };
        value.showManualFailure = [this](
            SelectionContextEligibility failure) {
            manualFailures.append(failure);
        };
        value.logMetadata = [this](const QString &eventId, int textLength) {
            logs.append(eventId + QLatin1Char(':')
                        + QString::number(textLength));
        };
        return value;
    }

    void deliverLast(const SelectionSnapshot &value)
    {
        QVERIFY(!starts.isEmpty());
        const ProbeStart start = starts.constLast();
        start.callbacks.completed(start.generation, value);
    }
};

} // namespace

class SelectionContextCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void mouseReleaseDebouncesForOneHundredSixtyMilliseconds()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        QElapsedTimer timer;
        timer.start();
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection));
        QTest::qWait(100);
        QCOMPARE(h.starts.size(), 0);
        QTRY_COMPARE(h.starts.size(), 1);
        QVERIFY(timer.elapsed() >= 150);
    }

    void keyboardObservationHonorsItsSeparateSetting()
    {
        Harness h;
        h.settings.keyboardSelectionEnabled = false;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.handleObservation(observation(
            SelectionObservationReason::KeyboardSelection));
        QTest::qWait(220);
        QCOMPARE(h.starts.size(), 0);
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection));
        QTRY_COMPARE(h.starts.size(), 1);
    }

    void fallbackShortcutWorksWhileAutomaticObservationIsPaused()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.pauseForMinutes(30);
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection));
        QTest::qWait(220);
        QCOMPARE(h.starts.size(), 0);
        coordinator.triggerFallbackShortcut();
        QCOMPARE(h.starts.size(), 1);
        h.deliverLast(snapshot(QStringLiteral("manual")));
        QCOMPARE(h.shown.size(), 1);
        QVERIFY(h.keyboardModes.constLast());
    }

    void equivalentSnapshotDoesNotReopenToolbar()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        const SelectionSnapshot first = snapshot(QStringLiteral("same"));
        h.deliverLast(first);
        QCOMPARE(h.shown.size(), 1);
        coordinator.triggerFallbackShortcut();
        SelectionSnapshot near = first;
        near.anchorRect.translate(2, 2);
        h.deliverLast(near);
        QCOMPARE(h.shown.size(), 1);
    }

    void newSnapshotCancelsOnlyUnpinnedActiveRequest()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        h.deliverLast(snapshot(QStringLiteral("one")));
        coordinator.triggerFallbackShortcut();
        h.deliverLast(snapshot(QStringLiteral("two")));
        QCOMPARE(h.cancelAction, 1);
        QCOMPARE(h.closeResult, 1);
        coordinator.setResultPinned(true);
        coordinator.triggerFallbackShortcut();
        h.deliverLast(snapshot(QStringLiteral("three")));
        QCOMPARE(h.cancelAction, 1);
        QCOMPARE(h.closeResult, 1);
        QCOMPARE(h.shown.size(), 3);
    }

    void ownedSurfaceReleaseChangesNothing()
    {
        Harness h;
        h.ownedWindows.insert(windowHandle(99));
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        h.deliverLast(snapshot(QStringLiteral("one")));
        const int starts = h.starts.size();
        const int probeCancels = h.cancelProbe;
        const int actionCancels = h.cancelAction;
        coordinator.handleObservation(observation(
            SelectionObservationReason::OutsidePointerRelease,
            windowHandle(99)));
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection,
            windowHandle(99)));
        QTest::qWait(220);
        QCOMPARE(h.starts.size(), starts);
        QCOMPARE(h.cancelProbe, probeCancels);
        QCOMPARE(h.cancelAction, actionCancels);
    }

    void outsideEscapeAndForegroundCloseOnlyUnpinned()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        h.deliverLast(snapshot(QStringLiteral("one")));
        coordinator.handleObservation(observation(
            SelectionObservationReason::OutsidePointerRelease,
            windowHandle(2)));
        QCOMPARE(h.closeResult, 1);
        QCOMPARE(h.cancelAction, 1);
        coordinator.setResultPinned(true);
        coordinator.handleObservation(observation(
            SelectionObservationReason::EscapePressed));
        coordinator.handleObservation(observation(
            SelectionObservationReason::ForegroundChanged,
            windowHandle(2)));
        QCOMPARE(h.closeResult, 1);
        QCOMPARE(h.cancelAction, 1);
    }

    void sensitiveOwnProcessAndBlockedSnapshotsNeverShowUi_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("password") << 0;
        QTest::newRow("protected") << 1;
        QTest::newRow("own-process") << 2;
        QTest::newRow("blocked") << 3;
        QTest::newRow("secure-desktop") << 4;
    }

    void sensitiveOwnProcessAndBlockedSnapshotsNeverShowUi()
    {
        QFETCH(int, kind);
        Harness h;
        if (kind == 3) {
            h.settings.blockedApplications << QStringLiteral("NOTEPAD.EXE");
        }
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        SelectionSnapshot value = snapshot(QStringLiteral("secret"));
        if (kind == 0) {
            value.sensitivity = SelectionSensitivity::Password;
        } else if (kind == 1) {
            value.sensitivity = SelectionSensitivity::Protected;
        } else if (kind == 2) {
            value.targetProcessId = 42;
        } else if (kind == 4) {
            value.sensitivity = SelectionSensitivity::SecureDesktop;
        }
        h.deliverLast(value);
        QCOMPARE(h.shown.size(), 0);
    }

    void automaticPermissionFailureIsSilentButManualExplainsOnce()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection));
        QTRY_COMPARE(h.starts.size(), 1);
        SelectionSnapshot denied = snapshot(QStringLiteral("private"));
        denied.sensitivity = SelectionSensitivity::PermissionDenied;
        h.deliverLast(denied);
        QCOMPARE(h.manualFailures.size(), 0);
        coordinator.triggerFallbackShortcut();
        const ProbeStart manual = h.starts.constLast();
        manual.callbacks.completed(manual.generation, denied);
        manual.callbacks.completed(manual.generation, denied);
        QCOMPARE(h.manualFailures.size(), 1);
        QCOMPARE(
            h.manualFailures.constFirst(),
            SelectionContextEligibility::PermissionDenied
        );
    }

    void staleProbeAndForegroundLossAreRejected()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.triggerFallbackShortcut();
        const ProbeStart old = h.starts.constLast();
        coordinator.triggerFallbackShortcut();
        const ProbeStart current = h.starts.constLast();
        old.callbacks.completed(old.generation, snapshot(QStringLiteral("old")));
        QCOMPARE(h.shown.size(), 0);
        h.foreground = windowHandle(2);
        current.callbacks.completed(
            current.generation,
            snapshot(QStringLiteral("current"), windowHandle(1))
        );
        QCOMPARE(h.shown.size(), 0);
    }

    void slowProbeDoesNotBlockOwnerEventLoop()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        QElapsedTimer elapsed;
        elapsed.start();
        coordinator.handleObservation(observation(
            SelectionObservationReason::MouseSelection));
        QVERIFY(elapsed.elapsed() < 30);
        int ticks = 0;
        QTimer::singleShot(20, [&ticks]() { ++ticks; });
        QTRY_COMPARE(ticks, 1);
    }

    void stoppingInvalidatesAndCancelsExactlyOnce()
    {
        Harness h;
        SelectionContextCoordinator coordinator(h.access());
        coordinator.start();
        coordinator.stop();
        const int cancelAfterStop = h.cancelProbe;
        QCOMPARE(cancelAfterStop, 1);
        coordinator.stop();
        QCOMPARE(h.cancelProbe, cancelAfterStop);
    }

    void callbackMayDestroyCoordinatorSynchronously()
    {
        Harness h;
        SelectionContextCoordinator *coordinator = nullptr;
        SelectionContextCoordinatorAccess access = h.access();
        access.showToolbar = [&coordinator](const SelectionSnapshot &, bool) {
            SelectionContextCoordinator *doomed = coordinator;
            coordinator = nullptr;
            delete doomed;
        };
        coordinator = new SelectionContextCoordinator(access);
        coordinator->start();
        coordinator->triggerFallbackShortcut();
        const ProbeStart start = h.starts.constLast();
        start.callbacks.completed(
            start.generation,
            snapshot(QStringLiteral("delete"))
        );
        QCOMPARE(
            coordinator,
            static_cast<SelectionContextCoordinator *>(nullptr)
        );
    }
};

QTEST_MAIN(SelectionContextCoordinatorTests)
#include "selection_context_coordinator_tests.moc"
