#ifndef VOCEKIT_FUNCTION_CANVAS_PALETTE_H
#define VOCEKIT_FUNCTION_CANVAS_PALETTE_H

#include "../domain/function_flow_ports.h"

#include <QVector>
#include <QWidget>

#include <functional>

class QDrag;
class QLineEdit;
class QLabel;
class QPushButton;
class QVBoxLayout;

// 九种固定节点的可搜索节点库；按钮同时支持点击和拖放。
class FunctionCanvasPalette : public QWidget
{
    Q_OBJECT

public:
    // 同步借用：runner 必须在调用返回前完成，不能保存 QDrag 引用。
    typedef std::function<Qt::DropAction(QDrag &)> DragRunner;

    explicit FunctionCanvasPalette(
        QWidget *parent = nullptr,
        const DragRunner &dragRunner = DragRunner()
    );

    QVector<FunctionFlowNodeType> nodeTypes() const;
    void setFilterText(const QString &text);
    int visibleNodeCount() const;

signals:
    void nodeTypeChosen(FunctionFlowNodeType type);

private:
    struct Entry
    {
        FunctionFlowNodeType type = FunctionFlowNodeType::Input;
        QString title;
        QString keywords;
        int category = 0;
        QPushButton *button = nullptr;
    };

    struct Category
    {
        QWidget *container = nullptr;
        QVBoxLayout *layout = nullptr;
    };

    QLineEdit *m_filter = nullptr;
    QLabel *m_empty = nullptr;
    DragRunner m_dragRunner;
    QVector<Category> m_categories;
    QVector<Entry> m_entries;
};

QString functionCanvasNodeMimeType();

#endif // VOCEKIT_FUNCTION_CANVAS_PALETTE_H
