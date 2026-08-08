#include "reorderable_card_column.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace {

const char kReorderCardIdProperty[] = "vocekitReorderCardId";

} // namespace

ReorderableCardColumn::ReorderableCardColumn(QWidget *parent)
    : QWidget(parent),
      m_layout(new QVBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(9);
}

void ReorderableCardColumn::addCard(
    const QString &id,
    QWidget *card,
    QWidget *dragSurface
)
{
    const QString normalizedId = id.trimmed();
    if (normalizedId.isEmpty() || !card || m_cards.contains(normalizedId)) {
        return;
    }

    m_cards.insert(normalizedId, card);
    m_dragSurfaces.insert(
        normalizedId,
        dragSurface ? dragSurface : card
    );
    m_layout->addWidget(card);
    installDragFilters(dragSurface ? dragSurface : card, normalizedId);
}

QStringList ReorderableCardColumn::order() const
{
    QStringList ids;
    for (int index = 0; index < m_layout->count(); ++index) {
        QWidget *widget = m_layout->itemAt(index)->widget();
        for (auto it = m_cards.constBegin(); it != m_cards.constEnd(); ++it) {
            if (it.value() == widget) {
                ids.append(it.key());
                break;
            }
        }
    }
    return ids;
}

bool ReorderableCardColumn::moveCard(int from, int to)
{
    return moveCardInternal(from, to, true);
}

void ReorderableCardColumn::setOrderChangedCallback(
    const std::function<void(const QStringList &)> &callback
)
{
    m_orderChanged = callback;
}

bool ReorderableCardColumn::eventFilter(
    QObject *watched,
    QEvent *event
)
{
    const QString id =
        watched->property(kReorderCardIdProperty).toString();
    if (id.isEmpty()) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_pressedId = id;
            m_pressGlobal = mouse->globalPos();
            m_pressTimer.restart();
            m_dragging = false;
            m_orderChangedDuringDrag = false;
        }
        return false;
    }

    if (event->type() == QEvent::MouseMove && !m_pressedId.isEmpty()) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!(mouse->buttons() & Qt::LeftButton)) {
            finishDrag(false);
            return false;
        }
        const int distance =
            (mouse->globalPos() - m_pressGlobal).manhattanLength();
        if (!m_dragging
            && distance >= QApplication::startDragDistance()
            && m_pressTimer.elapsed() >= 80) {
            m_dragging = true;
            if (QWidget *surface = qobject_cast<QWidget *>(watched)) {
                surface->setCursor(Qt::ClosedHandCursor);
            }
        }
        if (m_dragging) {
            updateDragPosition(mouse->globalPos());
            return true;
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && !m_pressedId.isEmpty()) {
            const bool consumed = m_dragging;
            finishDrag(true);
            return consumed;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool ReorderableCardColumn::moveCardInternal(
    int from,
    int to,
    bool notify
)
{
    const int count = m_layout->count();
    if (from < 0 || to < 0 || from >= count || to >= count || from == to) {
        return false;
    }

    QLayoutItem *item = m_layout->takeAt(from);
    QWidget *widget = item ? item->widget() : nullptr;
    delete item;
    if (!widget) {
        return false;
    }
    m_layout->insertWidget(to, widget);
    m_layout->invalidate();
    m_layout->activate();
    if (notify && m_orderChanged) {
        m_orderChanged(order());
    }
    return true;
}

int ReorderableCardColumn::indexOf(const QString &id) const
{
    QWidget *card = m_cards.value(id, nullptr);
    if (!card) {
        return -1;
    }
    return m_layout->indexOf(card);
}

void ReorderableCardColumn::installDragFilters(
    QWidget *surface,
    const QString &id
)
{
    if (!surface) {
        return;
    }
    surface->setCursor(Qt::OpenHandCursor);
    surface->setToolTip(QString::fromUtf8("按住并上下拖动调整输入顺序"));
    surface->setProperty(kReorderCardIdProperty, id);
    surface->installEventFilter(this);

    const QList<QWidget *> children = surface->findChildren<QWidget *>();
    for (QWidget *child : children) {
        if (qobject_cast<QAbstractButton *>(child)) {
            continue;
        }
        child->setProperty(kReorderCardIdProperty, id);
        child->installEventFilter(this);
    }
}

void ReorderableCardColumn::updateDragPosition(
    const QPoint &globalPosition
)
{
    int current = indexOf(m_pressedId);
    if (current < 0) {
        return;
    }

    const int localY = mapFromGlobal(globalPosition).y();
    if (current > 0) {
        QWidget *previous = m_layout->itemAt(current - 1)->widget();
        if (previous && localY < previous->geometry().center().y()) {
            if (moveCardInternal(current, current - 1, false)) {
                m_orderChangedDuringDrag = true;
            }
            return;
        }
    }
    if (current + 1 < m_layout->count()) {
        QWidget *next = m_layout->itemAt(current + 1)->widget();
        if (next && localY > next->geometry().center().y()) {
            if (moveCardInternal(current, current + 1, false)) {
                m_orderChangedDuringDrag = true;
            }
        }
    }
}

void ReorderableCardColumn::finishDrag(bool notify)
{
    const QString id = m_pressedId;
    m_pressedId.clear();
    m_dragging = false;
    if (QWidget *surface = m_dragSurfaces.value(id, nullptr)) {
        const QList<QWidget *> widgets =
            surface->findChildren<QWidget *>();
        surface->setCursor(Qt::OpenHandCursor);
        for (QWidget *widget : widgets) {
            if (!qobject_cast<QAbstractButton *>(widget)) {
                widget->setCursor(Qt::OpenHandCursor);
            }
        }
    }
    if (notify && m_orderChangedDuringDrag && m_orderChanged) {
        m_orderChanged(order());
    }
    m_orderChangedDuringDrag = false;
}
