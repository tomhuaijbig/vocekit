#ifndef VOCEKIT_FUNCTION_CANVAS_EDGE_ITEM_H
#define VOCEKIT_FUNCTION_CANVAS_EDGE_ITEM_H

#include "../domain/function_flow_graph.h"

#include <QGraphicsPathItem>

class FunctionCanvasNodeItem;

class FunctionCanvasEdgeItem : public QGraphicsPathItem
{
public:
    FunctionCanvasEdgeItem(
        const FunctionFlowEdge &edge,
        FunctionCanvasNodeItem *fromNode,
        FunctionCanvasNodeItem *toNode,
        QGraphicsItem *parent = nullptr
    );

    QString edgeId() const;
    FunctionFlowEdge edge() const;
    void setEdge(
        const FunctionFlowEdge &edge,
        FunctionCanvasNodeItem *fromNode,
        FunctionCanvasNodeItem *toNode
    );
    void updatePath();

    QRectF boundingRect() const override;
    QPainterPath arrowHeadPath() const;
    QPainterPath shape() const override;
    void paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget = nullptr
    ) override;

private:
    FunctionFlowEdge m_edge;
    FunctionCanvasNodeItem *m_fromNode = nullptr;
    FunctionCanvasNodeItem *m_toNode = nullptr;
};

#endif // VOCEKIT_FUNCTION_CANVAS_EDGE_ITEM_H
