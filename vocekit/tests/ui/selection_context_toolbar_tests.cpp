#include <QtTest>

#include "../../src/ui/selection_context_placement.h"

class SelectionContextToolbarTests : public QObject
{
    Q_OBJECT

private slots:
    void preferredBelowAndAbovePositionsAreDeterministic()
    {
        const QRect screen(0, 0, 1200, 900);
        const QSize toolbar(560, 48);
        SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(400, 100, 120, 24),
            QPoint(450, 112),
            toolbar,
            QSize(560, 240),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(180, 132));
        QVERIFY(!placed.toolbarAbove);

        placed = placeSelectionSurfaces(
            QRect(400, 850, 120, 24),
            QPoint(450, 860),
            toolbar,
            QSize(560, 240),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(180, 794));
        QVERIFY(placed.toolbarAbove);
    }

    void invalidAnchorFallsBackToCursorWithoutCoveringHotspot()
    {
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(),
            QPoint(300, 240),
            QSize(400, 48),
            QSize(400, 220),
            QRect(0, 0, 1000, 700),
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(308, 248));
        QVERIFY(!placed.toolbarAbove);
    }

    void placementStaysInsideAvailableGeometry_data()
    {
        QTest::addColumn<QRect>("screen");
        QTest::addColumn<QRect>("anchor");
        QTest::addColumn<QPoint>("cursor");
        QTest::addColumn<QSize>("toolbar");
        QTest::newRow("right-edge")
            << QRect(0, 0, 1920, 1040)
            << QRect(1870, 300, 45, 22)
            << QPoint(1900, 315)
            << QSize(560, 48);
        QTest::newRow("left-edge")
            << QRect(0, 0, 1920, 1040)
            << QRect(2, 300, 45, 22)
            << QPoint(5, 315)
            << QSize(560, 48);
        QTest::newRow("taskbar-bottom")
            << QRect(0, 0, 1920, 1040)
            << QRect(800, 1014, 140, 22)
            << QPoint(900, 1025)
            << QSize(560, 48);
        QTest::newRow("negative-monitor")
            << QRect(-1920, 0, 1920, 1040)
            << QRect(-80, 980, 70, 20)
            << QPoint(-20, 990)
            << QSize(560, 48);
        QTest::newRow("two-hundred-percent-logical")
            << QRect(-960, -40, 960, 520)
            << QRect(-140, 400, 80, 18)
            << QPoint(-90, 410)
            << QSize(560, 48);
    }

    void placementStaysInsideAvailableGeometry()
    {
        QFETCH(QRect, screen);
        QFETCH(QRect, anchor);
        QFETCH(QPoint, cursor);
        QFETCH(QSize, toolbar);
        const QSize card(
            qMin(toolbar.width(), screen.width()),
            qMin(320, screen.height())
        );
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            anchor,
            cursor,
            toolbar,
            card,
            screen,
            8
        );
        QVERIFY(screen.contains(QRect(placed.toolbarTopLeft, toolbar)));
        QVERIFY(screen.contains(QRect(placed.cardTopLeft, card)));
    }

    void cardIsCenteredBelowToolbarWhenItFits()
    {
        const QSize toolbar(400, 48);
        const QSize card(560, 220);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(500, 80, 100, 20),
            QPoint(550, 90),
            toolbar,
            card,
            QRect(0, 0, 1400, 900),
            8
        );
        QCOMPARE(
            placed.cardTopLeft.x(),
            placed.toolbarTopLeft.x()
                + (toolbar.width() - card.width()) / 2
        );
        QCOMPARE(
            placed.cardTopLeft.y(),
            placed.toolbarTopLeft.y() + toolbar.height() + 8
        );
        QVERIFY(!placed.cardAbove);
    }

    void cardMovesAboveBothSurfacesWhenBelowWouldOverflow()
    {
        const QSize toolbar(400, 48);
        const QSize card(560, 300);
        const QRect screen(0, 0, 1200, 700);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(500, 620, 100, 20),
            QPoint(550, 630),
            toolbar,
            card,
            screen,
            8
        );
        QVERIFY(placed.cardAbove);
        QVERIFY(
            placed.cardTopLeft.y() + card.height()
                <= placed.toolbarTopLeft.y() - 8
        );
        QVERIFY(screen.contains(QRect(placed.cardTopLeft, card)));
    }

    void oversizedSurfacePinsToAvailableGeometryOrigin()
    {
        const QRect screen(-500, 20, 300, 180);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(-400, 80, 20, 20),
            QPoint(-390, 90),
            QSize(600, 240),
            QSize(700, 300),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, screen.topLeft());
        QCOMPARE(placed.cardTopLeft, screen.topLeft());
    }
};

QTEST_APPLESS_MAIN(SelectionContextToolbarTests)
#include "selection_context_toolbar_tests.moc"
