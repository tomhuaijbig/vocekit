#ifndef VOCEKIT_FUNCTION_CANVAS_NODE_ITEM_H
#define VOCEKIT_FUNCTION_CANVAS_NODE_ITEM_H

#include "../domain/function_flow_graph.h"
#include "../domain/function_flow_runtime_types.h"

#include <QGraphicsObject>
#include <QStringList>

QString functionCanvasNodeTypeDisplayName(
    FunctionFlowNodeType type
);
QString functionCanvasPortDisplayName(const QString &portId);

class FunctionCanvasNodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit FunctionCanvasNodeItem(
        const FunctionFlowNode &node,
        QGraphicsItem *parent = nullptr
    );

    static QRectF boundsForType(FunctionFlowNodeType type);
    QRectF boundingRect() const override;
    void paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget = nullptr
    ) override;

    QString nodeId() const;
    FunctionFlowNodeType nodeType() const;
    QString title() const;
    QStringList portIds() const;
    bool hasPort(const QString &portId) const;
    QPointF portScenePosition(const QString &portId) const;
    QString portAt(
        const QPointF &scenePosition,
        FunctionFlowPortDirection direction,
        qreal radius = 12.0
    ) const;
    FunctionFlowNodeState runtimeState() const;

    void setNode(const FunctionFlowNode &node);
    void setRuntimeState(FunctionFlowNodeState state);

signals:
    void positionCommitted(QString nodeId, QPointF position);
    void settingsRequested(QString nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(
        QGraphicsSceneMouseEvent *event
    ) override;

private:
    QPointF portPosition(const QString &portId) const;

    FunctionFlowNode m_node;
    FunctionFlowNodeState m_runtimeState =
        FunctionFlowNodeState::Pending;
    QPointF m_pressPosition;
    bool m_dragActive = false;
};

#endif // VOCEKIT_FUNCTION_CANVAS_NODE_ITEM_H
