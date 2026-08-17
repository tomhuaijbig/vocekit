#ifndef MODE_CARD_FRAME_H
#define MODE_CARD_FRAME_H

#include <QFrame>
#include <QPoint>
#include <QString>

#include <functional>

// 主页功能卡片：负责拖动排序和双击编辑，避免主窗口直接承载鼠标拖拽细节。
class ModeCardFrame : public QFrame
{
public:
    explicit ModeCardFrame(const QString &id, QWidget *parent = nullptr);

    void setDropCallback(const std::function<void(const QString &, const QString &, bool)> &callback);
    void setDoubleClickCallback(const std::function<void()> &callback);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    static QString mimeType();
    bool acceptsEvent(const QDropEvent *event) const;
    void setDropHighlighted(bool highlighted);

    QString m_id;
    QPoint m_dragStartPosition;
    std::function<void(const QString &, const QString &, bool)> m_dropCallback;
    std::function<void()> m_doubleClickCallback;
};

#endif // MODE_CARD_FRAME_H
