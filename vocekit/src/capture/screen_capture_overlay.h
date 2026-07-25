#ifndef VOCEKIT_SCREEN_CAPTURE_OVERLAY_H
#define VOCEKIT_SCREEN_CAPTURE_OVERLAY_H

#include "screenshot_types.h"

#include <QImage>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <functional>

class QFrame;
class QLabel;
class QPainter;
class QPushButton;

class ScreenCaptureOverlay : public QWidget
{
public:
    explicit ScreenCaptureOverlay(QWidget *parent = nullptr);

    bool beginCapture(QString *error = nullptr);
    void setRecognitionStatus(
        const QString &status,
        bool busy,
        bool actionsEnabled
    );
    void setRecognizedText(const QString &text);
    void setActionResult(
        const QString &text,
        const QString &status = QString()
    );
    void setActionError(const QString &message);
    void setFunctionActionTitle(const QString &title);

    std::function<void(const QImage &, const QRect &)> capturedCallback;
    std::function<void(const QString &)> actionRequestedCallback;
    std::function<void()> cancelledCallback;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool captureVirtualDesktop(QString *error);
    void updateSelection(const QPoint &point);
    void positionToolbar();
    void notifySelectionChanged();
    void cancelCapture();
    void resetSelection();
    QImage selectedImage() const;
    QRect globalSelection() const;
    void copySelection();
    void saveSelection();
    void updateCursorForPoint(const QPoint &point);
    void setActionButtonsEnabled(bool enabled);
    void drawSelectionHandles(QPainter *painter);

    QImage m_desktopImage;
    QRect m_virtualGeometry;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    QPoint m_interactionStart;
    QRect m_interactionSelection;
    QRect m_selection;
    bool m_selecting = false;
    bool m_interacting = false;
    bool m_ocrReady = false;
    bool m_busy = false;
    ScreenshotSelectionHandle m_interactionHandle =
        ScreenshotSelectionHandle::None;
    QString m_resultText;
    QFrame *m_toolbar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_functionButton = nullptr;
    QList<QPushButton *> m_actionButtons;
};

#endif // VOCEKIT_SCREEN_CAPTURE_OVERLAY_H
