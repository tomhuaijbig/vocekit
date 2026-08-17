#include "floating_bar.h"

#include "../config/app_settings_defaults.h"
#include "screen_position.h"

#include <QApplication>
#include <QCursor>
#include <QHideEvent>
#include <QMouseEvent>
#include <QTimer>

QPoint floatingBarClampedTopLeftToScreen(
    const QPoint &topLeft,
    const QSize &size)
{
    const QRect screen = availableScreenGeometryAt(topLeft);
    if (!screen.isValid()) {
        return topLeft;
    }
    return QPoint(
        qBound(screen.left(), topLeft.x(),
               qMax(screen.left(), screen.right() - size.width() + 1)),
        qBound(screen.top(), topLeft.y(),
               qMax(screen.top(), screen.bottom() - size.height() + 1))
    );
}

FloatingBar::FloatingBar(
    const FloatingBarPositionCallbacks &positionCallbacks,
    QWidget *parent)
    : QWidget(parent),
      m_positionCallbacks(positionCallbacks),
      m_style(floatingBarStyleStatusPill())
{
    setWindowFlags(
        Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
        | Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    rebuildSurface();
}

FloatingBar::~FloatingBar()
{
    if (m_surface) {
        m_surface->render(m_state, FloatingBarActions());
    }
}

void FloatingBar::setStyle(const QString &style)
{
    const QString normalized = normalizeGlobalFloatingBarStyle(style);
    if (actionStage()
        && (m_state.cancelEnabled
            || m_state.confirmEnabled
            || m_state.waveformVisible)) {
        m_pendingStyle = normalized;
        return;
    }
    if (normalized == m_style && m_surface) return;
    m_style = normalized;
    m_pendingStyle.clear();
    rebuildSurface();
}

QString FloatingBar::style() const { return m_style; }

void FloatingBar::setActions(const FloatingBarActions &actions)
{
    m_actions = actions;
    m_state.cancelEnabled = bool(actions.cancel);
    m_state.confirmEnabled = bool(actions.confirm);
    render();
}

void FloatingBar::setStage(
    FloatingBarStage stage,
    const QString &title,
    const QString &detail)
{
    m_state.stage = stage;
    m_state.title = title;
    m_state.detail = detail;
    if (!actionStage()) {
        m_actions = FloatingBarActions();
        m_state.cancelEnabled = false;
        m_state.confirmEnabled = false;
        m_state.waveformVisible = false;
        applyPendingStyleIfAllowed();
    }
    render();
}

void FloatingBar::setStatus(const QString &title, const QString &subtitle)
{
    if (!m_enabled || m_suppressed) { hide(); return; }
    ++m_statusGeneration;
    m_state.title = title;
    m_state.detail = subtitle;
    render();
    placeNearBottom();
    show();
    raise();
}

void FloatingBar::setStreamingTranscript(
    const QString &committed,
    const QString &provisional)
{
    m_state.stage = FloatingBarStage::Streaming;
    m_state.committedText = committed;
    m_state.provisionalText = provisional;
    render();
}

void FloatingBar::setStreamingFinalizing()
{
    setStage(
        FloatingBarStage::StreamingFinalizing,
        QString::fromUtf8("正在完成识别"),
        QString::fromUtf8("正在等待最终文字")
    );
    setStatus(m_state.title, m_state.detail);
}

void FloatingBar::setStreamingFallback()
{
    setStage(
        FloatingBarStage::StreamingFallback,
        QString::fromUtf8("已切换整段识别"),
        QString::fromUtf8("将使用录音结束后的整段识别")
    );
    setStatus(m_state.title, m_state.detail);
}

void FloatingBar::clearStreamingTranscript()
{
    m_state.committedText.clear();
    m_state.provisionalText.clear();
    render();
}

void FloatingBar::setResult(const QString &title, const QString &result)
{
    m_lastResult = result;
    QString preview = result;
    preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (preview.size() > 80) preview = preview.left(80) + QStringLiteral("...");
    setStage(FloatingBarStage::Completed, title, preview);
    setStatus(title, preview);
}

QString FloatingBar::lastResult() const { return m_lastResult; }

void FloatingBar::hideLater(int msec)
{
    const int delay = msec >= 0 ? msec : m_autoHideMsec;
    const int generation = m_statusGeneration;
    QTimer::singleShot(delay, this, [this, generation]() {
        if (generation == m_statusGeneration) hide();
    });
}

void FloatingBar::setAutoHideMsec(int msec)
{
    m_autoHideMsec = qBound(1000, msec, 60000);
}

void FloatingBar::setEnabledVisible(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) { ++m_statusGeneration; hide(); }
}

void FloatingBar::setSuppressed(bool suppressed)
{
    m_suppressed = suppressed;
    if (suppressed) { ++m_statusGeneration; hide(); }
}

void FloatingBar::setWaveformVisible(bool visible)
{
    m_state.waveformVisible = visible;
    if (visible && m_state.stage != FloatingBarStage::Streaming) {
        m_state.stage = FloatingBarStage::Recording;
    }
    if (!visible) m_state.waveformPeak = 0;
    render();
}

void FloatingBar::setWaveformLevel(int peak)
{
    if (!m_state.waveformVisible) return;
    m_state.waveformPeak = peak;
    render();
}

void FloatingBar::placeNearBottom()
{
    if (m_positionCallbacks.hasSavedPosition
        && m_positionCallbacks.savedPosition
        && m_positionCallbacks.hasSavedPosition()) {
        move(floatingBarClampedTopLeftToScreen(
            m_positionCallbacks.savedPosition(), size()));
        return;
    }
    const QRect screen = primaryAvailableScreenGeometry();
    move(screen.center().x() - width() / 2,
         screen.bottom() - height() - 28);
}

bool FloatingBar::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartGlobal = mouse->globalPosition().toPoint();
            m_dragStartPosition = pos();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_dragging) {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        move(floatingBarClampedTopLeftToScreen(
            m_dragStartPosition
                + mouse->globalPosition().toPoint()
                - m_dragStartGlobal,
            size()));
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
        m_dragging = false;
        saveCurrentPosition();
        return true;
    }
    return QWidget::eventFilter(m_surface, event);
}

void FloatingBar::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

bool FloatingBar::actionStage() const
{
    return m_state.stage == FloatingBarStage::Preparing
        || m_state.stage == FloatingBarStage::Recording
        || m_state.stage == FloatingBarStage::Streaming;
}

void FloatingBar::applyPendingStyleIfAllowed()
{
    if (m_pendingStyle.isEmpty() || actionStage()) return;
    if (m_pendingStyle != m_style) {
        m_style = m_pendingStyle;
        m_pendingStyle.clear();
        rebuildSurface();
    } else {
        m_pendingStyle.clear();
    }
}

void FloatingBar::rebuildSurface()
{
    FloatingBarSurface *old = m_surface;
    if (old) {
        old->render(m_state, FloatingBarActions());
        old->removeEventFilter(this);
        old->hide();
        old->setParent(nullptr);
        old->deleteLater();
    }
    m_surface = createFloatingBarSurface(m_style, this);
    m_surface->installEventFilter(this);
    render();
}

void FloatingBar::render()
{
    if (!m_surface) return;
    m_surface->render(m_state, m_actions);
    m_surface->move(0, 0);
    resize(m_surface->size());
    if (isVisible()) placeNearBottom();
}

void FloatingBar::saveCurrentPosition()
{
    if (m_positionCallbacks.savePosition) {
        m_positionCallbacks.savePosition(pos());
    }
}
