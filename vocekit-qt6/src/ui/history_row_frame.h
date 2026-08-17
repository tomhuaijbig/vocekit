#ifndef VOCEKIT_HISTORY_ROW_FRAME_H
#define VOCEKIT_HISTORY_ROW_FRAME_H

#include <QFrame>
#include <QMouseEvent>

#include <functional>

// 历史记录行：点击整行打开详情，同时保留右侧“操作”菜单和批量选择。
class HistoryRowFrame : public QFrame
{
public:
    explicit HistoryRowFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    void setClickCallback(const std::function<void()> &callback)
    {
        m_clickCallback = callback;
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_clickCallback) {
            m_clickCallback();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    std::function<void()> m_clickCallback;
};

#endif // VOCEKIT_HISTORY_ROW_FRAME_H
