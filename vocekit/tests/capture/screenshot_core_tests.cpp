#include <QtTest>

#include "../../src/capture/screenshot_types.h"

class ScreenshotCoreTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizesTriggerModes();
    void reportsTriggerCapabilities();
    void roundTripsScreenshotHotkeyId();
    void mapsMatchingResultLines();
    void rejectsMismatchedResultLines();
    void normalizesSelectionInsideDesktop();
    void rejectsTinySelection();
    void detectsResizeHandles();
    void movesSelectionInsideDesktop();
    void resizesSelectionWithMinimumSize();
};

void ScreenshotCoreTests::normalizesTriggerModes()
{
    QCOMPARE(
        normalizeScreenshotTriggerMode(QStringLiteral("primary")),
        QStringLiteral("primary")
    );
    QCOMPARE(
        normalizeScreenshotTriggerMode(QStringLiteral("separate")),
        QStringLiteral("separate")
    );
    QCOMPARE(
        normalizeScreenshotTriggerMode(QStringLiteral("launcher")),
        QStringLiteral("launcher")
    );
    QCOMPARE(
        normalizeScreenshotTriggerMode(QStringLiteral("separateAndLauncher")),
        QStringLiteral("separateAndLauncher")
    );
    QCOMPARE(
        normalizeScreenshotTriggerMode(QStringLiteral("unknown")),
        QStringLiteral("separate")
    );
}

void ScreenshotCoreTests::reportsTriggerCapabilities()
{
    QVERIFY(screenshotTriggerUsesPrimary(QStringLiteral("primary")));
    QVERIFY(!screenshotTriggerUsesSeparate(QStringLiteral("primary")));
    QVERIFY(!screenshotTriggerUsesLauncher(QStringLiteral("primary")));

    QVERIFY(!screenshotTriggerUsesPrimary(QStringLiteral("separateAndLauncher")));
    QVERIFY(screenshotTriggerUsesSeparate(QStringLiteral("separateAndLauncher")));
    QVERIFY(screenshotTriggerUsesLauncher(QStringLiteral("separateAndLauncher")));
}

void ScreenshotCoreTests::roundTripsScreenshotHotkeyId()
{
    const QString logicalId = screenshotHotkeyLogicalId(QStringLiteral("custom_7"));
    QCOMPARE(logicalId, QStringLiteral("screenshot:custom_7"));

    QString functionId;
    QVERIFY(parseScreenshotHotkeyLogicalId(logicalId, &functionId));
    QCOMPARE(functionId, QStringLiteral("custom_7"));
    QVERIFY(!parseScreenshotHotkeyLogicalId(QStringLiteral("custom_7"), &functionId));
}

void ScreenshotCoreTests::mapsMatchingResultLines()
{
    const QStringList lines = mapScreenshotResultLines(
        QStringLiteral("第一行\n\n第二行"),
        2
    );
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0), QStringLiteral("第一行"));
    QCOMPARE(lines.at(1), QStringLiteral("第二行"));
}

void ScreenshotCoreTests::rejectsMismatchedResultLines()
{
    QVERIFY(mapScreenshotResultLines(QStringLiteral("只有一行"), 2).isEmpty());
    QVERIFY(mapScreenshotResultLines(QStringLiteral(""), 1).isEmpty());
}

void ScreenshotCoreTests::normalizesSelectionInsideDesktop()
{
    QCOMPARE(
        normalizedScreenshotSelection(
            QPoint(90, 70),
            QPoint(-10, 10),
            QSize(80, 60)
        ),
        QRect(0, 10, 80, 50)
    );
}

void ScreenshotCoreTests::rejectsTinySelection()
{
    QVERIFY(isValidScreenshotSelection(QRect(0, 0, 8, 8)));
    QVERIFY(!isValidScreenshotSelection(QRect(0, 0, 7, 8)));
    QVERIFY(!isValidScreenshotSelection(QRect(0, 0, 8, 7)));
}

void ScreenshotCoreTests::detectsResizeHandles()
{
    const QRect selection(100, 80, 320, 180);
    QCOMPARE(
        screenshotSelectionHandleAt(selection, selection.topLeft(), 8),
        ScreenshotSelectionHandle::TopLeft
    );
    QCOMPARE(
        screenshotSelectionHandleAt(
            selection,
            QPoint(selection.center().x(), selection.top()),
            8
        ),
        ScreenshotSelectionHandle::Top
    );
    QCOMPARE(
        screenshotSelectionHandleAt(selection, selection.bottomRight(), 8),
        ScreenshotSelectionHandle::BottomRight
    );
    QCOMPARE(
        screenshotSelectionHandleAt(selection, selection.center(), 8),
        ScreenshotSelectionHandle::Move
    );
    QCOMPARE(
        screenshotSelectionHandleAt(selection, QPoint(20, 20), 8),
        ScreenshotSelectionHandle::None
    );
}

void ScreenshotCoreTests::movesSelectionInsideDesktop()
{
    QCOMPARE(
        movedScreenshotSelection(
            QRect(100, 80, 300, 180),
            QPoint(-180, -120),
            QSize(800, 600)
        ),
        QRect(0, 0, 300, 180)
    );
    QCOMPARE(
        movedScreenshotSelection(
            QRect(100, 80, 300, 180),
            QPoint(900, 700),
            QSize(800, 600)
        ),
        QRect(500, 420, 300, 180)
    );
}

void ScreenshotCoreTests::resizesSelectionWithMinimumSize()
{
    QCOMPARE(
        resizedScreenshotSelection(
            QRect(100, 80, 300, 180),
            ScreenshotSelectionHandle::BottomRight,
            QPoint(520, 360),
            QSize(800, 600),
            24
        ),
        QRect(100, 80, 421, 281)
    );
    QCOMPARE(
        resizedScreenshotSelection(
            QRect(100, 80, 300, 180),
            ScreenshotSelectionHandle::Left,
            QPoint(395, 120),
            QSize(800, 600),
            24
        ),
        QRect(376, 80, 24, 180)
    );
}

QTEST_MAIN(ScreenshotCoreTests)

#include "screenshot_core_tests.moc"
