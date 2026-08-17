#ifndef VOCEKIT_REORDERABLE_CARD_COLUMN_H
#define VOCEKIT_REORDERABLE_CARD_COLUMN_H

#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QStringList>
#include <QWidget>

#include <functional>

class QVBoxLayout;

// 让设置卡片通过按住标题区域并上下拖动来调整顺序。
class ReorderableCardColumn : public QWidget
{
public:
    explicit ReorderableCardColumn(QWidget *parent = nullptr);

    void addCard(
        const QString &id,
        QWidget *card,
        QWidget *dragSurface = nullptr
    );
    QStringList order() const;
    bool moveCard(int from, int to);
    void setOrderChangedCallback(
        const std::function<void(const QStringList &)> &callback
    );

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool moveCardInternal(int from, int to, bool notify);
    int indexOf(const QString &id) const;
    void installDragFilters(QWidget *surface, const QString &id);
    void updateDragPosition(const QPoint &globalPosition);
    void finishDrag(bool notify);

    QVBoxLayout *m_layout = nullptr;
    QHash<QString, QWidget *> m_cards;
    QHash<QString, QWidget *> m_dragSurfaces;
    std::function<void(const QStringList &)> m_orderChanged;
    QString m_pressedId;
    QPoint m_pressGlobal;
    QElapsedTimer m_pressTimer;
    bool m_dragging = false;
    bool m_orderChangedDuringDrag = false;
};

#endif // VOCEKIT_REORDERABLE_CARD_COLUMN_H
