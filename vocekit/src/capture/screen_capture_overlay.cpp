#include "screen_capture_overlay.h"

#include "screenshot_types.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>

namespace {

QPushButton *captureButton(
    const QString &text,
    bool primary,
    QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumSize(primary ? QSize(76, 36) : QSize(64, 36));
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(primary
        ? QStringLiteral(
            "QPushButton { background:#2563eb; color:#ffffff; border:0;"
            " border-radius:6px; padding:0 14px; font-weight:600; }"
            "QPushButton:hover { background:#1d4ed8; }"
            "QPushButton:disabled { background:#475467; color:#98a2b3; }")
        : QStringLiteral(
            "QPushButton { background:transparent; color:#ffffff;"
            " border:1px solid #475467; border-radius:6px; padding:0 12px;"
            " font-weight:600; }"
            "QPushButton:hover { background:#263244; }"
            "QPushButton:disabled { color:#667085; border-color:#344054; }"));
    return button;
}

QCursor cursorForHandle(ScreenshotSelectionHandle handle)
{
    switch (handle) {
    case ScreenshotSelectionHandle::TopLeft:
    case ScreenshotSelectionHandle::BottomRight:
        return QCursor(Qt::SizeFDiagCursor);
    case ScreenshotSelectionHandle::TopRight:
    case ScreenshotSelectionHandle::BottomLeft:
        return QCursor(Qt::SizeBDiagCursor);
    case ScreenshotSelectionHandle::Top:
    case ScreenshotSelectionHandle::Bottom:
        return QCursor(Qt::SizeVerCursor);
    case ScreenshotSelectionHandle::Left:
    case ScreenshotSelectionHandle::Right:
        return QCursor(Qt::SizeHorCursor);
    case ScreenshotSelectionHandle::Move:
        return QCursor(Qt::SizeAllCursor);
    default:
        return QCursor(Qt::CrossCursor);
    }
}

QString functionActionId()
{
    return QStringLiteral("__currentFunction");
}

}

ScreenCaptureOverlay::ScreenCaptureOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(
        Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
    );
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    m_toolbar = new QFrame(this);
    m_toolbar->setObjectName(QStringLiteral("captureToolbar"));
    m_toolbar->setStyleSheet(QStringLiteral(
        "QFrame#captureToolbar { background:#111827; border:1px solid #344054;"
        " border-radius:7px; }"
    ));
    auto *toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(8);

    m_statusLabel = new QLabel(QStringLiteral("拖动完成后自动识别"), m_toolbar);
    m_statusLabel->setFixedWidth(150);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color:#e4e7ec; font-family:'Microsoft YaHei UI';"
        " font-size:13px; padding:0 6px;"
    ));
    toolbarLayout->addWidget(m_statusLabel);

    m_functionButton = captureButton(QStringLiteral("执行功能"), true, m_toolbar);
    m_functionButton->setMinimumSize(QSize(104, 36));
    m_functionButton->setMaximumWidth(176);
    m_functionButton->hide();
    connect(m_functionButton, &QPushButton::clicked, this, [this]() {
        if (actionRequestedCallback) {
            actionRequestedCallback(functionActionId());
        }
    });
    m_actionButtons.append(m_functionButton);
    toolbarLayout->addWidget(m_functionButton);

    const QVector<QPair<QString, QString>> actionDefinitions = {
        qMakePair(QStringLiteral("organize"), QStringLiteral("智能整理")),
        qMakePair(QStringLiteral("translate"), QStringLiteral("翻译")),
        qMakePair(QStringLiteral("polish"), QStringLiteral("润色")),
        qMakePair(QStringLiteral("summarize"), QStringLiteral("总结"))
    };
    for (const auto &definition : actionDefinitions) {
        auto *button = captureButton(definition.second, true, m_toolbar);
        button->setEnabled(false);
        connect(button, &QPushButton::clicked, this, [this, definition]() {
            if (actionRequestedCallback) {
                actionRequestedCallback(definition.first);
            }
        });
        m_actionButtons.append(button);
        toolbarLayout->addWidget(button);
    }

    auto *copy = captureButton(QStringLiteral("复制图片"), false, m_toolbar);
    auto *save = captureButton(QStringLiteral("保存"), false, m_toolbar);
    auto *reselect = captureButton(QStringLiteral("重选"), false, m_toolbar);
    auto *cancel = captureButton(QStringLiteral("关闭"), false, m_toolbar);
    toolbarLayout->addWidget(copy);
    toolbarLayout->addWidget(save);
    toolbarLayout->addWidget(reselect);
    toolbarLayout->addWidget(cancel);
    m_toolbar->adjustSize();
    m_toolbar->hide();

    connect(copy, &QPushButton::clicked, this, &ScreenCaptureOverlay::copySelection);
    connect(save, &QPushButton::clicked, this, &ScreenCaptureOverlay::saveSelection);
    connect(reselect, &QPushButton::clicked, this, [this]() {
        resetSelection();
    });
    connect(cancel, &QPushButton::clicked, this, [this]() {
        cancelCapture();
    });
}

bool ScreenCaptureOverlay::beginCapture(QString *error)
{
    if (!captureVirtualDesktop(error)) {
        return false;
    }
    setGeometry(m_virtualGeometry);
    show();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    return true;
}

bool ScreenCaptureOverlay::captureVirtualDesktop(QString *error)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        if (error) {
            *error = QStringLiteral("没有找到可截图的显示器。");
        }
        return false;
    }

    QRect virtualGeometry;
    for (QScreen *screen : screens) {
        if (screen) {
            virtualGeometry = virtualGeometry.united(screen->geometry());
        }
    }
    if (!virtualGeometry.isValid()) {
        if (error) {
            *error = QStringLiteral("无法读取桌面显示区域。");
        }
        return false;
    }

    QImage desktop(virtualGeometry.size(), QImage::Format_ARGB32_Premultiplied);
    desktop.fill(Qt::black);
    QPainter painter(&desktop);
    for (QScreen *screen : screens) {
        if (!screen) {
            continue;
        }
        const QRect geometry = screen->geometry();
        const QPixmap snapshot = screen->grabWindow(0);
        if (snapshot.isNull()) {
            continue;
        }
        const QRect target(
            geometry.topLeft() - virtualGeometry.topLeft(),
            geometry.size()
        );
        painter.drawImage(target, snapshot.toImage());
    }
    painter.end();

    m_desktopImage = desktop;
    m_virtualGeometry = virtualGeometry;
    return !m_desktopImage.isNull();
}

void ScreenCaptureOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(rect(), m_desktopImage);
    painter.fillRect(rect(), QColor(0, 0, 0, 118));

    if (!m_selection.isValid()) {
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 11));
        painter.drawText(
            rect().adjusted(24, 24, -24, -24),
            Qt::AlignTop | Qt::AlignHCenter,
            QStringLiteral("按住鼠标左键拖动选择区域，Esc 或右键取消")
        );
        return;
    }

    painter.drawImage(m_selection, m_desktopImage, m_selection);
    if (!m_resultText.trimmed().isEmpty()) {
        painter.save();
        painter.setClipRect(m_selection.adjusted(2, 2, -2, -2));
        painter.fillRect(
            m_selection.adjusted(2, 2, -2, -2),
            QColor(255, 255, 255, 244)
        );
        QFont resultFont(QStringLiteral("Microsoft YaHei UI"), 12);
        resultFont.setStyleStrategy(QFont::PreferAntialias);
        painter.setFont(resultFont);
        painter.setPen(QColor(QStringLiteral("#111827")));
        painter.drawText(
            m_selection.adjusted(18, 14, -18, -14),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            m_resultText
        );
        painter.restore();
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#2563eb")), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_selection.adjusted(0, 0, -1, -1));
    drawSelectionHandles(&painter);

    const QString sizeText = QStringLiteral("%1 × %2")
        .arg(m_selection.width())
        .arg(m_selection.height());
    const QFont font(QStringLiteral("Microsoft YaHei UI"), 9);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const QRect labelRect(
        m_selection.left(),
        qMax(4, m_selection.top() - 30),
        metrics.width(sizeText) + 18,
        25
    );
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#111827")));
    painter.drawRoundedRect(labelRect, 5, 5);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, sizeText);
}

void ScreenCaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        cancelCapture();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_toolbar->hide();
    m_resultText.clear();
    if (m_selection.isValid()) {
        const ScreenshotSelectionHandle handle =
            screenshotSelectionHandleAt(m_selection, event->pos(), 9);
        if (handle != ScreenshotSelectionHandle::None) {
            m_interacting = true;
            m_interactionHandle = handle;
            m_interactionStart = event->pos();
            m_interactionSelection = m_selection;
            return;
        }
    }

    m_selecting = true;
    m_startPoint = event->pos();
    m_currentPoint = event->pos();
    updateSelection(event->pos());
}

void ScreenCaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_interacting) {
        if (m_interactionHandle == ScreenshotSelectionHandle::Move) {
            m_selection = movedScreenshotSelection(
                m_interactionSelection,
                event->pos() - m_interactionStart,
                size()
            );
        } else {
            m_selection = resizedScreenshotSelection(
                m_interactionSelection,
                m_interactionHandle,
                event->pos(),
                size(),
                24
            );
        }
        update();
        return;
    }
    if (!m_selecting) {
        updateCursorForPoint(event->pos());
        return;
    }
    updateSelection(event->pos());
}

void ScreenCaptureOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_interacting) {
        m_interacting = false;
        m_interactionHandle = ScreenshotSelectionHandle::None;
        if (isValidScreenshotSelection(m_selection)) {
            positionToolbar();
            m_toolbar->show();
            m_toolbar->raise();
            notifySelectionChanged();
        }
        updateCursorForPoint(event->pos());
        return;
    }
    if (!m_selecting) {
        return;
    }
    m_selecting = false;
    updateSelection(event->pos());
    if (!isValidScreenshotSelection(m_selection)) {
        resetSelection();
        return;
    }
    positionToolbar();
    m_toolbar->show();
    m_toolbar->raise();
    notifySelectionChanged();
}

void ScreenCaptureOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isValidScreenshotSelection(m_selection)) {
        notifySelectionChanged();
    }
}

void ScreenCaptureOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelCapture();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        notifySelectionChanged();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ScreenCaptureOverlay::updateSelection(const QPoint &point)
{
    m_currentPoint = point;
    m_selection = normalizedScreenshotSelection(
        m_startPoint,
        m_currentPoint,
        size()
    );
    update();
}

void ScreenCaptureOverlay::positionToolbar()
{
    m_toolbar->adjustSize();
    int x = m_selection.right() - m_toolbar->width() + 1;
    int y = m_selection.bottom() + 10;
    x = qBound(6, x, width() - m_toolbar->width() - 6);
    if (y + m_toolbar->height() > height() - 6) {
        y = m_selection.top() - m_toolbar->height() - 10;
    }
    y = qBound(6, y, height() - m_toolbar->height() - 6);
    m_toolbar->move(x, y);
}

void ScreenCaptureOverlay::notifySelectionChanged()
{
    if (!isValidScreenshotSelection(m_selection)) {
        return;
    }
    m_resultText.clear();
    m_ocrReady = false;
    setRecognitionStatus(QStringLiteral("正在识别"), true, false);
    update();
    if (capturedCallback) {
        capturedCallback(selectedImage(), globalSelection());
    }
}

void ScreenCaptureOverlay::cancelCapture()
{
    const auto callback = cancelledCallback;
    close();
    if (callback) {
        callback();
    }
}

void ScreenCaptureOverlay::resetSelection()
{
    m_selecting = false;
    m_interacting = false;
    m_interactionHandle = ScreenshotSelectionHandle::None;
    m_selection = QRect();
    m_resultText.clear();
    m_ocrReady = false;
    m_busy = false;
    m_statusLabel->setText(QStringLiteral("拖动完成后自动识别"));
    setActionButtonsEnabled(false);
    m_toolbar->hide();
    setCursor(Qt::CrossCursor);
    update();
}

void ScreenCaptureOverlay::setRecognitionStatus(
    const QString &status,
    bool busy,
    bool actionsEnabled)
{
    m_busy = busy;
    if (m_statusLabel) {
        m_statusLabel->setText(status);
        m_statusLabel->setToolTip(status);
    }
    setActionButtonsEnabled(actionsEnabled && !busy);
}

void ScreenCaptureOverlay::setRecognizedText(const QString &text)
{
    m_ocrReady = !text.trimmed().isEmpty();
    setRecognitionStatus(
        m_ocrReady ? QStringLiteral("识别完成") : QStringLiteral("未识别到文字"),
        false,
        m_ocrReady
    );
}

void ScreenCaptureOverlay::setActionResult(
    const QString &text,
    const QString &status)
{
    m_resultText = text.trimmed();
    setRecognitionStatus(
        status.trimmed().isEmpty() ? QStringLiteral("处理完成") : status,
        false,
        m_ocrReady
    );
    update();
}

void ScreenCaptureOverlay::setActionError(const QString &message)
{
    setRecognitionStatus(
        message.trimmed().isEmpty() ? QStringLiteral("处理失败") : message,
        false,
        m_ocrReady
    );
}

void ScreenCaptureOverlay::setFunctionActionTitle(const QString &title)
{
    if (!m_functionButton) {
        return;
    }
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        m_functionButton->hide();
        return;
    }

    const QString fullText = QStringLiteral("执行：") + trimmed;
    const QFontMetrics metrics(m_functionButton->font());
    m_functionButton->setText(metrics.elidedText(
        fullText,
        Qt::ElideRight,
        154
    ));
    m_functionButton->setToolTip(fullText);
    m_functionButton->show();
    m_toolbar->adjustSize();
}

QImage ScreenCaptureOverlay::selectedImage() const
{
    return m_selection.isValid()
        ? m_desktopImage.copy(m_selection)
        : QImage();
}

QRect ScreenCaptureOverlay::globalSelection() const
{
    return QRect(
        m_selection.topLeft() + m_virtualGeometry.topLeft(),
        m_selection.size()
    );
}

void ScreenCaptureOverlay::copySelection()
{
    const QImage image = selectedImage();
    if (!image.isNull()) {
        QApplication::clipboard()->setImage(image);
        m_statusLabel->setText(QStringLiteral("截图已复制"));
    }
}

void ScreenCaptureOverlay::saveSelection()
{
    const QImage image = selectedImage();
    if (image.isNull()) {
        return;
    }
    const QString pictures = QStandardPaths::writableLocation(
        QStandardPaths::PicturesLocation
    );
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存截图"),
        QDir(pictures).filePath(QStringLiteral("vocekit-screenshot.png")),
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)")
    );
    if (path.isEmpty()) {
        return;
    }
    if (image.save(path)) {
        m_statusLabel->setText(QStringLiteral("截图已保存"));
    } else {
        m_statusLabel->setText(QStringLiteral("保存失败"));
    }
}

void ScreenCaptureOverlay::updateCursorForPoint(const QPoint &point)
{
    setCursor(cursorForHandle(
        screenshotSelectionHandleAt(m_selection, point, 9)
    ));
}

void ScreenCaptureOverlay::setActionButtonsEnabled(bool enabled)
{
    for (QPushButton *button : m_actionButtons) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

void ScreenCaptureOverlay::drawSelectionHandles(QPainter *painter)
{
    if (!painter || !m_selection.isValid()) {
        return;
    }
    const QVector<QPoint> points = {
        m_selection.topLeft(),
        QPoint(m_selection.center().x(), m_selection.top()),
        m_selection.topRight(),
        QPoint(m_selection.right(), m_selection.center().y()),
        m_selection.bottomRight(),
        QPoint(m_selection.center().x(), m_selection.bottom()),
        m_selection.bottomLeft(),
        QPoint(m_selection.left(), m_selection.center().y())
    };
    painter->setPen(QPen(QColor(QStringLiteral("#ffffff")), 1));
    painter->setBrush(QColor(QStringLiteral("#2563eb")));
    for (const QPoint &point : points) {
        painter->drawEllipse(point, 5, 5);
    }
}
