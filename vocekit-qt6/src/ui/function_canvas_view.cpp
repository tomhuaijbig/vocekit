#include "function_canvas_view.h"

#include "function_canvas_scene.h"
#include "function_canvas_visual_style.h"

#include <QFrame>
#include <QPainter>
#include <QPen>
#include <QScrollBar>
#include <QSizePolicy>
#include <QVector>
#include <QWheelEvent>
#include <QtMath>

namespace
{

const qreal kMinimumZoom = 0.35;
const qreal kMaximumZoom = 3.0;
const qreal kZoomStep = 1.15;
const qreal kGridSpacing = 24.0;
const qreal kDistantGridSpacing = 120.0;
const qreal kDistantZoomThreshold = 0.55;
const int kMajorPointInterval = 5;

} // namespace

FunctionCanvasView::FunctionCanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    setObjectName(QStringLiteral("functionCanvasSurface"));
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(QStringLiteral(
        "QGraphicsView#functionCanvasSurface {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 6px;"
        "}"
    ).arg(
        functionCanvasSurfaceColor().name(),
        functionCanvasPanelBorderColor().name()
    ));
    connect(
        horizontalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this]() {
            notifyViewportChanged();
        }
    );
    connect(
        verticalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this]() {
            notifyViewportChanged();
        }
    );
}

void FunctionCanvasView::setCanvasScene(
    FunctionCanvasScene *scene)
{
    if (m_canvasScene == scene) {
        return;
    }
    disconnectCanvasSceneSignals();
    m_canvasScene = scene;
    m_canvasSceneEmpty = !scene || scene->nodeCount() == 0;
    setScene(scene);
    if (scene) {
        m_sceneChangedConnection = connect(
            scene,
            &QGraphicsScene::changed,
            this,
            [this, scene](const QList<QRectF> &) {
                if (m_canvasScene != scene) {
                    return;
                }
                const bool empty = scene->nodeCount() == 0;
                if (empty == m_canvasSceneEmpty) {
                    return;
                }
                m_canvasSceneEmpty = empty;
                viewport()->update();
            }
        );
        m_sceneDestroyedConnection = connect(
            scene,
            &QObject::destroyed,
            this,
            [this, scene]() {
                if (m_canvasScene == scene) {
                    m_canvasScene = nullptr;
                    m_canvasSceneEmpty = true;
                    m_sceneChangedConnection =
                        QMetaObject::Connection();
                    m_sceneDestroyedConnection =
                        QMetaObject::Connection();
                }
            }
        );
    }
}

FunctionCanvasScene *FunctionCanvasView::canvasScene() const
{
    return m_canvasScene;
}

void FunctionCanvasView::resetLayout()
{
    m_restoringViewport = true;
    resetTransform();
    m_zoomLevel = 1.0;
    if (horizontalScrollBar())
    {
        horizontalScrollBar()->setValue(0);
    }
    if (verticalScrollBar())
    {
        verticalScrollBar()->setValue(0);
    }
    m_restoringViewport = false;
    notifyViewportChanged();
}

QPointF FunctionCanvasView::viewportCenter() const
{
    return mapToScene(viewport()->rect().center());
}

qreal FunctionCanvasView::zoomLevel() const
{
    return m_zoomLevel;
}

void FunctionCanvasView::restoreViewport(
    const FunctionFlowEditorState &editor)
{
    const FunctionFlowEditorState normalized =
        normalizeFunctionFlowEditorState(editor);
    m_restoringViewport = true;
    resetTransform();
    m_zoomLevel = normalized.zoom;
    scale(m_zoomLevel, m_zoomLevel);
    centerOn(normalized.viewportCenter);
    m_restoringViewport = false;
}

void FunctionCanvasView::wheelEvent(QWheelEvent *event)
{
    if (!event) {
        return;
    }
    if (!(event->modifiers() & Qt::ControlModifier)
        || event->angleDelta().y() == 0) {
        event->accept();
        return;
    }

    const qreal requested =
        event->angleDelta().y() > 0 ? m_zoomLevel * kZoomStep
                                    : m_zoomLevel / kZoomStep;
    const qreal nextZoom = qBound(kMinimumZoom, requested, kMaximumZoom);
    if (!qFuzzyCompare(nextZoom, m_zoomLevel))
    {
        const qreal factor = nextZoom / m_zoomLevel;
        scale(factor, factor);
        m_zoomLevel = nextZoom;
        notifyViewportChanged();
    }
    event->accept();
}

void FunctionCanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    if (!painter) {
        return;
    }

    painter->fillRect(rect, functionCanvasSurfaceColor());

    const qreal spacing =
        m_zoomLevel < kDistantZoomThreshold
            ? kDistantGridSpacing
            : kGridSpacing;
    const int firstColumn =
        static_cast<int>(qCeil(rect.left() / spacing));
    const int lastColumn =
        static_cast<int>(qFloor(rect.right() / spacing));
    const int firstRow =
        static_cast<int>(qCeil(rect.top() / spacing));
    const int lastRow =
        static_cast<int>(qFloor(rect.bottom() / spacing));
    QVector<QPointF> minorPoints;
    QVector<QPointF> majorPoints;

    for (int column = firstColumn; column <= lastColumn; ++column)
    {
        for (int row = firstRow; row <= lastRow; ++row)
        {
            const QPointF point(
                column * spacing,
                row * spacing
            );
            if (column % kMajorPointInterval == 0
                && row % kMajorPointInterval == 0) {
                majorPoints.append(point);
            } else {
                minorPoints.append(point);
            }
        }
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor minorColor = functionCanvasPanelBorderColor();
    minorColor.setAlpha(145);
    QPen minorPen(minorColor, 1.15);
    minorPen.setCosmetic(true);
    minorPen.setCapStyle(Qt::RoundCap);
    painter->setPen(minorPen);
    if (!minorPoints.isEmpty()) {
        painter->drawPoints(
            minorPoints.constData(),
            minorPoints.size()
        );
    }

    QColor majorColor = functionCanvasPanelBorderColor();
    majorColor.setAlpha(215);
    QPen majorPen(majorColor, 2.0);
    majorPen.setCosmetic(true);
    majorPen.setCapStyle(Qt::RoundCap);
    painter->setPen(majorPen);
    if (!majorPoints.isEmpty()) {
        painter->drawPoints(
            majorPoints.constData(),
            majorPoints.size()
        );
    }
    painter->restore();
}

void FunctionCanvasView::drawForeground(
    QPainter *painter,
    const QRectF &)
{
    if (!painter
        || !m_canvasScene
        || m_canvasScene->nodeCount() != 0) {
        return;
    }

    painter->save();
    painter->setPen(QColor(QStringLiteral("#94a3b8")));
    QFont hintFont = font();
    const qreal zoom = qMax(kMinimumZoom, m_zoomLevel);
    if (hintFont.pointSizeF() > 0.0) {
        hintFont.setPointSizeF(hintFont.pointSizeF() / zoom);
    } else if (hintFont.pixelSize() > 0) {
        hintFont.setPixelSize(
            qMax(1, qRound(hintFont.pixelSize() / zoom))
        );
    }
    painter->setFont(hintFont);
    const QRectF visibleSceneRect =
        mapToScene(viewport()->rect()).boundingRect();
    painter->drawText(
        visibleSceneRect,
        Qt::AlignCenter,
        QStringLiteral("从节点库拖入节点，或点击节点开始")
    );
    painter->restore();
}

void FunctionCanvasView::disconnectCanvasSceneSignals()
{
    QObject::disconnect(m_sceneChangedConnection);
    QObject::disconnect(m_sceneDestroyedConnection);
    m_sceneChangedConnection = QMetaObject::Connection();
    m_sceneDestroyedConnection = QMetaObject::Connection();
}

void FunctionCanvasView::notifyViewportChanged()
{
    if (m_restoringViewport) {
        return;
    }
    Q_EMIT viewportChanged(viewportCenter(), m_zoomLevel);
}
