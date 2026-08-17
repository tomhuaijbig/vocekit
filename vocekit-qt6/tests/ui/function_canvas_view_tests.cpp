#include "../../src/ui/function_canvas_view.h"
#include "../../src/ui/function_canvas_scene.h"
#include "../../src/ui/function_canvas_visual_style.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QGraphicsItem>
#include <QImage>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRegion>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTextItem>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>
#include <limits>

namespace {

class RecordingPaintEngine : public QPaintEngine
{
public:
    RecordingPaintEngine()
        : QPaintEngine(QPaintEngine::AllFeatures)
    {
    }

    bool begin(QPaintDevice *) override
    {
        setActive(true);
        return true;
    }

    bool end() override
    {
        setActive(false);
        return true;
    }

    Type type() const override
    {
        return QPaintEngine::User;
    }

    void updateState(const QPaintEngineState &state) override
    {
        if (state.state() & QPaintEngine::DirtyPen) {
            currentPen = state.pen();
            ++dirtyPenUpdateCount;
        }
    }

    void drawLines(const QLine *, int lineCount) override
    {
        drawnLineCount += lineCount;
    }

    void drawLines(const QLineF *, int lineCount) override
    {
        drawnLineCount += lineCount;
    }

    void drawPoints(
        const QPoint *points,
        int pointCount) override
    {
        drawnPointCount += pointCount;
        pointPens.append(currentPen);
        for (int index = 0; index < pointCount; ++index) {
            drawnPoints.append(points[index]);
        }
    }

    void drawPoints(
        const QPointF *points,
        int pointCount) override
    {
        drawnPointCount += pointCount;
        pointPens.append(currentPen);
        for (int index = 0; index < pointCount; ++index) {
            drawnPoints.append(points[index]);
        }
    }

    void drawPixmap(
        const QRectF &,
        const QPixmap &,
        const QRectF &) override
    {
    }

    void drawPath(const QPainterPath &) override
    {
    }

    void drawTextItem(
        const QPointF &position,
        const QTextItem &textItem) override
    {
        drawnText += textItem.text();
        drawnTextPositions.append(position);
        drawnTextFonts.append(textItem.font());
    }

    int drawnLineCount = 0;
    int drawnPointCount = 0;
    int dirtyPenUpdateCount = 0;
    QPen currentPen;
    QVector<QPen> pointPens;
    QVector<QPointF> drawnPoints;
    QString drawnText;
    QVector<QPointF> drawnTextPositions;
    QVector<QFont> drawnTextFonts;
};

class RecordingPaintDevice : public QPaintDevice
{
public:
    QPaintEngine *paintEngine() const override
    {
        return const_cast<RecordingPaintEngine *>(&engine);
    }

    RecordingPaintEngine engine;

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return 800;
        case PdmHeight:
            return 600;
        case PdmWidthMM:
            return 212;
        case PdmHeightMM:
            return 159;
        case PdmNumColors:
            return 16777216;
        case PdmDepth:
            return 32;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return 96;
        case PdmDevicePixelRatio:
            return 1;
        case PdmDevicePixelRatioScaled:
            return QPaintDevice::devicePixelRatioFScale();
        }
        return 0;
    }
};

class ViewportPaintRecorder : public QObject
{
public:
    explicit ViewportPaintRecorder(QWidget *viewport)
        : m_viewport(viewport)
    {
        m_viewport->installEventFilter(this);
    }

    ~ViewportPaintRecorder() override
    {
        if (m_viewport) {
            m_viewport->removeEventFilter(this);
        }
    }

    void clear()
    {
        m_paintedRegions.clear();
    }

    bool sawFullViewportPaint() const
    {
        const QRect viewportRect = m_viewport->rect();
        for (const QRegion &region : m_paintedRegions) {
            if (region.contains(viewportRect)
                && region.contains(viewportRect.center())) {
                return true;
            }
        }
        return false;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_viewport
            && event->type() == QEvent::Paint) {
            m_paintedRegions.append(
                static_cast<QPaintEvent *>(event)->region()
            );
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_viewport = nullptr;
    QVector<QRegion> m_paintedRegions;
};

FunctionFlowGraph graphWithOneNode(const QPointF &position)
{
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("input");
    node.type = FunctionFlowNodeType::Input;
    node.position = position;
    graph.nodes.append(node);
    return graph;
}

qreal minimumHorizontalPointSpacing(
    const QVector<QPointF> &points)
{
    QVector<qreal> coordinates;
    coordinates.reserve(points.size());
    for (const QPointF &point : points) {
        coordinates.append(point.x());
    }
    std::sort(coordinates.begin(), coordinates.end());

    qreal spacing = std::numeric_limits<qreal>::max();
    for (int index = 1; index < coordinates.size(); ++index)
    {
        const qreal difference =
            coordinates[index] - coordinates[index - 1];
        if (difference > 0.001) {
            spacing = qMin(spacing, difference);
        }
    }
    return spacing;
}

qreal effectiveFontSize(const QFont &font)
{
    if (font.pointSizeF() > 0.0) {
        return font.pointSizeF();
    }
    return font.pixelSize();
}

} // namespace

class TestableFunctionCanvasView : public FunctionCanvasView
{
  public:
    using FunctionCanvasView::drawBackground;
    using FunctionCanvasView::drawForeground;
    using FunctionCanvasView::mouseMoveEvent;
    using FunctionCanvasView::mousePressEvent;
    using FunctionCanvasView::mouseReleaseEvent;
    using FunctionCanvasView::wheelEvent;
};

class FunctionCanvasViewTests : public QObject
{
    Q_OBJECT

private slots:
    void showsAnEmptyCanvasSurface();
    void canvasOwnsNoVisibleScrollBarsAndPlainWheelDoesNotPan();
    void blankLeftDragPansCanvasWithoutChangingTheGraph();
    void resizingViewportDoesNotMoveExistingContent();
    void zoomRangeIsBoundedAndCanBeReset();
    void restoresViewportWithoutReportingUserChanges();
    void backgroundUsesVisualContractAndAdaptiveDots();
    void backgroundSpacingTracksZoom_data();
    void backgroundSpacingTracksZoom();
    void backgroundSpacingSupportsNegativeCoordinates_data();
    void backgroundSpacingSupportsNegativeCoordinates();
    void emptySceneHintIsViewOnly();
    void emptyHintUsesCompleteVisibleViewport();
    void emptyHintCompensatesFontForZoom();
    void populatedSceneHidesEmptyHint();
    void firstNodeTransitionRepaintsWholeViewport();
    void lastNodeRemovalRepaintsWholeViewport();
    void switchingCanvasScenesIgnoresOldScene();
};

void FunctionCanvasViewTests::showsAnEmptyCanvasSurface()
{
    FunctionCanvasScene scene;
    FunctionCanvasView view;
    view.setCanvasScene(&scene);

    QCOMPARE(view.objectName(), QStringLiteral("functionCanvasSurface"));
    QCOMPARE(view.scene(), static_cast<QGraphicsScene *>(&scene));
    QCOMPARE(view.scene()->items().size(), 0);
    QVERIFY(view.scene()->sceneRect().width() >= 2400.0);
    QVERIFY(view.scene()->sceneRect().height() >= 1600.0);
    QVERIFY(view.minimumHeight() <= 240);
}

void FunctionCanvasViewTests::
canvasOwnsNoVisibleScrollBarsAndPlainWheelDoesNotPan()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(QPointF(900.0, 700.0));
    const QPointF initialCenter = view.viewportCenter();
    const qreal initialScale = view.transform().m11();

    QWheelEvent plainWheel(
        QPointF(120.0, 120.0), QPointF(120.0, 120.0),
        QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false
    );
    view.wheelEvent(&plainWheel);

    QCOMPARE(
        view.horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );
    QCOMPARE(
        view.verticalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );
    QCOMPARE(view.transform().m11(), initialScale);
    QVERIFY(
        QLineF(view.viewportCenter(), initialCenter).length() < 1.0
    );
    QVERIFY(plainWheel.isAccepted());

    QWheelEvent zoomWheel(
        QPointF(120.0, 120.0), QPointF(120.0, 120.0),
        QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
        Qt::NoScrollPhase, false
    );
    QApplication::sendEvent(view.viewport(), &zoomWheel);
    QVERIFY(view.transform().m11() > initialScale);
}

void FunctionCanvasViewTests::
blankLeftDragPansCanvasWithoutChangingTheGraph()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(QPointF(900.0, 700.0));
    const QPointF initialCenter = view.viewportCenter();
    const QString graphHash = functionFlowGraphHash(scene.graph());

    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(400.0, 300.0),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    view.mousePressEvent(&press);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(300.0, 250.0),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    view.mouseMoveEvent(&move);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(300.0, 250.0),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier
    );
    view.mouseReleaseEvent(&release);

    const QPointF movedCenter = view.viewportCenter();
    QVERIFY(movedCenter.x() > initialCenter.x() + 80.0);
    QVERIFY(movedCenter.y() > initialCenter.y() + 30.0);
    QCOMPARE(functionFlowGraphHash(scene.graph()), graphHash);
}

void FunctionCanvasViewTests::
resizingViewportDoesNotMoveExistingContent()
{
    FunctionCanvasScene scene;
    FunctionCanvasView view;
    view.resize(1000, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(QPointF(900.0, 700.0));
    const QPointF contentPoint(820.0, 650.0);
    const QPoint before = view.mapFromScene(contentPoint);

    view.resize(700, 600);
    QCoreApplication::processEvents();

    const QPoint after = view.mapFromScene(contentPoint);
    QVERIFY2(
        QLineF(before, after).length() < 2.0,
        qPrintable(QStringLiteral(
            "content moved from %1,%2 to %3,%4 when the viewport resized"
        ).arg(before.x())
         .arg(before.y())
         .arg(after.x())
         .arg(after.y()))
    );
}

void FunctionCanvasViewTests::zoomRangeIsBoundedAndCanBeReset()
{
    TestableFunctionCanvasView view;

    for (int i = 0; i < 80; ++i)
    {
        QWheelEvent zoomIn(
            QPointF(120.0, 120.0), QPointF(120.0, 120.0),
            QPoint(), QPoint(0, 120), Qt::NoButton,
            Qt::ControlModifier, Qt::NoScrollPhase, false
        );
        view.wheelEvent(&zoomIn);
    }
    QVERIFY(qAbs(view.zoomLevel() - 3.0) < 0.000000001);
    QVERIFY(qAbs(view.transform().m11() - 3.0) < 0.000000001);

    for (int i = 0; i < 160; ++i)
    {
        QWheelEvent zoomOut(
            QPointF(120.0, 120.0), QPointF(120.0, 120.0),
            QPoint(), QPoint(0, -120), Qt::NoButton,
            Qt::ControlModifier, Qt::NoScrollPhase, false
        );
        view.wheelEvent(&zoomOut);
    }
    QVERIFY(qAbs(view.zoomLevel() - 0.35) < 0.000000001);
    QVERIFY(qAbs(view.transform().m11() - 0.35) < 0.000000001);

    view.resetLayout();
    QVERIFY(qAbs(view.transform().m11() - 1.0) < 0.001);
    QCOMPARE(view.zoomLevel(), qreal(1.0));
}

void FunctionCanvasViewTests::
restoresViewportWithoutReportingUserChanges()
{
    FunctionCanvasScene scene;
    FunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QTest::qWait(1);
    QSignalSpy viewportSpy(
        &view,
        &FunctionCanvasView::viewportChanged
    );

    FunctionFlowEditorState editor;
    editor.viewportCenter = QPointF(900.0, 700.0);
    editor.zoom = 1.75;
    view.restoreViewport(editor);

    QCOMPARE(viewportSpy.count(), 0);
    QVERIFY(qAbs(view.zoomLevel() - 1.75) < 0.001);
    QVERIFY(
        QLineF(view.viewportCenter(), editor.viewportCenter)
            .length() < 2.0
    );

    const QString graphHash =
        functionFlowGraphHash(scene.graph());
    QWheelEvent zoomWheel(
        QPointF(120.0, 120.0),
        QPointF(120.0, 120.0),
        QPoint(),
        QPoint(0, 120),
        Qt::NoButton,
        Qt::ControlModifier,
        Qt::NoScrollPhase,
        false
    );
    QApplication::sendEvent(view.viewport(), &zoomWheel);
    QVERIFY(viewportSpy.count() >= 1);
    QCOMPARE(functionFlowGraphHash(scene.graph()), graphHash);
}

void FunctionCanvasViewTests::
backgroundUsesVisualContractAndAdaptiveDots()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.setCanvasScene(&scene);
    const QRectF sampledArea(0.0, 0.0, 720.0, 480.0);

    FunctionFlowEditorState normalViewport;
    normalViewport.zoom = 1.0;
    view.restoreViewport(normalViewport);
    RecordingPaintDevice normalDevice;
    {
        QPainter painter(&normalDevice);
        view.drawBackground(&painter, sampledArea);
    }

    QVERIFY2(
        normalDevice.engine.drawnPointCount > 0,
        "the canvas background must draw a point pattern"
    );
    QCOMPARE(normalDevice.engine.drawnLineCount, 0);
    QVERIFY(normalDevice.engine.dirtyPenUpdateCount >= 2);
    QVERIFY(!normalDevice.engine.pointPens.isEmpty());
    for (const QPen &pen : normalDevice.engine.pointPens) {
        QVERIFY2(
            pen.isCosmetic(),
            "every pen used to draw canvas points must be cosmetic"
        );
    }

    FunctionFlowEditorState distantViewport;
    distantViewport.zoom = 0.35;
    view.restoreViewport(distantViewport);
    RecordingPaintDevice distantDevice;
    {
        QPainter painter(&distantDevice);
        view.drawBackground(&painter, sampledArea);
    }

    QVERIFY2(
        distantDevice.engine.drawnPointCount * 4
            < normalDevice.engine.drawnPointCount,
        "35% zoom must use a much sparser dot pattern than 100%"
    );
    QCOMPARE(distantDevice.engine.drawnLineCount, 0);

    view.restoreViewport(normalViewport);
    QImage surfaceImage(
        200,
        160,
        QImage::Format_ARGB32_Premultiplied
    );
    surfaceImage.fill(QColor(QStringLiteral("#ff00ff")));
    {
        QPainter painter(&surfaceImage);
        view.drawBackground(
            &painter,
            QRectF(0.0, 0.0, 200.0, 160.0)
        );
    }
    QCOMPARE(
        QColor::fromRgba(surfaceImage.pixel(7, 11)),
        functionCanvasSurfaceColor()
    );
    QVERIFY(
        QColor::fromRgba(surfaceImage.pixel(24, 24))
            != functionCanvasSurfaceColor()
    );
    QVERIFY(
        view.styleSheet().contains(
            functionCanvasSurfaceColor().name(),
            Qt::CaseInsensitive
        )
    );
    QVERIFY(
        view.styleSheet().contains(
            functionCanvasPanelBorderColor().name(),
            Qt::CaseInsensitive
        )
    );
}

void FunctionCanvasViewTests::backgroundSpacingTracksZoom_data()
{
    QTest::addColumn<qreal>("zoom");
    QTest::addColumn<qreal>("expectedSpacing");

    QTest::newRow("100 percent") << qreal(1.0) << qreal(24.0);
    QTest::newRow("55 percent") << qreal(0.55) << qreal(24.0);
    QTest::newRow("54 percent") << qreal(0.54) << qreal(120.0);
    QTest::newRow("35 percent") << qreal(0.35) << qreal(120.0);
}

void FunctionCanvasViewTests::backgroundSpacingTracksZoom()
{
    QFETCH(qreal, zoom);
    QFETCH(qreal, expectedSpacing);

    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.setCanvasScene(&scene);
    FunctionFlowEditorState viewport;
    viewport.zoom = zoom;
    view.restoreViewport(viewport);

    RecordingPaintDevice device;
    {
        QPainter painter(&device);
        view.drawBackground(
            &painter,
            QRectF(0.0, 0.0, 720.0, 480.0)
        );
    }

    QVERIFY(device.engine.drawnPointCount > 0);
    QCOMPARE(device.engine.drawnLineCount, 0);
    const qreal actualSpacing =
        minimumHorizontalPointSpacing(device.engine.drawnPoints);
    QVERIFY2(
        qAbs(actualSpacing - expectedSpacing) < 0.001,
        qPrintable(QStringLiteral(
            "zoom %1 expected point spacing %2, got %3"
        ).arg(zoom)
         .arg(expectedSpacing)
         .arg(actualSpacing))
    );
}

void FunctionCanvasViewTests::
backgroundSpacingSupportsNegativeCoordinates_data()
{
    QTest::addColumn<qreal>("zoom");
    QTest::addColumn<qreal>("expectedSpacing");

    QTest::newRow("negative normal")
        << qreal(1.0) << qreal(24.0);
    QTest::newRow("negative distant")
        << qreal(0.54) << qreal(120.0);
}

void FunctionCanvasViewTests::
backgroundSpacingSupportsNegativeCoordinates()
{
    QFETCH(qreal, zoom);
    QFETCH(qreal, expectedSpacing);

    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.setCanvasScene(&scene);
    FunctionFlowEditorState viewport;
    viewport.zoom = zoom;
    view.restoreViewport(viewport);

    RecordingPaintDevice device;
    {
        QPainter painter(&device);
        view.drawBackground(
            &painter,
            QRectF(-720.0, -480.0, 600.0, 360.0)
        );
    }

    const qreal actualSpacing =
        minimumHorizontalPointSpacing(device.engine.drawnPoints);
    QVERIFY2(
        qAbs(actualSpacing - expectedSpacing) < 0.001,
        qPrintable(QStringLiteral(
            "negative rect at zoom %1 expected spacing %2, got %3"
        ).arg(zoom)
         .arg(expectedSpacing)
         .arg(actualSpacing))
    );
}

void FunctionCanvasViewTests::emptySceneHintIsViewOnly()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(QPointF(900.0, 700.0));

    const QList<QGraphicsItem *> itemsBefore = scene.items();
    const QRectF itemsBoundingRectBefore = scene.itemsBoundingRect();
    const QRectF sceneRectBefore = scene.sceneRect();
    const QTransform transformBefore = view.transform();
    const QPointF centerBefore = view.viewportCenter();
    const QPoint dropProbe(217, 163);
    const QPointF dropPositionBefore = view.mapToScene(dropProbe);
    QGraphicsItem *const viewHitBefore = view.itemAt(dropProbe);
    QGraphicsItem *const sceneHitBefore =
        scene.itemAt(dropPositionBefore, view.transform());

    RecordingPaintDevice device;
    {
        QPainter painter(&device);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 720.0, 480.0)
        );
    }

    QVERIFY2(
        device.engine.drawnText.contains(QStringLiteral("从节点库拖入节点")),
        qPrintable(QStringLiteral(
            "missing Chinese empty-canvas hint; painted text was: %1"
        ).arg(device.engine.drawnText))
    );
    QVERIFY(scene.items() == itemsBefore);
    QCOMPARE(scene.itemsBoundingRect(), itemsBoundingRectBefore);
    QCOMPARE(scene.sceneRect(), sceneRectBefore);
    QCOMPARE(view.transform(), transformBefore);
    QCOMPARE(view.viewportCenter(), centerBefore);
    QCOMPARE(view.mapToScene(dropProbe), dropPositionBefore);
    QCOMPARE(view.itemAt(dropProbe), viewHitBefore);
    QCOMPARE(
        scene.itemAt(dropPositionBefore, view.transform()),
        sceneHitBefore
    );
    QVERIFY(itemsBefore.isEmpty());
}

void FunctionCanvasViewTests::emptyHintUsesCompleteVisibleViewport()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(QPointF(900.0, 700.0));

    RecordingPaintDevice firstDevice;
    {
        QPainter painter(&firstDevice);
        view.drawForeground(
            &painter,
            QRectF(700.0, 500.0, 40.0, 30.0)
        );
    }
    RecordingPaintDevice secondDevice;
    {
        QPainter painter(&secondDevice);
        view.drawForeground(
            &painter,
            QRectF(1000.0, 800.0, 20.0, 20.0)
        );
    }

    QVERIFY(!firstDevice.engine.drawnTextPositions.isEmpty());
    QVERIFY(!secondDevice.engine.drawnTextPositions.isEmpty());
    QCOMPARE(
        firstDevice.engine.drawnTextPositions.first(),
        secondDevice.engine.drawnTextPositions.first()
    );
}

void FunctionCanvasViewTests::emptyHintCompensatesFontForZoom()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();

    FunctionFlowEditorState normalViewport;
    normalViewport.zoom = 1.0;
    view.restoreViewport(normalViewport);
    RecordingPaintDevice normalDevice;
    {
        QPainter painter(&normalDevice);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 100.0, 100.0)
        );
    }

    FunctionFlowEditorState distantViewport;
    distantViewport.zoom = 0.5;
    view.restoreViewport(distantViewport);
    RecordingPaintDevice distantDevice;
    {
        QPainter painter(&distantDevice);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 100.0, 100.0)
        );
    }

    QVERIFY(!normalDevice.engine.drawnTextFonts.isEmpty());
    QVERIFY(!distantDevice.engine.drawnTextFonts.isEmpty());
    const qreal normalSize =
        effectiveFontSize(normalDevice.engine.drawnTextFonts.first());
    const qreal distantSize =
        effectiveFontSize(distantDevice.engine.drawnTextFonts.first());
    QVERIFY(normalSize > 0.0);
    QVERIFY2(
        qAbs(distantSize * 0.5 - normalSize) < 0.05,
        qPrintable(QStringLiteral(
            "expected inverse zoom font compensation; normal=%1 distant=%2"
        ).arg(normalSize)
         .arg(distantSize))
    );
}

void FunctionCanvasViewTests::populatedSceneHidesEmptyHint()
{
    FunctionCanvasScene scene;
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("input");
    node.type = FunctionFlowNodeType::Input;
    node.position = QPointF(120.0, 160.0);
    graph.nodes << node;
    scene.setGraph(graph);

    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    view.show();
    QCoreApplication::processEvents();
    view.centerOn(node.position);

    const QList<QGraphicsItem *> itemsBefore = scene.items();
    QVERIFY(!itemsBefore.isEmpty());
    const QRectF itemsBoundingRectBefore = scene.itemsBoundingRect();
    const QRectF sceneRectBefore = scene.sceneRect();
    const QTransform transformBefore = view.transform();
    const QPointF centerBefore = view.viewportCenter();
    const QPoint dropProbe = view.mapFromScene(
        itemsBefore.first()->sceneBoundingRect().center()
    );
    const QPointF dropPositionBefore = view.mapToScene(dropProbe);
    QGraphicsItem *const viewHitBefore = view.itemAt(dropProbe);
    QGraphicsItem *const sceneHitBefore =
        scene.itemAt(dropPositionBefore, view.transform());
    QVERIFY(viewHitBefore != nullptr);
    QVERIFY(sceneHitBefore != nullptr);

    RecordingPaintDevice device;
    {
        QPainter painter(&device);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 720.0, 480.0)
        );
    }

    QVERIFY(device.engine.drawnText.trimmed().isEmpty());
    QVERIFY(scene.items() == itemsBefore);
    QCOMPARE(scene.itemsBoundingRect(), itemsBoundingRectBefore);
    QCOMPARE(scene.sceneRect(), sceneRectBefore);
    QCOMPARE(view.transform(), transformBefore);
    QCOMPARE(view.viewportCenter(), centerBefore);
    QCOMPARE(view.mapToScene(dropProbe), dropPositionBefore);
    QCOMPARE(view.itemAt(dropProbe), viewHitBefore);
    QCOMPARE(
        scene.itemAt(dropPositionBefore, view.transform()),
        sceneHitBefore
    );
}

void FunctionCanvasViewTests::
firstNodeTransitionRepaintsWholeViewport()
{
    FunctionCanvasScene scene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    ViewportPaintRecorder recorder(view.viewport());
    view.show();
    QTest::qWait(30);
    recorder.clear();

    scene.setGraph(graphWithOneNode(QPointF(1800.0, 1200.0)));

    QTRY_VERIFY(recorder.sawFullViewportPaint());
    RecordingPaintDevice foregroundDevice;
    {
        QPainter painter(&foregroundDevice);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 20.0, 20.0)
        );
    }
    QVERIFY(foregroundDevice.engine.drawnText.trimmed().isEmpty());
}

void FunctionCanvasViewTests::
lastNodeRemovalRepaintsWholeViewport()
{
    FunctionCanvasScene scene;
    scene.setGraph(graphWithOneNode(QPointF(1800.0, 1200.0)));
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(&scene);
    ViewportPaintRecorder recorder(view.viewport());
    view.show();
    QTest::qWait(30);
    recorder.clear();

    scene.setGraph(FunctionFlowGraph());

    QTRY_VERIFY(recorder.sawFullViewportPaint());
    RecordingPaintDevice foregroundDevice;
    {
        QPainter painter(&foregroundDevice);
        view.drawForeground(
            &painter,
            QRectF(0.0, 0.0, 20.0, 20.0)
        );
    }
    QVERIFY(
        foregroundDevice.engine.drawnText.contains(
            QStringLiteral("从节点库拖入节点")
        )
    );
}

void FunctionCanvasViewTests::
switchingCanvasScenesIgnoresOldScene()
{
    FunctionCanvasScene *oldScene = new FunctionCanvasScene;
    FunctionCanvasScene *currentScene = new FunctionCanvasScene;
    TestableFunctionCanvasView view;
    view.resize(800, 600);
    view.setCanvasScene(oldScene);
    view.setCanvasScene(currentScene);
    ViewportPaintRecorder recorder(view.viewport());
    view.show();
    QTest::qWait(30);
    recorder.clear();

    oldScene->setGraph(
        graphWithOneNode(QPointF(1800.0, 1200.0))
    );
    QTest::qWait(30);

    QCOMPARE(view.canvasScene(), currentScene);
    QVERIFY(!recorder.sawFullViewportPaint());
    delete oldScene;
    QCOMPARE(view.canvasScene(), currentScene);
    delete currentScene;
    QCOMPARE(view.canvasScene(), static_cast<FunctionCanvasScene *>(nullptr));
}

QTEST_MAIN(FunctionCanvasViewTests)

#include "function_canvas_view_tests.moc"
