#include <QtTest>

#include "../../src/input/selection_observer.h"

#include <QApplication>
#include <QLineEdit>
#include <QVector>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

unsigned int leftButtonDownMessage()
{
#ifdef Q_OS_WIN
    return WM_LBUTTONDOWN;
#else
    return 0x0201;
#endif
}

unsigned int leftButtonUpMessage()
{
#ifdef Q_OS_WIN
    return WM_LBUTTONUP;
#else
    return 0x0202;
#endif
}

unsigned int keyDownMessage()
{
#ifdef Q_OS_WIN
    return WM_KEYDOWN;
#else
    return 0x0100;
#endif
}

unsigned int keyUpMessage()
{
#ifdef Q_OS_WIN
    return WM_KEYUP;
#else
    return 0x0101;
#endif
}

int reasonCount(
    const QVector<SelectionObservation> &observations,
    SelectionObservationReason reason)
{
    int count = 0;
    for (const SelectionObservation &observation : observations) {
        if (observation.reason == reason) {
            ++count;
        }
    }
    return count;
}

SelectedTextNativeWindowHandle nativeHandle(QWidget *widget)
{
    return reinterpret_cast<SelectedTextNativeWindowHandle>(
        quintptr(widget->winId())
    );
}

} // namespace

class SelectionObserverTests : public QObject
{
    Q_OBJECT

private slots:
    void everyExternalMouseReleaseCreatesOneProbeCandidate()
    {
        SelectionObservationMatcher matcher;
        matcher.mousePressed(QPoint(40, 40));
        QVERIFY(matcher.mouseReleased(QPoint(42, 42)));
        QVERIFY(matcher.mouseReleased(QPoint(42, 42)));
    }

    void mouseDragAndKeyboardSelectionEachEmitOnce()
    {
        SelectionObservationMatcher matcher;
        matcher.mousePressed(QPoint(10, 10));
        QVERIFY(matcher.mouseReleased(QPoint(60, 10)));
        matcher.keyPressed(0x10);
        QVERIFY(matcher.keyReleased(0x27));
        QVERIFY(!matcher.keyReleased(0x27));
        matcher.keyPressed(0x27);
        QVERIFY(matcher.keyReleased(0x27));
    }

    void ctrlAAndOrdinaryTypingDoNotRetainKeys()
    {
        SelectionObservationMatcher matcher;
        matcher.keyPressed(0x11);
        matcher.keyPressed('A');
        QVERIFY(matcher.keyReleased('A'));
        QVERIFY(!matcher.keyReleased('A'));
        matcher.keyReleased(0x11);

        matcher.keyPressed('X');
        QVERIFY(!matcher.keyReleased('X'));
        matcher.keyPressed(0x27);
        QVERIFY(!matcher.keyReleased(0x27));
    }

    void shortcutCandidateIsIndependentOfAutomaticPause()
    {
        SelectionObservationMatcher matcher;
        matcher.setPaused(true);
        QVERIFY(!matcher.mouseReleased(QPoint(90, 90)));
        QVERIFY(matcher.fallbackShortcutReleased());
        matcher.setAutomaticEnabled(false);
        QVERIFY(matcher.fallbackShortcutReleased());
    }

    void escapeRequiresAVisibleSelectionSurface()
    {
        SelectionObservationMatcher matcher;
        QVERIFY(!matcher.escapeReleased());
        matcher.setSurfaceVisible(true);
        QVERIFY(matcher.escapeReleased());
        QVERIFY(!matcher.escapeReleased());
    }

    void plainPointerReleaseEmitsOutsideAndCandidateExactlyOnce()
    {
        QWidget host;
        host.setGeometry(100, 100, 320, 180);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));

        const QPoint point = host.mapToGlobal(QPoint(40, 40));
        observer.processNativeMouse(leftButtonUpMessage(), point);
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::OutsidePointerRelease),
            1
        );
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::MouseSelection),
            1
        );
        observer.uninstall();
    }

    void doubleClickWordSelectionIsNotMissed()
    {
        QWidget host;
        host.setGeometry(120, 120, 320, 180);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));

        const QPoint point = host.mapToGlobal(QPoint(70, 60));
        for (int i = 0; i < 2; ++i) {
            observer.processNativeMouse(leftButtonDownMessage(), point);
            observer.processNativeMouse(leftButtonUpMessage(), point);
        }
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::MouseSelection),
            2
        );
        observer.uninstall();
    }

    void childForegroundWindowIsNormalizedToItsRootWindow()
    {
        QWidget host;
        host.setGeometry(140, 140, 360, 220);
        QLineEdit child(&host);
        child.setAttribute(Qt::WA_NativeWindow);
        child.setGeometry(30, 40, 220, 36);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            if (observation.reason
                == SelectionObservationReason::ForegroundChanged) {
                observations.append(observation);
            }
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));

        // The host can already be the foreground root under Qt 6. Force a
        // distinct prior state so this test verifies child-to-root
        // normalization instead of depending on window-manager focus timing.
        observer.processForegroundWindowChanged(nullptr);
        QTRY_COMPARE(observations.size(), 1);
        observations.clear();
        observer.processForegroundWindowChanged(nativeHandle(&child));
        QTRY_COMPARE(observations.size(), 1);
#ifdef Q_OS_WIN
        const HWND expected = GetAncestor(
            reinterpret_cast<HWND>(quintptr(child.winId())),
            GA_ROOT
        );
        QCOMPARE(
            observations.constFirst().targetWindow,
            reinterpret_cast<SelectedTextNativeWindowHandle>(expected)
        );
#else
        QCOMPARE(observations.constFirst().targetWindow, nativeHandle(&child));
#endif
        observer.uninstall();
    }

    void keyboardSelectionAndEscapeAreQueuedExactlyOnce()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));

        observer.processNativeKey(keyDownMessage(), 0x10);
        observer.processNativeKey(keyDownMessage(), 0x27);
        observer.processNativeKey(keyUpMessage(), 0x27);
        observer.processNativeKey(keyUpMessage(), 0x10);
        observer.setSurfaceVisible(true);
        observer.processNativeKey(keyUpMessage(), 0x1b);
        observer.processNativeKey(keyUpMessage(), 0x1b);

        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::KeyboardSelection),
            1
        );
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::EscapePressed),
            1
        );
        observer.uninstall();
    }

    void fallbackShortcutIgnoresAutomaticPause()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));
        observer.setPaused(true);
        observer.setAutomaticEnabled(false);
        observer.triggerFallbackShortcut();
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::FallbackShortcut),
            1
        );
        observer.uninstall();
    }

    void foregroundChangeCarriesTheNewNativeWindowOnce()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));
        const SelectedTextNativeWindowHandle changed =
            reinterpret_cast<SelectedTextNativeWindowHandle>(quintptr(0x7711));
        observer.processForegroundWindowChanged(changed);
        observer.processForegroundWindowChanged(changed);
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::ForegroundChanged),
            1
        );
        QCOMPARE(observations.constLast().targetWindow, changed);
        observer.uninstall();
    }

    void sessionLockAndSuspendResetAndUnlockDoesNotReplay()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver observer;
        QVector<SelectionObservation> observations;
        observer.setCallback([&observations](
            const SelectionObservation &observation) {
            observations.append(observation);
        });
        QString error;
        QVERIFY2(observer.install(nativeHandle(&host), &error),
                 qPrintable(error));

        observer.processNativeKey(keyDownMessage(), 0x10);
        observer.processSystemAvailabilityChanged(false);
        observer.processNativeKey(keyUpMessage(), 0x27);
        observer.processSystemAvailabilityChanged(false);
        observer.processSystemAvailabilityChanged(true);
        observer.processSystemAvailabilityChanged(true);
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::SystemUnavailable),
            1
        );
        QTRY_COMPARE(
            reasonCount(observations,
                        SelectionObservationReason::SystemAvailable),
            1
        );
        QCOMPARE(
            reasonCount(observations,
                        SelectionObservationReason::KeyboardSelection),
            0
        );
        observer.uninstall();
    }

    void repeatedInstallAndUninstallLeavesNoCallableHook()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver observer;
        for (int i = 0; i < 50; ++i) {
            QString error;
            QVERIFY2(observer.install(nativeHandle(&host), &error),
                     qPrintable(error));
            QVERIFY(observer.isInstalled());
            observer.uninstall();
            QVERIFY(!observer.isInstalled());
        }

        int callbackCount = 0;
        observer.setCallback([&callbackCount](
            const SelectionObservation &) {
            ++callbackCount;
        });
        observer.processNativeMouse(
            leftButtonUpMessage(),
            host.mapToGlobal(QPoint(20, 20))
        );
        QTest::qWait(250);
        QCOMPARE(callbackCount, 0);
    }

    void callbackMayDestroyTheObserverWithoutLateAccess()
    {
        QWidget host;
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        SelectionObserver *observer = new SelectionObserver;
        int callbackCount = 0;
        observer->setCallback([&observer, &callbackCount](
            const SelectionObservation &) {
            ++callbackCount;
            SelectionObserver *doomed = observer;
            observer = nullptr;
            delete doomed;
        });
        QString error;
        QVERIFY2(observer->install(nativeHandle(&host), &error),
                 qPrintable(error));
        observer->triggerFallbackShortcut();
        QTRY_COMPARE(callbackCount, 1);
        QCOMPARE(observer, static_cast<SelectionObserver *>(nullptr));
        QTest::qWait(220);
        QCOMPARE(callbackCount, 1);
    }
};

QTEST_MAIN(SelectionObserverTests)
#include "selection_observer_tests.moc"
