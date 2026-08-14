#include <QtTest>

#include "../../src/input/selected_text_reader.h"
#include "../../src/input/selection_coordinate_mapper.h"
#include "../../src/input/selection_probe_runner.h"
#include "../../src/input/selection_snapshot.h"

#include <QApplication>
#include <QAtomicInt>
#include <QClipboard>
#include <QElapsedTimer>
#include <QMimeData>
#include <QThread>

namespace {

SelectedTextNativeWindowHandle fakeWindow()
{
    return reinterpret_cast<SelectedTextNativeWindowHandle>(quintptr(0x1234));
}

SelectionProbeRequest requestAt(int x)
{
    SelectionProbeRequest request;
    request.targetWindow = fakeWindow();
    request.cursorPhysicalPosition = QPoint(x, 20);
    return request;
}

SelectionPhysicalProbeResult textResult(
    const QString &text,
    quint32 processId = 42)
{
    SelectionPhysicalProbeResult result;
    result.snapshotWithoutGeometry.text = text;
    result.snapshotWithoutGeometry.targetWindow = fakeWindow();
    result.snapshotWithoutGeometry.targetProcessId = processId;
    result.snapshotWithoutGeometry.method =
        SelectionAcquisitionMethod::UiAutomation;
    return result;
}

} // namespace

class SelectedTextProbeTests : public QObject
{
    Q_OBJECT

private slots:
    void choosesLastValidRectangleNearestCursor()
    {
        const QVector<QRect> rectangles = QVector<QRect>()
            << QRect(100, 100, 240, 20)
            << QRect(100, 122, 330, 20);

        QCOMPARE(
            selectionAnchorRectangle(rectangles, QPoint(430, 240)),
            QRect(100, 122, 330, 20)
        );
    }

    void normalizesFlatBoundsAndRejectsMalformedValues()
    {
        const QVector<QRect> rectangles = selectionRectanglesFromFlatBounds(
            QVector<double>()
                << 10.4 << 20.6 << 30.2 << 40.8
                << 1.0 << 2.0 << -3.0 << 4.0
                << qQNaN() << 1.0 << 2.0 << 3.0
                << 100.0 << 200.0 << 10.0
        );

        QCOMPARE(rectangles, QVector<QRect>() << QRect(10, 21, 30, 41));
    }

    void rejectsSensitiveSelectionEvenWhenTextExists()
    {
        SelectionSnapshot snapshot;
        snapshot.text = QStringLiteral("secret-value");
        snapshot.sensitivity = SelectionSensitivity::Password;
        QVERIFY(!snapshot.isUsable());

        snapshot.sensitivity = SelectionSensitivity::Normal;
        QVERIFY(snapshot.isUsable());
    }

    void secureDesktopDecisionIsExplicit()
    {
        QVERIFY(selectionInputDesktopIsSecure(false, QString()));
        QVERIFY(selectionInputDesktopIsSecure(true, QStringLiteral("Winlogon")));
        QVERIFY(!selectionInputDesktopIsSecure(true, QStringLiteral(" default ")));
    }

    void clipboardRestoreRequiresExactOwnershipAndForeground()
    {
        QVERIFY(selectionClipboardOwnershipMatches(7, 7, 42, 42, true));
        QVERIFY(!selectionClipboardOwnershipMatches(7, 8, 42, 42, true));
        QVERIFY(!selectionClipboardOwnershipMatches(7, 7, 42, 9, true));
        QVERIFY(!selectionClipboardOwnershipMatches(7, 7, 42, 42, false));
    }

    void mapsPhysicalSelectionToTheMatchedQtScreen()
    {
        SelectionMonitorGeometry monitor;
        monitor.deviceName = QStringLiteral("\\\\.\\DISPLAY2");
        monitor.physicalGeometry = QRect(-2560, 0, 2560, 1440);
        monitor.logicalGeometry = QRect(-1707, 0, 1707, 960);
        monitor.logicalAvailableGeometry = QRect(-1707, 0, 1707, 920);

        QCOMPARE(
            selectionPhysicalToLogical(
                QRect(-1280, 720, 300, 60),
                monitor
            ),
            QRect(-854, 480, 200, 40)
        );
    }

    void multiRectangleSelectionMapsEachRectangleThroughItsOwnMonitor()
    {
        SelectionMonitorGeometry left;
        left.deviceName = QStringLiteral("DISPLAY1");
        left.physicalGeometry = QRect(-2000, 0, 2000, 1200);
        left.logicalGeometry = QRect(-1000, 0, 1000, 600);
        left.logicalAvailableGeometry = left.logicalGeometry;

        SelectionMonitorGeometry right;
        right.deviceName = QStringLiteral("DISPLAY2");
        right.physicalGeometry = QRect(0, 0, 1920, 1080);
        right.logicalGeometry = QRect(0, 0, 1920, 1080);
        right.logicalAvailableGeometry = right.logicalGeometry;

        SelectionPhysicalProbeResult physical = textResult(
            QStringLiteral("two screens")
        );
        physical.cursorPhysicalPosition = QPoint(100, 100);
        physical.physicalRectangles = QVector<QRect>()
            << QRect(-1000, 400, 400, 40)
            << QRect(100, 200, 300, 40);

        const SelectionSnapshot snapshot = selectionSnapshotFromPhysicalProbe(
            physical,
            QVector<SelectionMonitorGeometry>() << left << right
        );
        QCOMPARE(
            snapshot.rectangles,
            QVector<QRect>() << QRect(-500, 200, 200, 20)
                             << QRect(100, 200, 300, 40)
        );
        QCOMPARE(snapshot.cursorPosition, QPoint(100, 100));
        QCOMPARE(snapshot.anchorRect, QRect(100, 200, 300, 40));
    }

    void displayDeviceNamesNormalizePrefixCaseAndMissingMatchFallback()
    {
        QVERIFY(selectionDisplayDeviceNamesMatch(
            QStringLiteral("\\\\.\\DISPLAY2"),
            QStringLiteral("display2")
        ));
        QVERIFY(!selectionDisplayDeviceNamesMatch(
            QStringLiteral("DISPLAY1"),
            QStringLiteral("DISPLAY2")
        ));
        SelectionMonitorGeometry first;
        first.deviceName = QStringLiteral("\\\\.\\DISPLAY1");
        first.physicalGeometry = QRect(-1000, 0, 1000, 800);
        first.logicalGeometry = QRect(-1000, 0, 1000, 800);
        SelectionMonitorGeometry second;
        second.deviceName = QStringLiteral("display2");
        second.physicalGeometry = QRect(0, 0, 1200, 900);
        second.logicalGeometry = second.physicalGeometry;

        const QVector<SelectionMonitorGeometry> monitors =
            QVector<SelectionMonitorGeometry>() << first << second;
        QCOMPARE(
            selectionMonitorForPhysicalPoint(QPoint(10, 10), monitors)
                .deviceName,
            QStringLiteral("display2")
        );
        QCOMPARE(
            selectionMonitorForPhysicalPoint(QPoint(-50, 10), monitors)
                .deviceName,
            QStringLiteral("\\\\.\\DISPLAY1")
        );
        QCOMPARE(
            selectionMonitorForPhysicalPoint(QPoint(5000, 5000), monitors)
                .deviceName,
            QStringLiteral("display2")
        );
    }

    void uiAutomationProbeNeverBlocksTheOwnerThread()
    {
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [](const SelectionProbeRequest &) {
            QThread::msleep(120);
            return textResult(QStringLiteral("ready"));
        };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle) {
            return true;
        };
        SelectionProbeRunner runner(access);
        QString completedText;
        SelectionProbeRunnerCallbacks callbacks;
        callbacks.completed = [&completedText](quint64,
                                                const SelectionSnapshot &value) {
            completedText = value.text;
        };

        QElapsedTimer timer;
        timer.start();
        runner.start(requestAt(10), false, 1, callbacks);
        QVERIFY2(timer.elapsed() < 50, "start() blocked the owner thread");
        QTRY_COMPARE_WITH_TIMEOUT(completedText, QStringLiteral("ready"), 1000);
    }

    void newestGenerationWinsWithoutParallelWorkers()
    {
        QAtomicInt activeWorkers(0);
        QAtomicInt maximumWorkers(0);
        QAtomicInt calls(0);
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [
            &activeWorkers, &maximumWorkers, &calls
        ](const SelectionProbeRequest &request) {
            const int active = activeWorkers.fetchAndAddOrdered(1) + 1;
            if (active > maximumWorkers.loadAcquire()) {
                maximumWorkers.storeRelease(active);
            }
            calls.ref();
            QThread::msleep(100);
            activeWorkers.deref();
            return textResult(QString::number(request.cursorPhysicalPosition.x()));
        };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle) {
            return true;
        };
        SelectionProbeRunner runner(access);
        QVector<quint64> completed;
        SelectionProbeRunnerCallbacks first;
        first.completed = [&completed](quint64 generation,
                                       const SelectionSnapshot &) {
            completed.append(generation);
        };
        SelectionProbeRunnerCallbacks latest = first;

        runner.start(requestAt(1), false, 1, first);
        runner.start(requestAt(2), false, 2, latest);
        runner.start(requestAt(3), false, 3, latest);

        QTRY_COMPARE_WITH_TIMEOUT(completed, QVector<quint64>() << 3, 1500);
        QCOMPARE(calls.loadAcquire(), 2);
        QCOMPARE(maximumWorkers.loadAcquire(), 1);
    }

    void softTimeoutRequestsComCancellationExactlyOnce()
    {
        QAtomicInt cancelCalls(0);
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [](const SelectionProbeRequest &) {
            QThread::msleep(950);
            return textResult(QStringLiteral("late"));
        };
        access.cancelComCall = [&cancelCalls](quint32) {
            cancelCalls.ref();
        };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle) {
            return true;
        };
        SelectionProbeRunner runner(access);
        QVector<quint64> timedOut;
        int completed = 0;
        SelectionProbeRunnerCallbacks callbacks;
        callbacks.completed = [&completed](quint64,
                                            const SelectionSnapshot &) {
            ++completed;
        };
        callbacks.timedOut = [&timedOut](quint64 generation) {
            timedOut.append(generation);
        };

        runner.start(requestAt(5), false, 5, callbacks);
        QTRY_COMPARE_WITH_TIMEOUT(cancelCalls.loadAcquire(), 1, 1200);
        QCOMPARE(timedOut, QVector<quint64>() << 5);
        QTRY_VERIFY_WITH_TIMEOUT(!runner.isRunning(), 1500);
        QCOMPARE(completed, 0);
        QCOMPARE(cancelCalls.loadAcquire(), 1);
    }

    void strongFallbackRestoresEveryOriginalFormatWhenOwnershipMatches()
    {
        QMimeData *original = new QMimeData;
        original->setText(QStringLiteral("original"));
        original->setHtml(QStringLiteral("<b>original</b>"));
        original->setData(QStringLiteral("application/x-vocekit-test"),
                          QByteArray("binary"));
        QApplication::clipboard()->setMimeData(original);

        quint32 sequence = 1;
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [](const SelectionProbeRequest &) {
            return textResult(QString(), 42);
        };
        access.clipboardSequenceNumber = [&sequence]() { return sequence; };
        access.clipboardOwnerProcessId = []() { return quint32(42); };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle) {
            return true;
        };
        access.sendCopyShortcut = [&sequence]() {
            ++sequence;
            QApplication::clipboard()->setText(QStringLiteral("selected"));
        };

        SelectionProbeRunner runner(access);
        SelectionSnapshot completed;
        SelectionProbeRunnerCallbacks callbacks;
        callbacks.completed = [&completed](quint64,
                                            const SelectionSnapshot &value) {
            completed = value;
        };
        runner.start(requestAt(1), true, 7, callbacks);

        QTRY_COMPARE_WITH_TIMEOUT(completed.text, QStringLiteral("selected"), 1500);
        QCOMPARE(completed.method, SelectionAcquisitionMethod::ClipboardFallback);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("original"));
        QCOMPARE(QApplication::clipboard()->mimeData()->html(),
                 QStringLiteral("<b>original</b>"));
        QCOMPARE(
            QApplication::clipboard()->mimeData()->data(
                QStringLiteral("application/x-vocekit-test")
            ),
            QByteArray("binary")
        );
    }

    void externalClipboardChangeIsNeverOverwrittenByFallbackRestore()
    {
        QApplication::clipboard()->setText(QStringLiteral("original"));
        quint32 sequence = 10;
        int ownerReads = 0;
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [](const SelectionProbeRequest &) {
            return textResult(QString(), 42);
        };
        access.clipboardSequenceNumber = [&sequence]() { return sequence; };
        access.clipboardOwnerProcessId = [&ownerReads, &sequence]() {
            ++ownerReads;
            if (ownerReads >= 2) {
                ++sequence;
                QApplication::clipboard()->setText(QStringLiteral("external"));
                return quint32(99);
            }
            return quint32(42);
        };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle) {
            return true;
        };
        access.sendCopyShortcut = [&sequence]() {
            ++sequence;
            QApplication::clipboard()->setText(QStringLiteral("selected"));
        };

        SelectionProbeRunner runner(access);
        QString result;
        SelectionProbeRunnerCallbacks callbacks;
        callbacks.completed = [&result](quint64,
                                        const SelectionSnapshot &value) {
            result = value.text;
        };
        runner.start(requestAt(1), true, 8, callbacks);

        QTRY_COMPARE_WITH_TIMEOUT(result, QStringLiteral("selected"), 1500);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("external"));
    }

    void validationUsesBackgroundProbeAndNeverActivatesAnotherWindow()
    {
        QAtomicInt probes(0);
        SelectionProbeRunnerAccess access;
        access.probeUiAutomationPhysical = [&probes](const SelectionProbeRequest &) {
            probes.ref();
            return textResult(QStringLiteral("still selected"));
        };
        access.targetStillForeground = [](SelectedTextNativeWindowHandle window) {
            return window == fakeWindow();
        };
        SelectionProbeRunner runner(access);
        bool validated = false;
        runner.validateSelectionAsync(
            fakeWindow(),
            9,
            [&validated](quint64 generation, bool valid) {
                QCOMPARE(generation, quint64(9));
                validated = valid;
            }
        );

        QTRY_VERIFY_WITH_TIMEOUT(validated, 1000);
        QCOMPARE(probes.loadAcquire(), 1);
        QCOMPARE(
            SelectedTextReader::read(
                false,
                reinterpret_cast<SelectedTextNativeWindowHandle>(quintptr(1))
            ),
            QString()
        );
    }
};

QTEST_MAIN(SelectedTextProbeTests)
#include "selected_text_probe_tests.moc"
