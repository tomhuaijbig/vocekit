#include "mode_card_frame.h"

#include "ui_style.h"

#include <QtWidgets>

ModeCardFrame::ModeCardFrame(const QString &id, QWidget *parent)
    : QFrame(parent), m_id(id)
{
    setAcceptDrops(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QString::fromUtf8("双击编辑，拖动调整顺序"));
}

void ModeCardFrame::setDropCallback(
    const std::function<void(const QString &, const QString &, bool)> &callback)
{
    m_dropCallback = callback;
}

void ModeCardFrame::setDoubleClickCallback(const std::function<void()> &callback)
{
    m_doubleClickCallback = callback;
}

void ModeCardFrame::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QFrame::mousePressEvent(event);
}

void ModeCardFrame::mouseReleaseEvent(QMouseEvent *event)
{
    setCursor(Qt::OpenHandCursor);
    QFrame::mouseReleaseEvent(event);
}

void ModeCardFrame::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_doubleClickCallback) {
        m_doubleClickCallback();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

void ModeCardFrame::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength()
        < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setData(mimeType(), m_id.toUtf8());
    drag->setMimeData(mime);

    QPixmap pixmap = grab();
    if (!pixmap.isNull()) {
        QPixmap transparent(pixmap.size());
        transparent.fill(Qt::transparent);
        QPainter painter(&transparent);
        painter.setOpacity(0.82);
        painter.drawPixmap(0, 0, pixmap);
        painter.end();
        drag->setPixmap(transparent);
        drag->setHotSpot(event->pos());
    }

    drag->exec(Qt::MoveAction);
    setCursor(Qt::OpenHandCursor);
}

void ModeCardFrame::dragEnterEvent(QDragEnterEvent *event)
{
    if (acceptsEvent(event)) {
        setDropHighlighted(true);
        event->acceptProposedAction();
        return;
    }
    QFrame::dragEnterEvent(event);
}

void ModeCardFrame::dragMoveEvent(QDragMoveEvent *event)
{
    if (acceptsEvent(event)) {
        event->acceptProposedAction();
        return;
    }
    QFrame::dragMoveEvent(event);
}

void ModeCardFrame::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropHighlighted(false);
    QFrame::dragLeaveEvent(event);
}

void ModeCardFrame::dropEvent(QDropEvent *event)
{
    setDropHighlighted(false);
    if (!acceptsEvent(event)) {
        QFrame::dropEvent(event);
        return;
    }

    const QString sourceId = QString::fromUtf8(event->mimeData()->data(mimeType()));
    const bool dropAfter =
        event->pos().y() > height() / 2 || event->pos().x() > width() / 2;
    if (m_dropCallback) {
        m_dropCallback(sourceId, m_id, dropAfter);
    }
    event->acceptProposedAction();
}

QString ModeCardFrame::mimeType()
{
    return QStringLiteral("application/x-voiceassistant-function-id");
}

bool ModeCardFrame::acceptsEvent(const QDropEvent *event) const
{
    if (!event || !event->mimeData()->hasFormat(mimeType())) {
        return false;
    }
    const QString sourceId = QString::fromUtf8(event->mimeData()->data(mimeType()));
    return !sourceId.isEmpty() && sourceId != m_id;
}

void ModeCardFrame::setDropHighlighted(bool highlighted)
{
    setStyleSheet(cardStyle() + (highlighted
        ? QStringLiteral("QFrame#card { border: 2px solid #2563eb; background: #f8fbff; }")
        : QString()));
}
