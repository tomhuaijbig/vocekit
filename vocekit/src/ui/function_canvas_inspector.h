#ifndef VOCEKIT_FUNCTION_CANVAS_INSPECTOR_H
#define VOCEKIT_FUNCTION_CANVAS_INSPECTOR_H

#include "../domain/function_flow_graph.h"

#include <QPair>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLayout;
class QVBoxLayout;
class QWidget;

struct FunctionCanvasInspectorOptions
{
    QVector<QPair<QString, QString>> models;
    QVector<QPair<QString, QString>> prompts;
    QVector<QPair<QString, QString>> speechProviders;
    QVector<QPair<QString, QString>> ocrEngines;
};

// 选中节点的类型化设置栏，不保存图，也不推导上下游目标。
class FunctionCanvasInspector : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionCanvasInspector(
        const FunctionCanvasInspectorOptions &options =
            FunctionCanvasInspectorOptions(),
        QWidget *parent = nullptr
    );

    void setGraphAndSelection(
        const FunctionFlowGraph &graph,
        const QString &nodeId
    );
    void clearSelection();
    QString selectedNodeId() const;
    void setEditable(bool editable);

signals:
    void nodeChanged(FunctionFlowNode node);
    void outputActionOrderChanged(
        QString outputNodeId,
        QStringList orderedEdgeIds
    );

private:
    void rebuild();
    void clearLayout(QLayout *layout);
    void addSection(
        const QString &title,
        const QString &objectName
    );
    QWidget *field(const QString &label, QWidget *control);
    QComboBox *combo(
        const QVector<QPair<QString, QString>> &options,
        const QString &current,
        const QString &objectName
    );
    void addVoiceFields();
    void addSelectionFields();
    void addScreenshotFields();
    void addInputFields();
    void addModelFields();
    void addOutputFields();
    void addPopupFields();
    void addScreenshotPanelFields();
    void addAutoWriteFields();
    void emitNodeChange();

    FunctionCanvasInspectorOptions m_options;
    FunctionFlowGraph m_graph;
    FunctionFlowNode m_node;
    QString m_selectedNodeId;
    QVBoxLayout *m_layout = nullptr;
    bool m_editable = true;
};

#endif // VOCEKIT_FUNCTION_CANVAS_INSPECTOR_H
