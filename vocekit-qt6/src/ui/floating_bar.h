#ifndef VOCEKIT_FLOATING_BAR_H
#define VOCEKIT_FLOATING_BAR_H

#include "floating_bar_surface.h"

#include <QPoint>
#include <QWidget>

#include <functional>

struct FloatingBarPositionCallbacks
{
    std::function<bool()> hasSavedPosition;
    std::function<QPoint()> savedPosition;
    std::function<void(const QPoint &)> savePosition;
};

QPoint floatingBarClampedTopLeftToScreen(
    const QPoint &topLeft,
    const QSize &size
);

class FloatingBar : public QWidget
{
public:
    explicit FloatingBar(
        const FloatingBarPositionCallbacks &positionCallbacks =
            FloatingBarPositionCallbacks(),
        QWidget *parent = nullptr
    );
    ~FloatingBar() override;

    void setStyle(const QString &style);
    QString style() const;
    void setActions(const FloatingBarActions &actions);
    void setStage(
        FloatingBarStage stage,
        const QString &title = QString(),
        const QString &detail = QString()
    );

    void setStatus(const QString &title, const QString &subtitle);
    void setStreamingTranscript(
        const QString &committed,
        const QString &provisional
    );
    void setStreamingFinalizing();
    void setStreamingFallback();
    void clearStreamingTranscript();
    void setResult(const QString &title, const QString &result);
    QString lastResult() const;
    void hideLater(int msec = -1);
    void setAutoHideMsec(int msec);
    void setEnabledVisible(bool enabled);
    void setSuppressed(bool suppressed);
    void setWaveformVisible(bool visible);
    void setWaveformLevel(int peak);
    void placeNearBottom();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    bool actionStage() const;
    void applyPendingStyleIfAllowed();
    void rebuildSurface();
    void render();
    void saveCurrentPosition();

    FloatingBarPositionCallbacks m_positionCallbacks;
    FloatingBarSurface *m_surface = nullptr;
    FloatingBarViewState m_state;
    FloatingBarActions m_actions;
    QString m_style;
    QString m_pendingStyle;
    QString m_lastResult;
    bool m_enabled = true;
    bool m_suppressed = false;
    bool m_dragging = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartPosition;
    int m_autoHideMsec = 2000;
    int m_statusGeneration = 0;
};

#endif // VOCEKIT_FLOATING_BAR_H
