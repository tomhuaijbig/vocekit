#include <QtTest>

#include "../../src/ui/function_canvas_edge_item.h"
#include "../../src/ui/function_canvas_node_item.h"
#include "../../src/ui/function_canvas_scene.h"
#include "../../src/ui/function_canvas_visual_style.h"

#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QSignalSpy>
#include <QStyleOptionGraphicsItem>
#include <QTextItem>
#include <QtMath>

namespace {

FunctionFlowNode node(
    const QString &id,
    FunctionFlowNodeType type,
    const QPointF &position)
{
    FunctionFlowNode value;
    value.id = id;
    value.type = type;
    value.position = position;
    return value;
}

FunctionFlowEdge edge(
    const QString &id,
    const QString &fromNodeId,
    const QString &fromPortId,
    const QString &toNodeId,
    const QString &toPortId)
{
    FunctionFlowEdge value;
    value.id = id;
    value.fromNodeId = fromNodeId;
    value.fromPortId = fromPortId;
    value.toNodeId = toNodeId;
    value.toPortId = toPortId;
    return value;
}

FunctionFlowGraph minimalGraph()
{
    FunctionFlowGraph graph;
    graph.nodes
        << node(
            QStringLiteral("voice"),
            FunctionFlowNodeType::VoiceSource,
            QPointF(120.0, 160.0)
        )
        << node(
            QStringLiteral("input"),
            FunctionFlowNodeType::Input,
            QPointF(420.0, 160.0)
        );
    graph.edges << edge(
        QStringLiteral("voice-input"),
        QStringLiteral("voice"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    );
    return graph;
}

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

    void updateState(const QPaintEngineState &state) override
    {
        if (state.state() & QPaintEngine::DirtyPen) {
            m_pen = state.pen();
        }
        if (state.state() & QPaintEngine::DirtyBrush) {
            m_brush = state.brush();
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
        recordColors();
    }

    void drawRects(const QRectF *, int) override
    {
        recordColors();
    }

    void drawEllipse(const QRectF &) override
    {
        recordColors();
    }

    void drawPolygon(
        const QPointF *,
        int,
        PolygonDrawMode) override
    {
        recordColors();
    }

    void drawTextItem(
        const QPointF &,
        const QTextItem &item) override
    {
        texts.append(item.text());
        recordColors();
    }

    Type type() const override
    {
        return QPaintEngine::User;
    }

    QStringList texts;
    QList<QColor> strokeColors;
    QList<qreal> strokeWidths;
    QList<QColor> fillColors;

private:
    void recordColors()
    {
        if (m_pen.style() != Qt::NoPen) {
            strokeColors.append(m_pen.color());
            strokeWidths.append(m_pen.widthF());
        }
        if (m_brush.style() != Qt::NoBrush) {
            fillColors.append(m_brush.color());
        }
    }

    QPen m_pen;
    QBrush m_brush;
};

class RecordingPaintDevice : public QPaintDevice
{
public:
    QPaintEngine *paintEngine() const override
    {
        return &engine;
    }

    mutable RecordingPaintEngine engine;

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return 480;
        case PdmHeight:
            return 320;
        case PdmWidthMM:
            return 127;
        case PdmHeightMM:
            return 85;
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
            return 65536;
        }
        return QPaintDevice::metric(metric);
    }
};

struct NodePaintRecord
{
    QStringList texts;
    QList<QColor> strokeColors;
    QList<qreal> strokeWidths;
    QList<QColor> fillColors;
};

NodePaintRecord paintNode(
    const FunctionFlowNode &node,
    FunctionFlowNodeState state = FunctionFlowNodeState::Pending,
    bool selected = false)
{
    FunctionCanvasNodeItem item(node);
    item.setRuntimeState(state);
    item.setSelected(selected);
    RecordingPaintDevice device;
    QPainter painter(&device);
    QStyleOptionGraphicsItem option;
    if (selected) {
        option.state |= QStyle::State_Selected;
    }
    item.paint(&painter, &option, nullptr);
    painter.end();

    NodePaintRecord record;
    record.texts = device.engine.texts;
    record.strokeColors = device.engine.strokeColors;
    record.strokeWidths = device.engine.strokeWidths;
    record.fillColors = device.engine.fillColors;
    return record;
}

struct EdgePaintRecord
{
    QList<QColor> strokeColors;
    QList<qreal> strokeWidths;
    QList<QColor> fillColors;
};

EdgePaintRecord paintEdge(
    FunctionCanvasEdgeItem *edgeItem,
    QStyle::State states = QStyle::State_None)
{
    RecordingPaintDevice device;
    QPainter painter(&device);
    QStyleOptionGraphicsItem option;
    option.state = states;
    edgeItem->paint(&painter, &option, nullptr);
    painter.end();

    EdgePaintRecord record;
    record.strokeColors = device.engine.strokeColors;
    record.strokeWidths = device.engine.strokeWidths;
    record.fillColors = device.engine.fillColors;
    return record;
}

qreal maximumWidth(const QList<qreal> &widths)
{
    qreal maximum = 0.0;
    for (qreal width : widths) {
        maximum = qMax(maximum, width);
    }
    return maximum;
}

bool containsColor(
    const QList<QColor> &colors,
    const QColor &expected)
{
    for (const QColor &color : colors) {
        if (color == expected) {
            return true;
        }
    }
    return false;
}

class TestableFunctionCanvasNodeItem : public FunctionCanvasNodeItem
{
public:
    explicit TestableFunctionCanvasNodeItem(
        const FunctionFlowNode &node)
        : FunctionCanvasNodeItem(node)
    {
    }

    using FunctionCanvasNodeItem::mousePressEvent;
    using FunctionCanvasNodeItem::mouseReleaseEvent;
};

class TestableFunctionCanvasScene : public FunctionCanvasScene
{
public:
    using FunctionCanvasScene::keyPressEvent;
    using FunctionCanvasScene::mouseMoveEvent;
    using FunctionCanvasScene::mousePressEvent;
    using FunctionCanvasScene::mouseReleaseEvent;
};

} // namespace

class FunctionCanvasSceneTests : public QObject
{
    Q_OBJECT

private slots:
    void startsEmptyAndLoadsDomainPositions();
    void exposesOnlyRegisteredPorts();
    void nodeCardsPaintUnifiedChinesePresentation();
    void longTitlesAreElidedAndInternalIdsStayHidden();
    void nodeVisualStatesKeepSelectionAndFailureIndependent();
    void nodeDragCommitsExactlyOnceAndKeepsPortsHittable();
    void drawsEdgesWithWideHitTargets();
    void edgeGeometryContainsArrowAndHitStroke();
    void edgesUseTypedHoverAndSelectionStyles();
    void temporaryConnectionTracksNoneValidInvalidAndEscape();
    void temporaryConnectionEmitsOnceOnlyForValidRelease();
    void temporaryConnectionClearsWhenSourceDisappearsDuringSync();
    void updatesGraphIncrementallyWithoutLosingSelection();
    void emitsOnlyValidEditingIntents();
    void deleteSelectedEdgeSurvivesSynchronousGraphRefresh();
    void deleteAndBackspaceAreConsumedWithoutAutoRepeatLeaks();
    void isolatesRuntimeOverlayFromTheDomainGraph();
};

void FunctionCanvasSceneTests::startsEmptyAndLoadsDomainPositions()
{
    FunctionCanvasScene scene;
    QCOMPARE(scene.nodeCount(), 0);
    QCOMPARE(scene.edgeCount(), 0);
    QVERIFY(scene.graph().nodes.isEmpty());

    const FunctionFlowGraph graph = minimalGraph();
    scene.setGraph(graph);
    QCOMPARE(scene.nodeCount(), 2);
    QCOMPARE(scene.edgeCount(), 1);
    QVERIFY(scene.nodeItem(QStringLiteral("voice")) != nullptr);
    QCOMPARE(
        scene.nodeItem(QStringLiteral("voice"))->pos(),
        QPointF(120.0, 160.0)
    );
    QCOMPARE(
        functionFlowGraphHash(scene.graph()),
        functionFlowGraphHash(graph)
    );
}

void FunctionCanvasSceneTests::exposesOnlyRegisteredPorts()
{
    FunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    const FunctionCanvasNodeItem *voice =
        scene.nodeItem(QStringLiteral("voice"));
    QVERIFY(voice != nullptr);

    const QVector<FunctionFlowPortSpec> specs =
        functionFlowPortSpecs(FunctionFlowNodeType::VoiceSource);
    QCOMPARE(voice->portIds().size(), specs.size());
    for (const FunctionFlowPortSpec &spec : specs) {
        QVERIFY(voice->hasPort(spec.id));
        const QPointF position =
            voice->portScenePosition(spec.id);
        QVERIFY(qIsFinite(position.x()));
        QVERIFY(qIsFinite(position.y()));
    }
    QVERIFY(!voice->hasPort(QStringLiteral("invented_port")));
    const QPointF missing =
        voice->portScenePosition(QStringLiteral("invented_port"));
    QVERIFY(qIsNaN(missing.x()));
    QVERIFY(qIsNaN(missing.y()));

    FunctionFlowGraph invalid = minimalGraph();
    invalid.edges[0].fromPortId = QStringLiteral("invented_port");
    scene.setGraph(invalid);
    QCOMPARE(scene.nodeCount(), 2);
    QCOMPARE(scene.edgeCount(), 0);
}

void FunctionCanvasSceneTests::
nodeCardsPaintUnifiedChinesePresentation()
{
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };

    for (int index = 0; index < 9; ++index) {
        FunctionFlowNode value = node(
            QStringLiteral("internal_node_%1").arg(index),
            types[index],
            QPointF()
        );
        const NodePaintRecord record = paintNode(value);
        const QString allText =
            record.texts.join(QStringLiteral("|"));
        QVERIFY2(
            record.texts.contains(
                functionCanvasNodeDisplayName(types[index])
            ),
            qPrintable(allText)
        );
        QVERIFY2(
            record.texts.contains(
                functionCanvasNodeGlyph(types[index])
            ),
            qPrintable(allText)
        );
        const QString summary = functionCanvasNodeSummary(value);
        QVERIFY(!summary.trimmed().isEmpty());
        QVERIFY2(
            record.texts.contains(summary),
            qPrintable(allText)
        );
        QVERIFY(containsColor(
            record.fillColors,
            functionCanvasNodeAccent(types[index])
        ));

        for (const FunctionFlowPortSpec &spec :
             functionFlowPortSpecs(types[index])) {
            QVERIFY2(
                record.texts.contains(
                    functionCanvasPortDisplayName(spec.id)
                ),
                qPrintable(allText)
            );
            QVERIFY(containsColor(
                record.fillColors,
                functionCanvasPortColor(spec.id)
            ));
        }

        QVERIFY(!allText.contains(value.id));
        QVERIFY(!allText.contains(QStringLiteral("text_in")));
        QVERIFY(!allText.contains(QStringLiteral("text_out")));
        QVERIFY(!allText.contains(QStringLiteral("action_in")));
        QVERIFY(!allText.contains(QStringLiteral("action_out")));
        QVERIFY(!allText.contains(QStringLiteral("source")));
        QVERIFY(!allText.contains(QStringLiteral("insert")));
        QVERIFY(!allText.contains(QStringLiteral("replace")));
    }
}

void FunctionCanvasSceneTests::
longTitlesAreElidedAndInternalIdsStayHidden()
{
    FunctionFlowNode value = node(
        QStringLiteral("node_private_identifier"),
        FunctionFlowNodeType::Input,
        QPointF()
    );
    value.title = QString::fromUtf8(
        "这是一个为了验证节点卡片标题不会突破固定宽度而准备的特别特别长的自定义标题"
    );
    const NodePaintRecord record = paintNode(value);
    const QString allText =
        record.texts.join(QStringLiteral("|"));
    QVERIFY(!record.texts.contains(value.title));
    QVERIFY2(
        allText.contains(QChar(0x2026)),
        qPrintable(allText)
    );
    QVERIFY(!allText.contains(value.id));
    QVERIFY(!allText.contains(QStringLiteral("text_in")));
    QVERIFY(!allText.contains(QStringLiteral("source")));
}

void FunctionCanvasSceneTests::
nodeVisualStatesKeepSelectionAndFailureIndependent()
{
    FunctionFlowNode value = node(
        QStringLiteral("voice"),
        FunctionFlowNodeType::VoiceSource,
        QPointF()
    );
    const FunctionFlowNodeState states[] = {
        FunctionFlowNodeState::Pending,
        FunctionFlowNodeState::Running,
        FunctionFlowNodeState::Succeeded,
        FunctionFlowNodeState::Failed
    };
    for (int index = 0; index < 4; ++index) {
        const NodePaintRecord record =
            paintNode(value, states[index], false);
        QVERIFY(containsColor(
            record.fillColors,
            functionCanvasRuntimeColor(states[index])
        ));
    }

    const NodePaintRecord selectedFailed =
        paintNode(value, FunctionFlowNodeState::Failed, true);
    QVERIFY(containsColor(
        selectedFailed.strokeColors,
        QColor(QStringLiteral("#2563eb"))
    ));
    QVERIFY(containsColor(
        selectedFailed.fillColors,
        functionCanvasRuntimeColor(FunctionFlowNodeState::Failed)
    ));

    value.enabled = false;
    FunctionCanvasNodeItem disabled(value);
    QCOMPARE(disabled.opacity(), 1.0);
    const NodePaintRecord disabledRecord = paintNode(value);
    QVERIFY(disabledRecord.texts.contains(
        functionCanvasNodeDisplayName(value.type)
    ));
    QVERIFY(disabledRecord.texts.contains(
        functionCanvasNodeSummary(value)
    ));
}

void FunctionCanvasSceneTests::
nodeDragCommitsExactlyOnceAndKeepsPortsHittable()
{
    FunctionFlowNode value = node(
        QStringLiteral("input"),
        FunctionFlowNodeType::Input,
        QPointF(100.0, 120.0)
    );
    TestableFunctionCanvasNodeItem item(value);
    QVERIFY(item.flags() & QGraphicsItem::ItemIsMovable);
    QVERIFY(item.flags() & QGraphicsItem::ItemIsSelectable);
    QVERIFY(item.flags() & QGraphicsItem::ItemIsFocusable);
    QVERIFY(item.flags() & QGraphicsItem::ItemSendsGeometryChanges);

    QSignalSpy committed(
        &item,
        &FunctionCanvasNodeItem::positionCommitted
    );
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    item.mousePressEvent(&press);
    item.setPos(QPointF(180.0, 210.0));

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setButton(Qt::LeftButton);
    release.setButtons(Qt::NoButton);
    item.mouseReleaseEvent(&release);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(
        committed.first().at(0).toString(),
        QStringLiteral("input")
    );
    QCOMPARE(
        committed.first().at(1).toPointF(),
        QPointF(180.0, 210.0)
    );

    const QPointF inputPort =
        item.portScenePosition(QStringLiteral("text_in"));
    QCOMPARE(
        item.portAt(
            inputPort + QPointF(11.9, 0.0),
            FunctionFlowPortDirection::Input
        ),
        QStringLiteral("text_in")
    );
}

void FunctionCanvasSceneTests::drawsEdgesWithWideHitTargets()
{
    FunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    FunctionCanvasEdgeItem *item =
        scene.edgeItem(QStringLiteral("voice-input"));
    QVERIFY(item != nullptr);
    QVERIFY(!item->path().isEmpty());
    QVERIFY(
        item->shape().boundingRect().height()
            > item->path().boundingRect().height()
    );

    const QPainterPath before = item->path();
    scene.nodeItem(QStringLiteral("voice"))->setPos(160.0, 240.0);
    QVERIFY(item->path() != before);
}

void FunctionCanvasSceneTests::
edgeGeometryContainsArrowAndHitStroke()
{
    FunctionCanvasScene scene;
    FunctionFlowGraph graph;
    graph.nodes
        << node(
            QStringLiteral("voice"),
            FunctionFlowNodeType::VoiceSource,
            QPointF(80.0, 40.0)
        )
        << node(
            QStringLiteral("input"),
            FunctionFlowNodeType::Input,
            QPointF(520.0, 1500.0)
        );
    graph.edges.append(edge(
        QStringLiteral("voice-input"),
        QStringLiteral("voice"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    ));
    scene.setGraph(graph);
    FunctionCanvasEdgeItem *item =
        scene.edgeItem(QStringLiteral("voice-input"));
    QVERIFY(item);
    QVERIFY(!item->path().isEmpty());

    const QPointF end = item->path().pointAtPercent(1.0);
    const QPointF before = item->path().pointAtPercent(0.94);
    const qreal angle = qAtan2(
        end.y() - before.y(),
        end.x() - before.x()
    );
    const qreal arrowSize = 10.0;
    const QPointF firstCorner = end - QPointF(
        qCos(angle - M_PI / 6.0) * arrowSize,
        qSin(angle - M_PI / 6.0) * arrowSize
    );
    const QPointF secondCorner = end - QPointF(
        qCos(angle + M_PI / 6.0) * arrowSize,
        qSin(angle + M_PI / 6.0) * arrowSize
    );
    QPainterPath expectedArrow;
    expectedArrow.moveTo(end);
    expectedArrow.lineTo(firstCorner);
    expectedArrow.lineTo(secondCorner);
    expectedArrow.closeSubpath();
    const QPainterPath arrow = item->arrowHeadPath();
    QVERIFY(!arrow.isEmpty());
    QCOMPARE(
        arrow.boundingRect(),
        expectedArrow.boundingRect()
    );

    QVERIFY(item->boundingRect().contains(
        item->shape().boundingRect()
    ));
    QVERIFY(item->boundingRect().contains(
        arrow.boundingRect()
    ));

    const QPointF centroid =
        (end + firstCorner + secondCorner) / 3.0;
    const QPointF arrowSamples[] = {
        centroid,
        centroid * 0.2 + end * 0.8,
        centroid * 0.2 + firstCorner * 0.8,
        centroid * 0.2 + secondCorner * 0.8
    };
    for (const QPointF &sample : arrowSamples) {
        QVERIFY2(
            item->shape().contains(sample),
            qPrintable(QStringLiteral(
                "arrow sample outside edge shape at %1,%2"
            ).arg(sample.x()).arg(sample.y()))
        );
    }
}

void FunctionCanvasSceneTests::
edgesUseTypedHoverAndSelectionStyles()
{
    FunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    FunctionCanvasEdgeItem *item =
        scene.edgeItem(QStringLiteral("voice-input"));
    QVERIFY(item);
    QVERIFY(item->acceptHoverEvents());

    const EdgePaintRecord normal = paintEdge(item);
    QVERIFY(containsColor(
        normal.strokeColors,
        functionCanvasPortColor(QStringLiteral("text_out"))
    ));

    const EdgePaintRecord hovered = paintEdge(
        item,
        QStyle::State_MouseOver
    );
    QVERIFY(maximumWidth(hovered.strokeWidths)
        > maximumWidth(normal.strokeWidths));

    item->setSelected(true);
    const EdgePaintRecord selected = paintEdge(
        item,
        QStyle::State_Selected
    );
    QVERIFY(containsColor(
        selected.strokeColors,
        QColor(QStringLiteral("#2563eb"))
    ));
    QVERIFY(maximumWidth(selected.strokeWidths)
        > maximumWidth(normal.strokeWidths));
}

void FunctionCanvasSceneTests::
temporaryConnectionTracksNoneValidInvalidAndEscape()
{
    TestableFunctionCanvasScene scene;
    FunctionFlowGraph graph;
    graph.nodes
        << node(
            QStringLiteral("voice"),
            FunctionFlowNodeType::VoiceSource,
            QPointF(80.0, 100.0)
        )
        << node(
            QStringLiteral("input"),
            FunctionFlowNodeType::Input,
            QPointF(480.0, 100.0)
        )
        << node(
            QStringLiteral("model"),
            FunctionFlowNodeType::Model,
            QPointF(480.0, 340.0)
        );
    scene.setGraph(graph);
    const QString originalHash = scene.graphHash();
    const QPointF start = scene.nodeItem(
        QStringLiteral("voice")
    )->portScenePosition(QStringLiteral("text_out"));
    const QPointF valid = scene.nodeItem(
        QStringLiteral("input")
    )->portScenePosition(QStringLiteral("text_in"));
    const QPointF invalidPort = scene.nodeItem(
        QStringLiteral("model")
    )->portScenePosition(QStringLiteral("text_in"));
    const QPointF invalidBody = scene.nodeItem(
        QStringLiteral("model")
    )->mapToScene(QPointF(110.0, 45.0));
    const QPointF empty(360.0, 700.0);

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setScenePos(start);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    scene.mousePressEvent(&press);
    QVERIFY(press.isAccepted());
    QVERIFY(scene.hasTemporaryConnection());
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::None
    );
    QVERIFY(!scene.temporaryConnectionPath().isEmpty());

    QGraphicsSceneMouseEvent moveEmpty(QEvent::GraphicsSceneMouseMove);
    moveEmpty.setScenePos(empty);
    moveEmpty.setButtons(Qt::LeftButton);
    scene.mouseMoveEvent(&moveEmpty);
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::None
    );

    QGraphicsSceneMouseEvent moveValid(QEvent::GraphicsSceneMouseMove);
    moveValid.setScenePos(valid);
    moveValid.setButtons(Qt::LeftButton);
    scene.mouseMoveEvent(&moveValid);
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::Valid
    );
    QCOMPARE(
        scene.temporaryConnectionColor(),
        QColor(QStringLiteral("#16a34a"))
    );

    QGraphicsSceneMouseEvent moveInvalidPort(
        QEvent::GraphicsSceneMouseMove
    );
    moveInvalidPort.setScenePos(invalidPort);
    moveInvalidPort.setButtons(Qt::LeftButton);
    scene.mouseMoveEvent(&moveInvalidPort);
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::Invalid
    );
    QCOMPARE(
        scene.temporaryConnectionColor(),
        QColor(QStringLiteral("#dc2626"))
    );

    QGraphicsSceneMouseEvent moveInvalidBody(
        QEvent::GraphicsSceneMouseMove
    );
    moveInvalidBody.setScenePos(invalidBody);
    moveInvalidBody.setButtons(Qt::LeftButton);
    scene.mouseMoveEvent(&moveInvalidBody);
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::Invalid
    );

    QKeyEvent escape(
        QEvent::KeyPress,
        Qt::Key_Escape,
        Qt::NoModifier
    );
    scene.keyPressEvent(&escape);
    QVERIFY(escape.isAccepted());
    QVERIFY(!scene.hasTemporaryConnection());
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::None
    );
    QCOMPARE(scene.graphHash(), originalHash);
}

void FunctionCanvasSceneTests::
temporaryConnectionEmitsOnceOnlyForValidRelease()
{
    TestableFunctionCanvasScene scene;
    FunctionFlowGraph graph;
    graph.nodes
        << node(
            QStringLiteral("voice"),
            FunctionFlowNodeType::VoiceSource,
            QPointF(80.0, 100.0)
        )
        << node(
            QStringLiteral("input"),
            FunctionFlowNodeType::Input,
            QPointF(480.0, 100.0)
        )
        << node(
            QStringLiteral("model"),
            FunctionFlowNodeType::Model,
            QPointF(480.0, 340.0)
        );
    scene.setGraph(graph);
    int requested = 0;
    connect(
        &scene,
        &FunctionCanvasScene::connectionRequested,
        &scene,
        [&requested](
            const FunctionFlowEndpoint &,
            const FunctionFlowEndpoint &) {
            ++requested;
        }
    );
    const QPointF start = scene.nodeItem(
        QStringLiteral("voice")
    )->portScenePosition(QStringLiteral("text_out"));
    const QPointF valid = scene.nodeItem(
        QStringLiteral("input")
    )->portScenePosition(QStringLiteral("text_in"));
    const QPointF invalid = scene.nodeItem(
        QStringLiteral("model")
    )->portScenePosition(QStringLiteral("text_in"));

    auto beginAndMove = [&scene, start](const QPointF &target) {
        QGraphicsSceneMouseEvent press(
            QEvent::GraphicsSceneMousePress
        );
        press.setScenePos(start);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        scene.mousePressEvent(&press);
        QGraphicsSceneMouseEvent move(
            QEvent::GraphicsSceneMouseMove
        );
        move.setScenePos(target);
        move.setButtons(Qt::LeftButton);
        scene.mouseMoveEvent(&move);
    };
    auto releaseAt = [&scene](const QPointF &target) {
        QGraphicsSceneMouseEvent release(
            QEvent::GraphicsSceneMouseRelease
        );
        release.setScenePos(target);
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        scene.mouseReleaseEvent(&release);
        QVERIFY(release.isAccepted());
    };

    beginAndMove(valid);
    releaseAt(valid);
    QCOMPARE(requested, 1);
    QVERIFY(!scene.hasTemporaryConnection());

    beginAndMove(invalid);
    releaseAt(invalid);
    QCOMPARE(requested, 1);

    beginAndMove(QPointF(360.0, 700.0));
    releaseAt(QPointF(360.0, 700.0));
    QCOMPARE(requested, 1);
}

void FunctionCanvasSceneTests::
temporaryConnectionClearsWhenSourceDisappearsDuringSync()
{
    TestableFunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    int requested = 0;
    connect(
        &scene,
        &FunctionCanvasScene::connectionRequested,
        &scene,
        [&requested](
            const FunctionFlowEndpoint &,
            const FunctionFlowEndpoint &) {
            ++requested;
        }
    );

    const QPointF start = scene.nodeItem(
        QStringLiteral("voice")
    )->portScenePosition(QStringLiteral("text_out"));
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setScenePos(start);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    scene.mousePressEvent(&press);
    QVERIFY(scene.hasTemporaryConnection());

    FunctionFlowGraph withoutSource;
    withoutSource.nodes.append(node(
        QStringLiteral("input"),
        FunctionFlowNodeType::Input,
        QPointF(420.0, 160.0)
    ));
    scene.setGraph(withoutSource);

    QVERIFY(!scene.nodeItem(QStringLiteral("voice")));
    QVERIFY(!scene.hasTemporaryConnection());
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::None
    );
    QVERIFY(scene.temporaryConnectionPath().isEmpty());
    QVERIFY(!scene.temporaryConnectionColor().isValid());

    const QPointF remainingInput = scene.nodeItem(
        QStringLiteral("input")
    )->portScenePosition(QStringLiteral("text_in"));
    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setScenePos(remainingInput);
    move.setButtons(Qt::LeftButton);
    scene.mouseMoveEvent(&move);

    QGraphicsSceneMouseEvent release(
        QEvent::GraphicsSceneMouseRelease
    );
    release.setScenePos(remainingInput);
    release.setButton(Qt::LeftButton);
    release.setButtons(Qt::NoButton);
    scene.mouseReleaseEvent(&release);

    QKeyEvent escape(
        QEvent::KeyPress,
        Qt::Key_Escape,
        Qt::NoModifier
    );
    scene.keyPressEvent(&escape);
    QCOMPARE(requested, 0);
    QVERIFY(!scene.hasTemporaryConnection());
    QCOMPARE(
        scene.connectionTargetState(),
        FunctionCanvasConnectionTargetState::None
    );
}

void FunctionCanvasSceneTests::
updatesGraphIncrementallyWithoutLosingSelection()
{
    FunctionCanvasScene scene;
    FunctionFlowGraph graph = minimalGraph();
    scene.setGraph(graph);
    FunctionCanvasNodeItem *voice =
        scene.nodeItem(QStringLiteral("voice"));
    FunctionCanvasEdgeItem *connection =
        scene.edgeItem(QStringLiteral("voice-input"));
    voice->setSelected(true);

    graph.nodes[0].title = QStringLiteral("Microphone");
    graph.nodes[0].position = QPointF(220.0, 260.0);
    scene.setGraph(graph);

    QCOMPARE(scene.nodeItem(QStringLiteral("voice")), voice);
    QCOMPARE(
        scene.edgeItem(QStringLiteral("voice-input")),
        connection
    );
    QVERIFY(voice->isSelected());
    QCOMPARE(voice->pos(), QPointF(220.0, 260.0));
    QCOMPARE(voice->title(), QStringLiteral("Microphone"));
}

void FunctionCanvasSceneTests::emitsOnlyValidEditingIntents()
{
    FunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    int connections = 0;
    FunctionFlowEndpoint observedFrom;
    FunctionFlowEndpoint observedTo;
    connect(
        &scene,
        &FunctionCanvasScene::connectionRequested,
        &scene,
        [&connections, &observedFrom, &observedTo](
            FunctionFlowEndpoint from,
            FunctionFlowEndpoint to) {
            ++connections;
            observedFrom = from;
            observedTo = to;
        }
    );

    FunctionFlowEndpoint from;
    from.nodeId = QStringLiteral("voice");
    from.portId = QStringLiteral("text_out");
    FunctionFlowEndpoint to;
    to.nodeId = QStringLiteral("input");
    to.portId = QStringLiteral("text_in");
    QVERIFY(scene.requestConnection(from, to));
    QCOMPARE(connections, 1);
    QCOMPARE(observedFrom.nodeId, QStringLiteral("voice"));
    QCOMPARE(observedTo.nodeId, QStringLiteral("input"));
    QCOMPARE(scene.edgeCount(), 1);

    from.portId = QStringLiteral("invented");
    QVERIFY(!scene.requestConnection(from, to));
    QCOMPARE(connections, 1);

    int placements = 0;
    connect(
        &scene,
        &FunctionCanvasScene::nodePlacementRequested,
        &scene,
        [&placements](FunctionFlowNodeType, QPointF) {
            ++placements;
        }
    );
    scene.requestNodePlacement(
        FunctionFlowNodeType::Model,
        QPointF(700.0, 400.0)
    );
    QCOMPARE(placements, 1);
    QCOMPARE(scene.nodeCount(), 2);
}

void FunctionCanvasSceneTests::
deleteSelectedEdgeSurvivesSynchronousGraphRefresh()
{
    TestableFunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    FunctionCanvasEdgeItem *selected =
        scene.edgeItem(QStringLiteral("voice-input"));
    QVERIFY(selected != nullptr);
    selected->setSelected(true);

    int removalRequests = 0;
    connect(
        &scene,
        &FunctionCanvasScene::edgeRemovalRequested,
        &scene,
        [&scene, &removalRequests](const QString &edgeId) {
            ++removalRequests;
            FunctionFlowGraph graph = scene.graph();
            for (int index = graph.edges.size() - 1;
                 index >= 0;
                 --index) {
                if (graph.edges.at(index).id == edgeId) {
                    graph.edges.remove(index);
                }
            }
            scene.setGraph(graph);
        }
    );

    QKeyEvent event(
        QEvent::KeyPress,
        Qt::Key_Delete,
        Qt::NoModifier
    );
    scene.keyPressEvent(&event);

    QCOMPARE(removalRequests, 1);
    QCOMPARE(scene.edgeCount(), 0);
    QVERIFY(event.isAccepted());
}

void FunctionCanvasSceneTests::
deleteAndBackspaceAreConsumedWithoutAutoRepeatLeaks()
{
    TestableFunctionCanvasScene scene;
    scene.setGraph(minimalGraph());
    QGraphicsView view(&scene);
    view.resize(640, 480);
    view.show();
    view.activateWindow();
    view.setFocus(Qt::OtherFocusReason);
    scene.setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(scene.hasFocus());

    int edgeRemovals = 0;
    int nodeRemovals = 0;
    connect(
        &scene,
        &FunctionCanvasScene::edgeRemovalRequested,
        &scene,
        [&edgeRemovals](const QString &) {
            ++edgeRemovals;
        }
    );
    connect(
        &scene,
        &FunctionCanvasScene::nodeRemovalRequested,
        &scene,
        [&nodeRemovals](const QString &) {
            ++nodeRemovals;
        }
    );

    QKeyEvent emptyDelete(
        QEvent::KeyPress,
        Qt::Key_Delete,
        Qt::NoModifier
    );
    emptyDelete.setAccepted(false);
    scene.keyPressEvent(&emptyDelete);
    QVERIFY(emptyDelete.isAccepted());
    QCOMPARE(edgeRemovals, 0);
    QCOMPARE(nodeRemovals, 0);

    FunctionCanvasEdgeItem *selectedEdge =
        scene.edgeItem(QStringLiteral("voice-input"));
    QVERIFY(selectedEdge);
    selectedEdge->setSelected(true);
    QKeyEvent deletePress(
        QEvent::KeyPress,
        Qt::Key_Delete,
        Qt::NoModifier
    );
    deletePress.setAccepted(false);
    scene.keyPressEvent(&deletePress);
    QVERIFY(deletePress.isAccepted());
    QCOMPARE(edgeRemovals, 1);

    QKeyEvent repeatedDelete(
        QEvent::KeyPress,
        Qt::Key_Delete,
        Qt::NoModifier,
        QString(),
        true,
        1
    );
    repeatedDelete.setAccepted(false);
    scene.keyPressEvent(&repeatedDelete);
    QVERIFY(repeatedDelete.isAccepted());
    QCOMPARE(edgeRemovals, 1);

    scene.clearSelection();
    FunctionCanvasNodeItem *selectedNode =
        scene.nodeItem(QStringLiteral("voice"));
    QVERIFY(selectedNode);
    selectedNode->setSelected(true);
    QKeyEvent backspacePress(
        QEvent::KeyPress,
        Qt::Key_Backspace,
        Qt::NoModifier
    );
    backspacePress.setAccepted(false);
    scene.keyPressEvent(&backspacePress);
    QVERIFY(backspacePress.isAccepted());
    QCOMPARE(nodeRemovals, 1);

    QKeyEvent repeatedBackspace(
        QEvent::KeyPress,
        Qt::Key_Backspace,
        Qt::NoModifier,
        QString(),
        true,
        1
    );
    repeatedBackspace.setAccepted(false);
    scene.keyPressEvent(&repeatedBackspace);
    QVERIFY(repeatedBackspace.isAccepted());
    QCOMPARE(nodeRemovals, 1);

    scene.clearSelection();
    QKeyEvent emptyBackspace(
        QEvent::KeyPress,
        Qt::Key_Backspace,
        Qt::NoModifier
    );
    emptyBackspace.setAccepted(false);
    scene.keyPressEvent(&emptyBackspace);
    QVERIFY(emptyBackspace.isAccepted());
    QCOMPARE(edgeRemovals, 1);
    QCOMPARE(nodeRemovals, 1);
}

void FunctionCanvasSceneTests::
isolatesRuntimeOverlayFromTheDomainGraph()
{
    FunctionCanvasScene scene;
    const FunctionFlowGraph graph = minimalGraph();
    scene.setGraph(graph);
    const QString graphHash = functionFlowGraphHash(scene.graph());
    FunctionCanvasNodeItem *voice =
        scene.nodeItem(QStringLiteral("voice"));
    QCOMPARE(
        voice->runtimeState(),
        FunctionFlowNodeState::Pending
    );

    FunctionFlowNodeExecutionEvent event;
    event.functionId = QStringLiteral("custom_1");
    event.publishedHash = QString(64, QLatin1Char('0'));
    event.nodeId = QStringLiteral("voice");
    event.nodeType = FunctionFlowNodeType::VoiceSource;
    event.state = FunctionFlowNodeState::Running;
    QVERIFY(!scene.applyRuntimeEvent(event));
    QCOMPARE(
        voice->runtimeState(),
        FunctionFlowNodeState::Pending
    );

    event.publishedHash = graphHash;
    event.nodeId = QStringLiteral("missing");
    QVERIFY(!scene.applyRuntimeEvent(event));
    event.nodeId = QStringLiteral("voice");
    event.nodeType = FunctionFlowNodeType::Model;
    QVERIFY(!scene.applyRuntimeEvent(event));

    event.nodeType = FunctionFlowNodeType::VoiceSource;
    QVERIFY(scene.applyRuntimeEvent(event));
    QCOMPARE(
        voice->runtimeState(),
        FunctionFlowNodeState::Running
    );
    QCOMPARE(functionFlowGraphHash(scene.graph()), graphHash);
    QCOMPARE(scene.graph().nodes.size(), graph.nodes.size());
    QCOMPARE(scene.graph().edges.size(), graph.edges.size());
}

QTEST_MAIN(FunctionCanvasSceneTests)

#include "function_canvas_scene_tests.moc"
