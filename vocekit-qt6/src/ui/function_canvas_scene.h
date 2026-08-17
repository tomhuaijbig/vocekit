#ifndef VOCEKIT_FUNCTION_CANVAS_SCENE_H
#define VOCEKIT_FUNCTION_CANVAS_SCENE_H

#include "../domain/function_flow_graph.h"
#include "../domain/function_flow_runtime_types.h"

#include <QColor>
#include <QGraphicsScene>
#include <QMap>
#include <QPainterPath>

class FunctionCanvasEdgeItem;
class FunctionCanvasNodeItem;
class QGraphicsPathItem;

enum class FunctionCanvasConnectionTargetState
{
    None,
    Valid,
    Invalid
};

class FunctionCanvasScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit FunctionCanvasScene(QObject *parent = nullptr);

    void setGraph(const FunctionFlowGraph &graph);
    const FunctionFlowGraph &graph() const;
    QString graphHash() const;
    int nodeCount() const;
    int edgeCount() const;
    FunctionCanvasNodeItem *nodeItem(
        const QString &nodeId
    ) const;
    FunctionCanvasEdgeItem *edgeItem(
        const QString &edgeId
    ) const;
    bool hasTemporaryConnection() const;
    FunctionCanvasConnectionTargetState
    connectionTargetState() const;
    QPainterPath temporaryConnectionPath() const;
    QColor temporaryConnectionColor() const;

    void requestNodePlacement(
        FunctionFlowNodeType type,
        const QPointF &position
    );
    bool requestConnection(
        const FunctionFlowEndpoint &from,
        const FunctionFlowEndpoint &to
    );
    bool requestNodeRemoval(const QString &nodeId);
    bool requestEdgeRemoval(const QString &edgeId);

    bool applyRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    void clearRuntimeOverlay();

signals:
    void nodePlacementRequested(
        FunctionFlowNodeType type,
        QPointF position
    );
    void connectionRequested(
        FunctionFlowEndpoint from,
        FunctionFlowEndpoint to
    );
    void nodeRemovalRequested(QString nodeId);
    void edgeRemovalRequested(QString edgeId);
    void positionCommitted(QString nodeId, QPointF position);
    void nodeSettingsRequested(QString nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void addNodeItem(const FunctionFlowNode &node);
    void removeNodeItem(const QString &nodeId);
    void addEdgeItem(const FunctionFlowEdge &edge);
    void removeEdgeItem(const QString &edgeId);
    void refreshEdgesForNode(const QString &nodeId);
    FunctionCanvasNodeItem *nodeAtPort(
        const QPointF &scenePosition,
        FunctionFlowPortDirection direction,
        QString *portId
    ) const;
    FunctionCanvasConnectionTargetState connectionTargetStateAt(
        const QPointF &scenePosition
    ) const;
    void setTemporaryConnectionTargetState(
        FunctionCanvasConnectionTargetState state
    );
    void updateTemporaryConnection(const QPointF &end);
    void clearTemporaryConnection();

    FunctionFlowGraph m_graph;
    QString m_graphHash;
    QMap<QString, FunctionCanvasNodeItem *> m_nodes;
    QMap<QString, FunctionCanvasEdgeItem *> m_edges;
    FunctionFlowEndpoint m_connectionStart;
    QPointF m_connectionStartPosition;
    QGraphicsPathItem *m_temporaryConnection = nullptr;
    FunctionCanvasConnectionTargetState m_connectionTargetState =
        FunctionCanvasConnectionTargetState::None;
};

#endif // VOCEKIT_FUNCTION_CANVAS_SCENE_H
