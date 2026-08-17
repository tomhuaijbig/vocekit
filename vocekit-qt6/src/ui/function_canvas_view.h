#ifndef VOCEKIT_FUNCTION_CANVAS_VIEW_H
#define VOCEKIT_FUNCTION_CANVAS_VIEW_H

#include "../domain/function_flow_graph.h"

#include <QGraphicsView>
#include <QMetaObject>

class FunctionCanvasScene;
class QWheelEvent;

class FunctionCanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit FunctionCanvasView(QWidget *parent = nullptr);

    void setCanvasScene(FunctionCanvasScene *scene);
    FunctionCanvasScene *canvasScene() const;
    void resetLayout();
    QPointF viewportCenter() const;
    qreal zoomLevel() const;
    void restoreViewport(const FunctionFlowEditorState &editor);

signals:
    void viewportChanged(QPointF center, qreal zoom);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void disconnectCanvasSceneSignals();
    void notifyViewportChanged();

    FunctionCanvasScene *m_canvasScene = nullptr;
    QMetaObject::Connection m_sceneChangedConnection;
    QMetaObject::Connection m_sceneDestroyedConnection;
    qreal m_zoomLevel = 1.0;
    bool m_canvasSceneEmpty = true;
    bool m_restoringViewport = false;
};

#endif // VOCEKIT_FUNCTION_CANVAS_VIEW_H
