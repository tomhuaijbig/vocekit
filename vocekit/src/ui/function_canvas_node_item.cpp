#include "function_canvas_node_item.h"

#include "function_canvas_visual_style.h"

#include <QCursor>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {

const qreal kNodeWidth = 224.0;
const qreal kMinimumNodeHeight = 132.0;
const qreal kPortStartY = 106.0;
const qreal kPortSpacing = 26.0;
const qreal kPortRadius = 5.5;

int portCount(
    FunctionFlowNodeType type,
    FunctionFlowPortDirection direction)
{
    int count = 0;
    for (const FunctionFlowPortSpec &spec :
         functionFlowPortSpecs(type)) {
        if (spec.direction == direction) {
            ++count;
        }
    }
    return count;
}

} // namespace

QString functionCanvasNodeTypeDisplayName(
    FunctionFlowNodeType type)
{
    return functionCanvasNodeDisplayName(type);
}

QString functionCanvasPortDisplayName(const QString &portId)
{
    if (portId == QStringLiteral("text_in")) {
        return QStringLiteral("文字输入");
    }
    if (portId == QStringLiteral("text_out")) {
        return QStringLiteral("文字输出");
    }
    if (portId == QStringLiteral("action_in")) {
        return QStringLiteral("动作输入");
    }
    if (portId == QStringLiteral("action_out")) {
        return QStringLiteral("动作输出");
    }
    return portId;
}

FunctionCanvasNodeItem::FunctionCanvasNodeItem(
    const FunctionFlowNode &node,
    QGraphicsItem *parent)
    : QGraphicsObject(parent),
      m_node(node)
{
    setFlags(
        QGraphicsItem::ItemIsMovable
        | QGraphicsItem::ItemIsSelectable
        | QGraphicsItem::ItemIsFocusable
        | QGraphicsItem::ItemSendsGeometryChanges
    );
    setCursor(QCursor(Qt::OpenHandCursor));
    setPos(node.position);
    setOpacity(1.0);
    setZValue(1.0);
}

QRectF FunctionCanvasNodeItem::boundsForType(
    FunctionFlowNodeType type)
{
    const int rows = qMax(
        portCount(
            type,
            FunctionFlowPortDirection::Input
        ),
        portCount(
            type,
            FunctionFlowPortDirection::Output
        )
    );
    const qreal height = qMax(
        kMinimumNodeHeight,
        kPortStartY + rows * kPortSpacing + 13.0
    );
    return QRectF(
        -kPortRadius - 3.0,
        -3.0,
        kNodeWidth + (kPortRadius + 3.0) * 2.0,
        height + 6.0
    );
}

QRectF FunctionCanvasNodeItem::boundingRect() const
{
    return boundsForType(m_node.type);
}

void FunctionCanvasNodeItem::paint(
    QPainter *painter,
    const QStyleOptionGraphicsItem *,
    QWidget *)
{
    const QRectF body(
        0.0,
        0.0,
        kNodeWidth,
        boundingRect().height() - 6.0
    );
    const QColor accent = functionCanvasNodeAccent(m_node.type);
    const QColor runtime =
        functionCanvasRuntimeColor(m_runtimeState);
    const QColor bodyColor = m_node.enabled
        ? QColor(QStringLiteral("#ffffff"))
        : QColor(QStringLiteral("#f1f5f9"));
    const QColor titleColor = m_node.enabled
        ? QColor(QStringLiteral("#172033"))
        : QColor(QStringLiteral("#475569"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(
        QPen(
            isSelected()
                ? QColor(QStringLiteral("#2563eb"))
                : QColor(QStringLiteral("#cbd5e1")),
            isSelected() ? 2.4 : 1.4
        )
    );
    painter->setBrush(bodyColor);
    painter->drawRoundedRect(body, 12.0, 12.0);

    painter->setPen(Qt::NoPen);
    painter->setBrush(accent);
    painter->drawRect(
        QRectF(12.0, 0.0, kNodeWidth - 24.0, 4.0)
    );

    painter->setBrush(accent);
    painter->drawRoundedRect(
        QRectF(14.0, 14.0, 32.0, 32.0),
        9.0,
        9.0
    );
    painter->setPen(QColor(QStringLiteral("#ffffff")));
    QFont glyphFont = painter->font();
    glyphFont.setBold(true);
    glyphFont.setPointSizeF(qMax(10.0, glyphFont.pointSizeF()));
    painter->setFont(glyphFont);
    painter->drawText(
        QRectF(14.0, 14.0, 32.0, 32.0),
        Qt::AlignCenter,
        functionCanvasNodeGlyph(m_node.type)
    );

    painter->setPen(titleColor);
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    painter->setFont(titleFont);
    const QString displayName =
        functionCanvasNodeDisplayName(m_node.type);
    const QString label = m_node.title.trimmed().isEmpty()
        ? displayName
        : m_node.title;
    painter->drawText(
        QRectF(56.0, 11.0, kNodeWidth - 88.0, 22.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter->fontMetrics().elidedText(
            label,
            Qt::ElideRight,
            static_cast<int>(kNodeWidth - 88.0)
        )
    );

    QFont typeFont = painter->font();
    typeFont.setBold(false);
    typeFont.setPointSizeF(qMax(8.0, typeFont.pointSizeF() - 1.0));
    painter->setFont(typeFont);
    painter->setPen(QColor(QStringLiteral("#64748b")));
    painter->drawText(
        QRectF(56.0, 31.0, kNodeWidth - 88.0, 16.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        displayName
    );

    painter->setPen(Qt::NoPen);
    painter->setBrush(runtime);
    painter->drawEllipse(
        QPointF(kNodeWidth - 17.0, 20.0),
        5.0,
        5.0
    );
    if (m_runtimeState == FunctionFlowNodeState::Failed) {
        painter->setPen(QColor(QStringLiteral("#ffffff")));
        QFont failureFont = painter->font();
        failureFont.setBold(true);
        failureFont.setPointSizeF(7.0);
        painter->setFont(failureFont);
        painter->drawText(
            QRectF(kNodeWidth - 22.0, 15.0, 10.0, 10.0),
            Qt::AlignCenter,
            QStringLiteral("!")
        );
    }

    QFont summaryFont = painter->font();
    summaryFont.setBold(false);
    summaryFont.setPointSizeF(qMax(
        8.0,
        summaryFont.pointSizeF()
    ));
    painter->setFont(summaryFont);
    painter->setPen(QColor(QStringLiteral("#64748b")));
    const QString summary = functionCanvasNodeSummary(m_node);
    painter->drawText(
        QRectF(14.0, 56.0, kNodeWidth - 28.0, 24.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter->fontMetrics().elidedText(
            summary,
            Qt::ElideRight,
            static_cast<int>(kNodeWidth - 28.0)
        )
    );

    painter->setPen(QPen(
        QColor(QStringLiteral("#e2e8f0")),
        1.0
    ));
    painter->drawLine(
        QPointF(14.0, 88.0),
        QPointF(kNodeWidth - 14.0, 88.0)
    );

    for (const FunctionFlowPortSpec &spec :
         functionFlowPortSpecs(m_node.type)) {
        const QPointF port = portPosition(spec.id);
        painter->setPen(
            QPen(QColor(QStringLiteral("#ffffff")), 1.5)
        );
        painter->setBrush(functionCanvasPortColor(spec.id));
        painter->drawEllipse(port, kPortRadius, kPortRadius);

        painter->setPen(QColor(QStringLiteral("#475569")));
        const qreal textX =
            spec.direction == FunctionFlowPortDirection::Input
                ? 14.0
                : kNodeWidth - 104.0;
        painter->drawText(
            QRectF(textX, port.y() - 9.0, 90.0, 18.0),
            spec.direction == FunctionFlowPortDirection::Input
                ? Qt::AlignLeft | Qt::AlignVCenter
                : Qt::AlignRight | Qt::AlignVCenter,
            functionCanvasPortDisplayName(spec.id)
        );
    }
}

QString FunctionCanvasNodeItem::nodeId() const
{
    return m_node.id;
}

FunctionFlowNodeType FunctionCanvasNodeItem::nodeType() const
{
    return m_node.type;
}

QString FunctionCanvasNodeItem::title() const
{
    return m_node.title;
}

QStringList FunctionCanvasNodeItem::portIds() const
{
    QStringList result;
    for (const FunctionFlowPortSpec &spec :
         functionFlowPortSpecs(m_node.type)) {
        result.append(spec.id);
    }
    return result;
}

bool FunctionCanvasNodeItem::hasPort(const QString &portId) const
{
    return portIds().contains(portId);
}

QPointF FunctionCanvasNodeItem::portScenePosition(
    const QString &portId) const
{
    if (!hasPort(portId)) {
        return QPointF(qQNaN(), qQNaN());
    }
    return mapToScene(portPosition(portId));
}

QString FunctionCanvasNodeItem::portAt(
    const QPointF &scenePosition,
    FunctionFlowPortDirection direction,
    qreal radius) const
{
    for (const FunctionFlowPortSpec &spec :
         functionFlowPortSpecs(m_node.type)) {
        if (spec.direction != direction) {
            continue;
        }
        if (QLineF(
                portScenePosition(spec.id),
                scenePosition
            ).length() <= radius) {
            return spec.id;
        }
    }
    return QString();
}

FunctionFlowNodeState
FunctionCanvasNodeItem::runtimeState() const
{
    return m_runtimeState;
}

void FunctionCanvasNodeItem::setNode(
    const FunctionFlowNode &node)
{
    if (node.id != m_node.id || node.type != m_node.type) {
        return;
    }
    prepareGeometryChange();
    m_node = node;
    if (pos() != node.position) {
        setPos(node.position);
    }
    setOpacity(1.0);
    update();
}

void FunctionCanvasNodeItem::setRuntimeState(
    FunctionFlowNodeState state)
{
    if (m_runtimeState == state) {
        return;
    }
    m_runtimeState = state;
    update();
}

void FunctionCanvasNodeItem::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    m_pressPosition = pos();
    m_dragActive = event && event->button() == Qt::LeftButton;
    setCursor(QCursor(Qt::ClosedHandCursor));
    QGraphicsObject::mousePressEvent(event);
}

void FunctionCanvasNodeItem::mouseReleaseEvent(
    QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    setCursor(QCursor(Qt::OpenHandCursor));
    if (m_dragActive
        && event && event->button() == Qt::LeftButton
        && pos() != m_pressPosition) {
        Q_EMIT positionCommitted(m_node.id, pos());
    }
    m_dragActive = false;
}

void FunctionCanvasNodeItem::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseDoubleClickEvent(event);
    if (event && event->button() == Qt::LeftButton) {
        Q_EMIT settingsRequested(m_node.id);
    }
}

QPointF FunctionCanvasNodeItem::portPosition(
    const QString &portId) const
{
    int inputIndex = 0;
    int outputIndex = 0;
    for (const FunctionFlowPortSpec &spec :
         functionFlowPortSpecs(m_node.type)) {
        if (spec.id == portId) {
            const int index =
                spec.direction == FunctionFlowPortDirection::Input
                    ? inputIndex
                    : outputIndex;
            return QPointF(
                spec.direction == FunctionFlowPortDirection::Input
                    ? 0.0
                    : kNodeWidth,
                kPortStartY + index * kPortSpacing
            );
        }
        if (spec.direction == FunctionFlowPortDirection::Input) {
            ++inputIndex;
        } else {
            ++outputIndex;
        }
    }
    return QPointF(qQNaN(), qQNaN());
}
