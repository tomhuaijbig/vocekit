#include "function_canvas_scene.h"

#include "function_canvas_edge_item.h"
#include "function_canvas_node_item.h"
#include "function_canvas_visual_style.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QPen>
#include <QSet>
#include <QStringList>

namespace {

const qreal kSceneWidth = 2600.0;
const qreal kSceneHeight = 1800.0;

bool sameEndpointDefinition(
    const FunctionFlowEdge &left,
    const FunctionFlowEdge &right)
{
    return left.fromNodeId == right.fromNodeId
        && left.fromPortId == right.fromPortId
        && left.toNodeId == right.toNodeId
        && left.toPortId == right.toPortId;
}

} // namespace

FunctionCanvasScene::FunctionCanvasScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0.0, 0.0, kSceneWidth, kSceneHeight);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}

void FunctionCanvasScene::setGraph(
    const FunctionFlowGraph &graph)
{
    const QString previousHash = m_graphHash;
    QSet<QString> selectedNodeIds;
    QSet<QString> selectedEdgeIds;
    for (auto it = m_nodes.constBegin();
         it != m_nodes.constEnd();
         ++it) {
        if (it.value()->isSelected()) {
            selectedNodeIds.insert(it.key());
        }
    }
    for (auto it = m_edges.constBegin();
         it != m_edges.constEnd();
         ++it) {
        if (it.value()->isSelected()) {
            selectedEdgeIds.insert(it.key());
        }
    }

    QMap<QString, FunctionFlowNode> incomingNodes;
    for (const FunctionFlowNode &node : graph.nodes) {
        if (!node.id.trimmed().isEmpty()
            && !incomingNodes.contains(node.id)) {
            incomingNodes.insert(node.id, node);
        }
    }

    const QStringList existingEdgeIds = m_edges.keys();
    for (const QString &edgeId : existingEdgeIds) {
        bool keep = false;
        for (const FunctionFlowEdge &edge : graph.edges) {
            if (edge.id == edgeId) {
                keep = true;
                break;
            }
        }
        if (!keep) {
            removeEdgeItem(edgeId);
        }
    }
    const QStringList existingNodeIds = m_nodes.keys();
    for (const QString &nodeId : existingNodeIds) {
        if (!incomingNodes.contains(nodeId)
            || incomingNodes.value(nodeId).type
                != m_nodes.value(nodeId)->nodeType()) {
            removeNodeItem(nodeId);
        }
    }
    for (auto it = incomingNodes.constBegin();
         it != incomingNodes.constEnd();
         ++it) {
        if (m_nodes.contains(it.key())) {
            m_nodes.value(it.key())->setNode(it.value());
        } else {
            addNodeItem(it.value());
        }
    }

    QSet<QString> validIncomingEdgeIds;
    for (const FunctionFlowEdge &edge : graph.edges) {
        if (edge.id.trimmed().isEmpty()
            || validIncomingEdgeIds.contains(edge.id)
            || !m_nodes.contains(edge.fromNodeId)
            || !m_nodes.contains(edge.toNodeId)) {
            continue;
        }
        FunctionCanvasNodeItem *from =
            m_nodes.value(edge.fromNodeId);
        FunctionCanvasNodeItem *to =
            m_nodes.value(edge.toNodeId);
        if (!isFunctionFlowConnectionAllowed(
                from->nodeType(),
                edge.fromPortId,
                to->nodeType(),
                edge.toPortId)) {
            continue;
        }
        validIncomingEdgeIds.insert(edge.id);
        if (m_edges.contains(edge.id)
            && sameEndpointDefinition(
                m_edges.value(edge.id)->edge(),
                edge
            )) {
            m_edges.value(edge.id)->setEdge(edge, from, to);
        } else {
            removeEdgeItem(edge.id);
            addEdgeItem(edge);
        }
    }
    for (const QString &edgeId : m_edges.keys()) {
        if (!validIncomingEdgeIds.contains(edgeId)) {
            removeEdgeItem(edgeId);
        }
    }

    m_graph = graph;
    m_graphHash = functionFlowGraphHash(graph);
    if (previousHash != m_graphHash) {
        clearRuntimeOverlay();
    }
    for (const QString &nodeId : selectedNodeIds) {
        if (m_nodes.contains(nodeId)) {
            m_nodes.value(nodeId)->setSelected(true);
        }
    }
    for (const QString &edgeId : selectedEdgeIds) {
        if (m_edges.contains(edgeId)) {
            m_edges.value(edgeId)->setSelected(true);
        }
    }
}

const FunctionFlowGraph &FunctionCanvasScene::graph() const
{
    return m_graph;
}

QString FunctionCanvasScene::graphHash() const
{
    return m_graphHash;
}

int FunctionCanvasScene::nodeCount() const
{
    return m_nodes.size();
}

int FunctionCanvasScene::edgeCount() const
{
    return m_edges.size();
}

FunctionCanvasNodeItem *FunctionCanvasScene::nodeItem(
    const QString &nodeId) const
{
    return m_nodes.value(nodeId, nullptr);
}

FunctionCanvasEdgeItem *FunctionCanvasScene::edgeItem(
    const QString &edgeId) const
{
    return m_edges.value(edgeId, nullptr);
}

bool FunctionCanvasScene::hasTemporaryConnection() const
{
    return m_temporaryConnection != nullptr;
}

FunctionCanvasConnectionTargetState
FunctionCanvasScene::connectionTargetState() const
{
    return m_connectionTargetState;
}

QPainterPath FunctionCanvasScene::temporaryConnectionPath() const
{
    return m_temporaryConnection
        ? m_temporaryConnection->path()
        : QPainterPath();
}

QColor FunctionCanvasScene::temporaryConnectionColor() const
{
    return m_temporaryConnection
        ? m_temporaryConnection->pen().color()
        : QColor();
}

void FunctionCanvasScene::requestNodePlacement(
    FunctionFlowNodeType type,
    const QPointF &position)
{
    Q_EMIT nodePlacementRequested(type, position);
}

bool FunctionCanvasScene::requestConnection(
    const FunctionFlowEndpoint &from,
    const FunctionFlowEndpoint &to)
{
    if (!m_nodes.contains(from.nodeId)
        || !m_nodes.contains(to.nodeId)) {
        return false;
    }
    const FunctionCanvasNodeItem *fromNode =
        m_nodes.value(from.nodeId);
    const FunctionCanvasNodeItem *toNode =
        m_nodes.value(to.nodeId);
    if (!isFunctionFlowConnectionAllowed(
            fromNode->nodeType(),
            from.portId,
            toNode->nodeType(),
            to.portId)) {
        return false;
    }
    Q_EMIT connectionRequested(from, to);
    return true;
}

bool FunctionCanvasScene::requestNodeRemoval(
    const QString &nodeId)
{
    if (!m_nodes.contains(nodeId)) {
        return false;
    }
    Q_EMIT nodeRemovalRequested(nodeId);
    return true;
}

bool FunctionCanvasScene::requestEdgeRemoval(
    const QString &edgeId)
{
    if (!m_edges.contains(edgeId)) {
        return false;
    }
    Q_EMIT edgeRemovalRequested(edgeId);
    return true;
}

bool FunctionCanvasScene::applyRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    if (event.publishedHash != m_graphHash
        || !m_nodes.contains(event.nodeId)) {
        return false;
    }
    FunctionCanvasNodeItem *node =
        m_nodes.value(event.nodeId);
    if (node->nodeType() != event.nodeType) {
        return false;
    }
    node->setRuntimeState(event.state);
    return true;
}

void FunctionCanvasScene::clearRuntimeOverlay()
{
    for (FunctionCanvasNodeItem *node : m_nodes) {
        node->setRuntimeState(FunctionFlowNodeState::Pending);
    }
}

void FunctionCanvasScene::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    if (event && event->button() == Qt::LeftButton) {
        QString portId;
        FunctionCanvasNodeItem *node = nodeAtPort(
            event->scenePos(),
            FunctionFlowPortDirection::Output,
            &portId
        );
        if (node && !portId.isEmpty()) {
            m_connectionStart.nodeId = node->nodeId();
            m_connectionStart.portId = portId;
            m_connectionStartPosition =
                node->portScenePosition(portId);
            m_temporaryConnection =
                addPath(QPainterPath(), QPen(
                    functionCanvasPortColor(portId),
                    2.0,
                    Qt::DashLine
            ));
            m_temporaryConnection->setZValue(-0.5);
            updateTemporaryConnection(event->scenePos());
            setTemporaryConnectionTargetState(
                FunctionCanvasConnectionTargetState::None
            );
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void FunctionCanvasScene::mouseMoveEvent(
    QGraphicsSceneMouseEvent *event)
{
    if (m_temporaryConnection && event) {
        updateTemporaryConnection(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void FunctionCanvasScene::mouseReleaseEvent(
    QGraphicsSceneMouseEvent *event)
{
    if (m_temporaryConnection && event) {
        updateTemporaryConnection(event->scenePos());
        QString portId;
        FunctionCanvasNodeItem *node = nodeAtPort(
            event->scenePos(),
            FunctionFlowPortDirection::Input,
            &portId
        );
        if (m_connectionTargetState
                == FunctionCanvasConnectionTargetState::Valid
            && node && !portId.isEmpty()) {
            FunctionFlowEndpoint target;
            target.nodeId = node->nodeId();
            target.portId = portId;
            requestConnection(m_connectionStart, target);
        }
        clearTemporaryConnection();
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void FunctionCanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event && event->key() == Qt::Key_Escape
        && m_temporaryConnection) {
        clearTemporaryConnection();
        event->accept();
        return;
    }
    if (event
        && (event->key() == Qt::Key_Delete
            || event->key() == Qt::Key_Backspace)) {
        event->accept();
        if (event->isAutoRepeat()) {
            return;
        }
        QStringList selectedEdgeIds;
        QStringList selectedNodeIds;
        {
            const QList<QGraphicsItem *> selected = selectedItems();
            for (QGraphicsItem *item : selected) {
                if (FunctionCanvasEdgeItem *edge =
                        dynamic_cast<FunctionCanvasEdgeItem *>(item)) {
                    selectedEdgeIds.append(edge->edgeId());
                } else if (FunctionCanvasNodeItem *node =
                               dynamic_cast<FunctionCanvasNodeItem *>(item)) {
                    selectedNodeIds.append(node->nodeId());
                }
            }
        }
        for (const QString &edgeId : selectedEdgeIds) {
            requestEdgeRemoval(edgeId);
        }
        for (const QString &nodeId : selectedNodeIds) {
            requestNodeRemoval(nodeId);
        }
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void FunctionCanvasScene::addNodeItem(
    const FunctionFlowNode &node)
{
    FunctionCanvasNodeItem *item =
        new FunctionCanvasNodeItem(node);
    addItem(item);
    m_nodes.insert(node.id, item);
    connect(
        item,
        &FunctionCanvasNodeItem::positionCommitted,
        this,
        &FunctionCanvasScene::positionCommitted
    );
    connect(
        item,
        &FunctionCanvasNodeItem::settingsRequested,
        this,
        &FunctionCanvasScene::nodeSettingsRequested
    );
    const QString nodeId = node.id;
    connect(item, &QGraphicsObject::xChanged, this, [
        this,
        nodeId
    ]() {
        refreshEdgesForNode(nodeId);
    });
    connect(item, &QGraphicsObject::yChanged, this, [
        this,
        nodeId
    ]() {
        refreshEdgesForNode(nodeId);
    });
}

void FunctionCanvasScene::removeNodeItem(
    const QString &nodeId)
{
    if (!m_nodes.contains(nodeId)) {
        return;
    }
    if (m_temporaryConnection
        && m_connectionStart.nodeId == nodeId) {
        clearTemporaryConnection();
    }
    const QStringList edgeIds = m_edges.keys();
    for (const QString &edgeId : edgeIds) {
        const FunctionFlowEdge edge =
            m_edges.value(edgeId)->edge();
        if (edge.fromNodeId == nodeId
            || edge.toNodeId == nodeId) {
            removeEdgeItem(edgeId);
        }
    }
    FunctionCanvasNodeItem *item = m_nodes.take(nodeId);
    removeItem(item);
    delete item;
}

void FunctionCanvasScene::addEdgeItem(
    const FunctionFlowEdge &edge)
{
    if (!m_nodes.contains(edge.fromNodeId)
        || !m_nodes.contains(edge.toNodeId)) {
        return;
    }
    FunctionCanvasEdgeItem *item =
        new FunctionCanvasEdgeItem(
            edge,
            m_nodes.value(edge.fromNodeId),
            m_nodes.value(edge.toNodeId)
        );
    addItem(item);
    m_edges.insert(edge.id, item);
}

void FunctionCanvasScene::removeEdgeItem(
    const QString &edgeId)
{
    if (!m_edges.contains(edgeId)) {
        return;
    }
    FunctionCanvasEdgeItem *item = m_edges.take(edgeId);
    removeItem(item);
    delete item;
}

void FunctionCanvasScene::refreshEdgesForNode(
    const QString &nodeId)
{
    for (FunctionCanvasEdgeItem *edge : m_edges) {
        const FunctionFlowEdge definition = edge->edge();
        if (definition.fromNodeId == nodeId
            || definition.toNodeId == nodeId) {
            edge->updatePath();
        }
    }
}

FunctionCanvasNodeItem *FunctionCanvasScene::nodeAtPort(
    const QPointF &scenePosition,
    FunctionFlowPortDirection direction,
    QString *portId) const
{
    if (portId) {
        portId->clear();
    }
    for (FunctionCanvasNodeItem *node : m_nodes) {
        const QString candidate =
            node->portAt(scenePosition, direction);
        if (!candidate.isEmpty()) {
            if (portId) {
                *portId = candidate;
            }
            return node;
        }
    }
    return nullptr;
}

FunctionCanvasConnectionTargetState
FunctionCanvasScene::connectionTargetStateAt(
    const QPointF &scenePosition) const
{
    QString portId;
    FunctionCanvasNodeItem *node = nodeAtPort(
        scenePosition,
        FunctionFlowPortDirection::Input,
        &portId
    );
    if (node && !portId.isEmpty()) {
        return isFunctionFlowConnectionAllowed(
            m_nodes.value(m_connectionStart.nodeId)->nodeType(),
            m_connectionStart.portId,
            node->nodeType(),
            portId
        )
            ? FunctionCanvasConnectionTargetState::Valid
            : FunctionCanvasConnectionTargetState::Invalid;
    }

    const QList<QGraphicsItem *> candidates = items(scenePosition);
    for (QGraphicsItem *candidate : candidates) {
        if (dynamic_cast<FunctionCanvasNodeItem *>(candidate)) {
            return FunctionCanvasConnectionTargetState::Invalid;
        }
    }
    return FunctionCanvasConnectionTargetState::None;
}

void FunctionCanvasScene::setTemporaryConnectionTargetState(
    FunctionCanvasConnectionTargetState state)
{
    m_connectionTargetState = state;
    if (!m_temporaryConnection) {
        return;
    }

    QColor color = functionCanvasPortColor(
        m_connectionStart.portId
    );
    if (state == FunctionCanvasConnectionTargetState::Valid) {
        color = QColor(QStringLiteral("#16a34a"));
    } else if (
        state == FunctionCanvasConnectionTargetState::Invalid) {
        color = QColor(QStringLiteral("#dc2626"));
    }
    QPen pen = m_temporaryConnection->pen();
    pen.setColor(color);
    pen.setWidthF(state == FunctionCanvasConnectionTargetState::None
        ? 2.0
        : 2.6);
    m_temporaryConnection->setPen(pen);
}

void FunctionCanvasScene::updateTemporaryConnection(
    const QPointF &end)
{
    if (!m_temporaryConnection) {
        return;
    }
    const qreal controlDistance = qMax(
        80.0,
        qAbs(end.x() - m_connectionStartPosition.x()) * 0.45
    );
    QPainterPath path(m_connectionStartPosition);
    path.cubicTo(
        QPointF(
            m_connectionStartPosition.x() + controlDistance,
            m_connectionStartPosition.y()
        ),
        QPointF(end.x() - controlDistance, end.y()),
        end
    );
    m_temporaryConnection->setPath(path);
    setTemporaryConnectionTargetState(
        connectionTargetStateAt(end)
    );
}

void FunctionCanvasScene::clearTemporaryConnection()
{
    if (m_temporaryConnection) {
        removeItem(m_temporaryConnection);
        delete m_temporaryConnection;
        m_temporaryConnection = nullptr;
    }
    m_connectionStart = FunctionFlowEndpoint();
    m_connectionStartPosition = QPointF();
    m_connectionTargetState =
        FunctionCanvasConnectionTargetState::None;
}
