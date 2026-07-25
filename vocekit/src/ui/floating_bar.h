#ifndef VOCEKIT_FLOATING_BAR_H
#define VOCEKIT_FLOATING_BAR_H

#include "ui_style.h"

#include <QtWidgets>

#include <cmath>
#include <functional>

enum class FloatingBarStage
{
    Preparing,
    Recording,
    Recognizing,
    ModelProcessing,
    Writing,
    Completed,
    Failed
};

struct FloatingBarPositionCallbacks
{
    std::function<bool()> hasSavedPosition;
    std::function<QPoint()> savedPosition;
    std::function<void(const QPoint &)> savePosition;
};

static QPoint floatingBarClampedTopLeftToScreen(const QPoint &topLeft, const QSize &size)
{
    int screenNumber = QApplication::desktop()->screenNumber(topLeft);
    if (screenNumber < 0) {
        screenNumber = QApplication::desktop()->screenNumber(QCursor::pos());
    }
    const QRect screen = QApplication::desktop()->availableGeometry(screenNumber);
    const int maxX = qMax(screen.left(), screen.right() - size.width() + 1);
    const int maxY = qMax(screen.top(), screen.bottom() - size.height() + 1);
    return QPoint(
        qBound(screen.left(), topLeft.x(), maxX),
        qBound(screen.top(), topLeft.y(), maxY)
    );
}

// 录音波形控件：显示麦克风是否有声音输入，只在录音相关状态下展示。
class WaveformMeter : public QWidget
{
public:
    explicit WaveformMeter(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(126, 34);
        m_levels.fill(0, 16);
    }

    void reset()
    {
        m_levels.fill(0, 16);
        m_displayLevel = 0;
        update();
    }

    void setPeak(int peak)
    {
        const int noiseFloor = 70;
        const int fullScalePeak = 2200;
        const int cleanPeak = qMax(0, peak - noiseFloor);
        int normalized = 0;
        if (cleanPeak > 0) {
            const double ratio = qMin(1.0, cleanPeak / static_cast<double>(fullScalePeak));
            normalized = qBound(0, static_cast<int>(std::sqrt(ratio) * 110.0), 100);
        }

        if (normalized > m_displayLevel) {
            m_displayLevel = (m_displayLevel + normalized * 3) / 4;
        } else {
            m_displayLevel = (m_displayLevel * 3 + normalized) / 4;
        }
        if (!m_levels.isEmpty()) {
            m_levels.removeFirst();
        }
        m_levels.append(m_displayLevel);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(QStringLiteral("#344155")), 1));
        painter.setBrush(QColor(QStringLiteral("#172033")));
        painter.drawRoundedRect(outer, 9, 9);

        const bool active = m_displayLevel > 8;
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#64748b")));
        painter.drawEllipse(QRectF(10, height() / 2.0 - 3.0, 6, 6));

        const int count = m_levels.size();
        const qreal startX = 24;
        const qreal gap = 3;
        const qreal barWidth = 3.8;
        const qreal maxHeight = height() - 12;
        for (int i = 0; i < count; ++i) {
            const int level = m_levels.at(i);
            const qreal heightRatio = level / 100.0;
            const qreal barHeight = qMax<qreal>(5, 5 + heightRatio * (maxHeight - 5));
            const qreal x = startX + i * (barWidth + gap);
            const qreal y = (this->height() - barHeight) / 2.0;

            QColor color = active ? QColor(QStringLiteral("#34d399")) : QColor(QStringLiteral("#516075"));
            if (active && i > count - 5) {
                color = QColor(QStringLiteral("#a7f3d0"));
            }
            color.setAlpha(active ? qBound(130, 110 + level, 245) : 150);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(x, y, barWidth, barHeight), 2, 2);
        }
    }

private:
    QVector<int> m_levels;
    int m_displayLevel = 0;
};

// 浮动条状态点：用颜色和轻微动画提示录音、处理、完成和错误状态。
class FloatingStatusIndicator : public QWidget
{
public:
    explicit FloatingStatusIndicator(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(40, 40);
        m_timer.setInterval(45);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_phase = (m_phase + 1) % 1000;
            update();
        });
    }

    void setRecording(bool recording)
    {
        if (m_recording == recording) {
            return;
        }
        m_recording = recording;
        update();
    }

    void startPulse()
    {
        if (!m_timer.isActive()) {
            m_timer.start();
        }
    }

    void stopPulse()
    {
        m_timer.stop();
        m_phase = 0;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal wave = (std::sin(m_phase / 8.0) + 1.0) / 2.0;
        const QColor base = m_recording
            ? QColor(QStringLiteral("#fb7185"))
            : QColor(QStringLiteral("#60a5fa"));
        const QColor core = m_recording
            ? QColor(QStringLiteral("#f43f5e"))
            : QColor(QStringLiteral("#3b82f6"));

        painter.setPen(Qt::NoPen);
        QColor halo = base;
        halo.setAlpha(m_recording ? 32 + static_cast<int>(wave * 58) : 22 + static_cast<int>(wave * 38));
        const qreal haloRadius = m_recording ? 15.0 + wave * 5.0 : 13.0 + wave * 3.0;
        painter.setBrush(halo);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), haloRadius, haloRadius);

        painter.setBrush(QColor(QStringLiteral("#1f2937")));
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), 15.5, 15.5);

        QColor ring = base;
        ring.setAlpha(m_recording ? 185 : 150);
        painter.setBrush(ring);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), 11.5, 11.5);

        QColor inner = core;
        inner.setAlpha(235);
        const qreal innerRadius = m_recording ? 5.2 + wave * 1.8 : 4.6 + wave * 1.1;
        painter.setBrush(inner);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), innerRadius, innerRadius);

        if (m_recording) {
            QColor shine(QStringLiteral("#ffe4e6"));
            shine.setAlpha(190);
            painter.setBrush(shine);
            painter.drawEllipse(QPointF(width() / 2.0 - 3.0, height() / 2.0 - 3.0), 1.7, 1.7);
        }
    }

private:
    QTimer m_timer;
    int m_phase = 0;
    bool m_recording = false;
};

// 浮动条：后台模式下的临时状态界面，显示录音、识别、处理、错误和快捷操作。
class FloatingBar : public QWidget
{
public:
    explicit FloatingBar(
        const FloatingBarPositionCallbacks &positionCallbacks = FloatingBarPositionCallbacks(),
        QWidget *parent = nullptr)
        : QWidget(parent), m_positionCallbacks(positionCallbacks)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(720, 76);

        auto *root = new QFrame(this);
        root->setObjectName(QStringLiteral("root"));
        root->setGeometry(rect());
        root->setCursor(Qt::OpenHandCursor);
        root->installEventFilter(this);
        root->setStyleSheet(QStringLiteral(
            "QFrame#root {"
            "  background: #111827;"
            "  border: 1px solid #2f3a4a;"
            "  border-radius: 8px;"
            "}"
            "QLabel { color: #f9fafb; }"
        ));

        auto *layout = new QHBoxLayout(root);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(12);

        m_indicator = new FloatingStatusIndicator;

        auto *textBox = new QWidget;
        textBox->setCursor(Qt::OpenHandCursor);
        textBox->installEventFilter(this);
        auto *textLayout = new QVBoxLayout(textBox);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        m_title = new QLabel(QString::fromUtf8("等待快捷键"));
        m_title->setFont(appFont(11, QFont::DemiBold));
        m_title->setCursor(Qt::OpenHandCursor);
        m_title->installEventFilter(this);

        m_subtitle = new QLabel(QString::fromUtf8("按快捷键开始使用"));
        m_subtitle->setFont(appFont(9));
        m_subtitle->setStyleSheet(QStringLiteral("color: #aeb7c5;"));
        m_subtitle->setCursor(Qt::OpenHandCursor);
        m_subtitle->installEventFilter(this);

        textLayout->addWidget(m_title);
        textLayout->addWidget(m_subtitle);

        auto *copy = smallActionButton(QString::fromUtf8("复制"));
        auto *undo = smallActionButton(QString::fromUtf8("撤销"));
        auto *retry = smallActionButton(QString::fromUtf8("重试"));
        connect(copy, &QPushButton::clicked, this, [this]() {
            if (!m_lastResult.isEmpty()) {
                QApplication::clipboard()->setText(m_lastResult);
                setStatus(QString::fromUtf8("已复制"), QString::fromUtf8("结果已复制到剪贴板"));
                hideLater();
            }
        });

        m_waveform = new WaveformMeter;
        m_waveform->setVisible(false);
        m_waveform->installEventFilter(this);
        m_indicator->installEventFilter(this);

        layout->addWidget(m_indicator);
        layout->addWidget(textBox, 1);
        layout->addWidget(m_waveform);
        layout->addWidget(copy);
        layout->addWidget(undo);
        layout->addWidget(retry);
    }

    void setStatus(const QString &title, const QString &subtitle)
    {
        if (!m_enabled || m_suppressed) {
            hide();
            return;
        }
        ++m_statusGeneration;
        m_title->setText(title);
        m_subtitle->setText(subtitle);
        placeNearBottom();
        show();
        raise();
        if (m_indicator) {
            m_indicator->startPulse();
        }
    }

    void setResult(const QString &title, const QString &result)
    {
        m_lastResult = result;
        QString preview = result;
        preview.replace(QStringLiteral("\n"), QStringLiteral(" "));
        if (preview.size() > 42) {
            preview = preview.left(42) + QStringLiteral("...");
        }
        setStatus(title, preview);
    }

    QString lastResult() const
    {
        return m_lastResult;
    }

    void hideLater(int msec = -1)
    {
        const int delay = msec >= 0 ? msec : m_autoHideMsec;
        const int generation = m_statusGeneration;
        QTimer::singleShot(delay, this, [this, generation]() {
            if (generation == m_statusGeneration) {
                hide();
            }
        });
    }

    void setAutoHideMsec(int msec)
    {
        m_autoHideMsec = qBound(1000, msec, 60000);
    }

    void setEnabledVisible(bool enabled)
    {
        m_enabled = enabled;
        if (!m_enabled) {
            ++m_statusGeneration;
            if (m_indicator) {
                m_indicator->stopPulse();
            }
            hide();
        }
    }

    void setSuppressed(bool suppressed)
    {
        m_suppressed = suppressed;
        if (m_suppressed) {
            ++m_statusGeneration;
            if (m_indicator) {
                m_indicator->stopPulse();
            }
            hide();
        }
    }

    void setWaveformVisible(bool visible)
    {
        if (m_indicator) {
            m_indicator->setRecording(visible);
            if (visible) {
                m_indicator->startPulse();
            }
        }
        if (m_waveform) {
            m_waveform->setVisible(visible);
            if (!visible) {
                m_waveform->reset();
            }
        }
    }

    void setWaveformLevel(int peak)
    {
        if (!m_waveform || !m_waveform->isVisible()) {
            return;
        }
        m_waveform->setPeak(peak);
    }

    void placeNearBottom()
    {
        if (m_positionCallbacks.hasSavedPosition
            && m_positionCallbacks.savedPosition
            && m_positionCallbacks.hasSavedPosition()) {
            move(floatingBarClampedTopLeftToScreen(
                m_positionCallbacks.savedPosition(),
                size()
            ));
            return;
        }
        const QRect screen = QApplication::desktop()->availableGeometry();
        move(screen.center().x() - width() / 2, screen.bottom() - height() - 28);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragStartGlobal = mouse->globalPos();
                m_dragStartPosition = pos();
                setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(floatingBarClampedTopLeftToScreen(
                m_dragStartPosition + (mouse->globalPos() - m_dragStartGlobal),
                size()
            ));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = false;
                setCursor(Qt::ArrowCursor);
                saveCurrentPosition();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void hideEvent(QHideEvent *event) override
    {
        if (m_indicator) {
            m_indicator->stopPulse();
            m_indicator->setRecording(false);
        }
        QWidget::hideEvent(event);
    }

private:
    QPushButton *smallActionButton(const QString &text)
    {
        auto *button = new QPushButton(text);
        button->setFixedHeight(32);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: #263244;"
            "  color: #f9fafb;"
            "  border: 1px solid #3b4658;"
            "  border-radius: 6px;"
            "  padding: 0 10px;"
            "}"
            "QPushButton:hover { background: #344155; }"
        ));
        return button;
    }

    void saveCurrentPosition()
    {
        if (m_positionCallbacks.savePosition) {
            m_positionCallbacks.savePosition(pos());
        }
    }

    FloatingBarPositionCallbacks m_positionCallbacks;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    FloatingStatusIndicator *m_indicator = nullptr;
    WaveformMeter *m_waveform = nullptr;
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
