#include "function_canvas_edge_item.h"

#include "function_canvas_node_item.h"
#include "function_canvas_visual_style.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace {

const qreal kEdgeHitWidth = 14.0;
const qreal kArrowSize = 10.0;

} // namespace

FunctionCanvasEdgeItem::FunctionCanvasEdgeItem(
    const FunctionFlowEdge &edge,
    FunctionCanvasNodeItem *fromNode,
    FunctionCanvasNodeItem *toNode,
    QGraphicsItem *parent)
    : QGraphicsPathItem(parent),
      m_edge(edge),
      m_fromNode(fromNode),
      m_toNode(toNode)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setAcceptHoverEvents(true);
    setZValue(-1.0);
    updatePath();
}

QString FunctionCanvasEdgeItem::edgeId() const
{
    return m_edge.id;
}

FunctionFlowEdge FunctionCanvasEdgeItem::edge() const
{
    return m_edge;
}

void FunctionCanvasEdgeItem::setEdge(
    const FunctionFlowEdge &edge,
    FunctionCanvasNodeItem *fromNode,
    FunctionCanvasNodeItem *toNode)
{
    m_edge = edge;
    m_fromNode = fromNode;
    m_toNode = toNode;
    updatePath();
}

void FunctionCanvasEdgeItem::updatePath()
{
    if (!m_fromNode || !m_toNode) {
        setPath(QPainterPath());
        return;
    }
    const QPointF start =
        m_fromNode->portScenePosition(m_edge.fromPortId);
    const QPointF end =
        m_toNode->portScenePosition(m_edge.toPortId);
    if (!qIsFinite(start.x()) || !qIsFinite(start.y())
        || !qIsFinite(end.x()) || !qIsFinite(end.y())) {
        setPath(QPainterPath());
        return;
    }

    const qreal controlDistance = qMax(
        80.0,
        qAbs(end.x() - start.x()) * 0.45
    );
    QPainterPath curve(start);
    curve.cubicTo(
        QPointF(start.x() + controlDistance, start.y()),
        QPointF(end.x() - controlDistance, end.y()),
        end
    );
    setPath(curve);
}

QRectF FunctionCanvasEdgeItem::boundingRect() const
{
    return shape().boundingRect();
}

QPainterPath FunctionCanvasEdgeItem::arrowHeadPath() const
{
    if (path().isEmpty()) {
        return QPainterPath();
    }

    const QPointF end = path().pointAtPercent(1.0);
    const QPointF before = path().pointAtPercent(0.94);
    const qreal angle = qAtan2(
        end.y() - before.y(),
        end.x() - before.x()
    );
    QPainterPath arrow(end);
    arrow.lineTo(end - QPointF(
        qCos(angle - M_PI / 6.0) * kArrowSize,
        qSin(angle - M_PI / 6.0) * kArrowSize
    ));
    arrow.lineTo(end - QPointF(
        qCos(angle + M_PI / 6.0) * kArrowSize,
        qSin(angle + M_PI / 6.0) * kArrowSize
    ));
    arrow.closeSubpath();
    return arrow;
}

QPainterPath FunctionCanvasEdgeItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(kEdgeHitWidth);
    QPainterPath result = stroker.createStroke(path());
    result.addPath(arrowHeadPath());
    return result;
}

void FunctionCanvasEdgeItem::paint(
    QPainter *painter,
    const QStyleOptionGraphicsItem *option,
    QWidget *)
{
    if (path().isEmpty()) {
        return;
    }
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = isSelected()
        || (option && (option->state & QStyle::State_Selected));
    const bool hovered =
        option && (option->state & QStyle::State_MouseOver);
    QColor color = functionCanvasPortColor(m_edge.fromPortId);
    if (hovered && !selected) {
        color = color.lighter(112);
    }
    if (selected) {
        color = QColor(QStringLiteral("#2563eb"));
    }
    const qreal width = selected ? 3.2 : (hovered ? 2.8 : 2.0);
    painter->setPen(QPen(color, width));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(arrowHeadPath());
}
