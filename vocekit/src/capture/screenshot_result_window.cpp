#include "screenshot_result_window.h"

#include "../providers/model_catalog.h"
#include "screenshot_types.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTabBar>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

class ScreenshotImageCanvas : public QWidget
{
public:
    explicit ScreenshotImageCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(340, 260);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setContent(
        const QImage &image,
        const QVector<OcrTextBlock> &blocks,
        const QStringList &overlayLines,
        bool showOverlay)
    {
        m_image = image;
        m_blocks = blocks;
        m_overlayLines = overlayLines;
        m_showOverlay = showOverlay;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#eef0f4")));
        if (m_image.isNull()) {
            painter.setPen(QColor(QStringLiteral("#667085")));
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("没有截图"));
            return;
        }

        QSize drawSize = m_image.size();
        drawSize.scale(size() - QSize(24, 24), Qt::KeepAspectRatio);
        const QRect imageRect(
            QPoint(
                (width() - drawSize.width()) / 2,
                (height() - drawSize.height()) / 2
            ),
            drawSize
        );
        painter.drawImage(imageRect, m_image);
        if (!m_showOverlay
            || m_overlayLines.size() != m_blocks.size()
            || m_image.width() <= 0
            || m_image.height() <= 0) {
            return;
        }

        const double scaleX = double(imageRect.width()) / m_image.width();
        const double scaleY = double(imageRect.height()) / m_image.height();
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int index = 0; index < m_blocks.size(); ++index) {
            const QRect source = m_blocks.at(index).boundingRect();
            if (!source.isValid()) {
                continue;
            }
            QRect target(
                imageRect.left() + qRound(source.left() * scaleX),
                imageRect.top() + qRound(source.top() * scaleY),
                qMax(18, qRound(source.width() * scaleX)),
                qMax(18, qRound(source.height() * scaleY))
            );
            target = target.intersected(imageRect);
            if (!target.isValid()) {
                continue;
            }
            painter.fillRect(target, QColor(255, 255, 255, 228));
            painter.setPen(QPen(QColor(QStringLiteral("#2563eb")), 1));
            painter.drawRect(target.adjusted(0, 0, -1, -1));
            QFont font(QStringLiteral("Microsoft YaHei UI"));
            font.setPixelSize(qBound(11, int(target.height() * 0.52), 24));
            painter.setFont(font);
            painter.setPen(QColor(QStringLiteral("#111827")));
            painter.drawText(
                target.adjusted(4, 2, -4, -2),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                m_overlayLines.at(index)
            );
        }
    }

private:
    QImage m_image;
    QVector<OcrTextBlock> m_blocks;
    QStringList m_overlayLines;
    bool m_showOverlay = false;
};

ScreenshotResultWindow::ScreenshotResultWindow(
    const QString &title,
    const QImage &image,
    const QVector<OcrTextBlock> &blocks,
    const QString &recognizedText,
    const QString &resultText,
    const QRect &savedGeometry,
    int opacityPercent,
    int autoCloseMsec,
    QWidget *parent)
    : QWidget(parent),
      m_image(image),
      m_blocks(blocks),
      m_recognizedText(recognizedText),
      m_result(resultText),
      m_initialResult(resultText),
      m_savedGeometry(savedGeometry),
      m_opacityPercent(qBound(30, opacityPercent, 100)),
      m_autoCloseMsec(qMax(0, autoCloseMsec))
{
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);
    setMinimumSize(760, 520);
    resize(920, 620);
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    setObjectName(QStringLiteral("screenshotResultWindow"));
    setStyleSheet(QStringLiteral(
        "QWidget#screenshotResultWindow { background:#ffffff; }"
        "QLabel { color:#111827; }"
        "QTabBar::tab { min-height:34px; padding:0 18px; color:#475467;"
        " background:#f2f4f7; border:1px solid #d0d5dd; }"
        "QTabBar::tab:selected { color:#111827; background:#ffffff;"
        " font-weight:600; }"
        "QTextEdit { background:#f9fafb; border:1px solid #d0d5dd;"
        " border-radius:6px; padding:10px; }"
    ));
    setWindowOpacity(m_opacityPercent / 100.0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *heading = new QLabel(title);
    heading->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 14, QFont::DemiBold));
    m_statusLabel = new QLabel(QStringLiteral("已生成"));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#667085;"));
    header->addWidget(heading);
    header->addStretch();
    header->addWidget(m_statusLabel);
    layout->addLayout(header);

    auto *viewRow = new QHBoxLayout;
    m_viewTabs = new QTabBar;
    m_viewTabs->setExpanding(false);
    m_viewTabs->addTab(QStringLiteral("原图"));
    m_viewTabs->addTab(QStringLiteral("译文覆盖"));
    m_viewTabs->addTab(QStringLiteral("双语对照"));
    m_viewTabs->setCurrentIndex(2);
    auto *opacityLabel = new QLabel(QStringLiteral("透明度"));
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(30, 100);
    m_opacitySlider->setValue(m_opacityPercent);
    m_opacitySlider->setFixedWidth(150);
    auto *opacityValue = new QLabel(QString::number(m_opacityPercent) + QStringLiteral("%"));
    opacityValue->setMinimumWidth(42);
    viewRow->addWidget(m_viewTabs);
    viewRow->addStretch();
    viewRow->addWidget(opacityLabel);
    viewRow->addWidget(m_opacitySlider);
    viewRow->addWidget(opacityValue);
    layout->addLayout(viewRow);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_canvas = new ScreenshotImageCanvas;
    m_editor = new QTextEdit;
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setPlainText(resultText);
    m_splitter->addWidget(m_canvas);
    m_splitter->addWidget(m_editor);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    layout->addWidget(m_splitter, 1);

    auto *advanced = new QHBoxLayout;
    advanced->addStretch();
    m_regenerateButton = actionButton(QStringLiteral("重新生成"), false);
    m_retryModelButton = actionButton(QStringLiteral("换模型"), false);
    m_followUpButton = actionButton(QStringLiteral("继续追问"), false);
    advanced->addWidget(m_regenerateButton);
    advanced->addWidget(m_retryModelButton);
    advanced->addWidget(m_followUpButton);
    layout->addLayout(advanced);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    m_copyButton = actionButton(QStringLiteral("复制"), false);
    m_writeButton = actionButton(QStringLiteral("写入"), true);
    m_replaceButton = actionButton(QStringLiteral("替换选中"), false);
    auto *closeButton = actionButton(QStringLiteral("关闭"), false);
    m_copyButton->setObjectName(
        QStringLiteral("screenshotAction_copy")
    );
    m_writeButton->setObjectName(
        QStringLiteral("screenshotAction_write")
    );
    m_replaceButton->setObjectName(
        QStringLiteral("screenshotAction_replace")
    );
    closeButton->setObjectName(
        QStringLiteral("screenshotAction_close")
    );
    actions->addWidget(m_copyButton);
    actions->addWidget(m_writeButton);
    actions->addWidget(m_replaceButton);
    actions->addWidget(closeButton);
    layout->addLayout(actions);

    connect(m_viewTabs, &QTabBar::currentChanged, this, [this](int index) {
        applyViewMode(index);
    });
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this, opacityValue](int value) {
        m_opacityPercent = value;
        setWindowOpacity(value / 100.0);
        opacityValue->setText(QString::number(value) + QStringLiteral("%"));
    });
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        if (m_editor) {
            m_result = m_editor->toPlainText();
        }
        if (!m_programmaticChange && !m_busy) {
            m_userEdited = true;
            if (m_onLiveDraft) {
                m_onLiveDraft(m_result);
            }
        }
        updateOverlay();
        updateActionState();
    });
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(this->resultText());
        m_statusLabel->setText(QStringLiteral("已复制"));
    });
    connect(m_writeButton, &QPushButton::clicked, this, [this]() {
        if (m_checkedWrite) {
            const ClipboardWriteResult result = m_checkedWrite(
                QStringLiteral("write"),
                this->resultText()
            );
            if (!result.ok) {
                setBusy(false);
                m_statusLabel->setText(
                    QStringLiteral("写入失败")
                    + (result.errorCode.trimmed().isEmpty()
                        ? QString()
                        : QStringLiteral("（")
                            + result.errorCode
                            + QStringLiteral("）"))
                );
                return;
            }
            close();
            return;
        }
        if (m_onWrite) {
            m_onWrite(this->resultText());
            close();
        }
    });
    connect(m_replaceButton, &QPushButton::clicked, this, [this]() {
        if (m_checkedWrite) {
            const ClipboardWriteResult result = m_checkedWrite(
                QStringLiteral("replace"),
                this->resultText()
            );
            if (!result.ok) {
                setBusy(false);
                m_statusLabel->setText(
                    QStringLiteral("替换失败")
                    + (result.errorCode.trimmed().isEmpty()
                        ? QString()
                        : QStringLiteral("（")
                            + result.errorCode
                            + QStringLiteral("）"))
                );
                return;
            }
            close();
            return;
        }
        if (m_onReplace) {
            m_onReplace(this->resultText());
            close();
        }
    });
    connect(m_regenerateButton, &QPushButton::clicked, this, [this]() {
        if (m_onRegenerate) {
            m_onRegenerate();
        }
    });
    connect(m_retryModelButton, &QPushButton::clicked, this, [this]() {
        chooseModelAndRetry();
    });
    connect(m_followUpButton, &QPushButton::clicked, this, [this]() {
        askFollowUp();
    });
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);

    updateOverlay();
    applyViewMode(2);
    updateActionState();
}

void ScreenshotResultWindow::setCurrentModel(const QString &modelId)
{
    m_currentModel = modelId;
}

void ScreenshotResultWindow::setModelOptions(
    const QVector<QPair<QString, QString>> &modelOptions)
{
    m_modelOptions = modelOptions;
    updateActionState();
}

void ScreenshotResultWindow::setResultText(
    const QString &text,
    bool resetDraftState)
{
    m_result = text;
    m_programmaticChange = true;
    m_editor->setPlainText(text);
    m_editor->moveCursor(QTextCursor::End);
    m_programmaticChange = false;
    if (resetDraftState) {
        m_initialResult = text;
        m_userEdited = false;
        m_draftSaved = false;
    }
    updateOverlay();
    updateActionState();
}

void ScreenshotResultWindow::appendResultText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    m_programmaticChange = true;
    m_editor->moveCursor(QTextCursor::End);
    m_editor->insertPlainText(text);
    m_editor->moveCursor(QTextCursor::End);
    m_programmaticChange = false;
    m_result = m_editor->toPlainText();
    m_initialResult = m_result;
    updateOverlay();
    updateActionState();
}

QString ScreenshotResultWindow::resultText() const
{
    return m_editor ? m_editor->toPlainText() : m_result;
}

void ScreenshotResultWindow::setBusy(bool busy, const QString &status)
{
    m_busy = busy;
    m_editor->setReadOnly(busy);
    m_statusLabel->setText(status.trimmed().isEmpty()
        ? (busy ? QStringLiteral("正在生成") : QStringLiteral("已生成"))
        : status);
    updateActionState();
}

void ScreenshotResultWindow::showNearBottom()
{
    const QRect screen = QApplication::desktop()->availableGeometry();
    if (m_savedGeometry.isValid()) {
        resize(m_savedGeometry.size().expandedTo(minimumSize()));
        QPoint topLeft = m_savedGeometry.topLeft();
        topLeft.setX(qBound(screen.left(), topLeft.x(), screen.right() - width()));
        topLeft.setY(qBound(screen.top(), topLeft.y(), screen.bottom() - height()));
        move(topLeft);
    } else {
        resize(
            qMin(980, screen.width() - 80),
            qMin(680, screen.height() - 100)
        );
        move(
            screen.center().x() - width() / 2,
            screen.bottom() - height() - 50
        );
    }
    show();
    raise();
    activateWindow();
    if (m_autoCloseMsec > 0) {
        QTimer::singleShot(m_autoCloseMsec, this, &QWidget::close);
    }
}

void ScreenshotResultWindow::setActionCallbacks(
    const std::function<void()> &onRegenerate,
    const std::function<void(const QString &)> &onRetryModel,
    const std::function<void(const QString &)> &onFollowUp,
    const std::function<void(const QString &)> &onWrite,
    const std::function<void(const QString &)> &onReplace)
{
    m_onRegenerate = onRegenerate;
    m_onRetryModel = onRetryModel;
    m_onFollowUp = onFollowUp;
    m_onWrite = onWrite;
    m_onReplace = onReplace;
    updateActionState();
}

void ScreenshotResultWindow::setCheckedWriteCallback(
    const std::function<ClipboardWriteResult(
        const QString &,
        const QString &
    )> &callback)
{
    m_checkedWrite = callback;
    updateActionState();
}

void ScreenshotResultWindow::setDraftCallback(
    const std::function<void(const QString &)> &onDraft)
{
    m_onDraft = onDraft;
}

void ScreenshotResultWindow::setLiveDraftCallback(
    const std::function<void(const QString &)> &onLiveDraft)
{
    m_onLiveDraft = onLiveDraft;
}

void ScreenshotResultWindow::setResolvedCallback(
    const std::function<void()> &onResolved)
{
    m_onResolved = onResolved;
}

void ScreenshotResultWindow::setWindowPreferenceCallback(
    const std::function<void(const QRect &, int)> &onChanged)
{
    m_onWindowPreferenceChanged = onChanged;
}

void ScreenshotResultWindow::closeEvent(QCloseEvent *event)
{
    if (m_busy && !QCoreApplication::closingDown()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("生成尚未结束，请稍候再关闭"));
        }
        event->ignore();
        return;
    }
    saveDraftIfNeeded();
    saveWindowPreference();
    if (m_onResolved) {
        m_onResolved();
    }
    QWidget::closeEvent(event);
}

QPushButton *ScreenshotResultWindow::actionButton(
    const QString &text,
    bool primary)
{
    auto *button = new QPushButton(text);
    button->setMinimumSize(72, 40);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(primary
        ? QStringLiteral(
            "QPushButton { background:#111827; color:#ffffff; border:0;"
            " border-radius:6px; padding:0 14px; font-weight:600; }"
            "QPushButton:hover { background:#263244; }"
            "QPushButton:disabled { background:#d0d5dd; color:#ffffff; }")
        : QStringLiteral(
            "QPushButton { background:#ffffff; color:#111827;"
            " border:1px solid #d0d5dd; border-radius:6px; padding:0 12px;"
            " font-weight:600; }"
            "QPushButton:hover { background:#f3f4f6; }"
            "QPushButton:disabled { color:#98a2b3; background:#f2f4f7; }"));
    return button;
}

void ScreenshotResultWindow::applyViewMode(int index)
{
    if (!m_canvas || !m_editor) {
        return;
    }
    const bool compare = index == 2;
    m_editor->setVisible(compare);
    updateOverlay();
}

void ScreenshotResultWindow::updateOverlay()
{
    if (!m_canvas || !m_viewTabs) {
        return;
    }
    const QStringList lines = mapScreenshotResultLines(
        resultText(),
        m_blocks.size()
    );
    const bool requestedOverlay = m_viewTabs->currentIndex() == 1;
    if (requestedOverlay && lines.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("无法逐块对应，已显示双语对照"));
        m_viewTabs->blockSignals(true);
        m_viewTabs->setCurrentIndex(2);
        m_viewTabs->blockSignals(false);
        m_editor->setVisible(true);
        m_canvas->setContent(m_image, m_blocks, QStringList(), false);
        return;
    }
    m_canvas->setContent(
        m_image,
        m_blocks,
        lines,
        requestedOverlay
    );
}

void ScreenshotResultWindow::updateActionState()
{
    const bool hasResult = !resultText().trimmed().isEmpty();
    m_copyButton->setEnabled(!m_busy && hasResult);
    m_writeButton->setEnabled(
        !m_busy
        && hasResult
        && (bool(m_checkedWrite) || bool(m_onWrite))
    );
    m_replaceButton->setEnabled(
        !m_busy
        && hasResult
        && (bool(m_checkedWrite) || bool(m_onReplace))
    );
    m_regenerateButton->setEnabled(!m_busy && bool(m_onRegenerate));
    m_retryModelButton->setEnabled(
        !m_busy && bool(m_onRetryModel) && !m_modelOptions.isEmpty()
    );
    m_followUpButton->setEnabled(!m_busy && bool(m_onFollowUp));
}

void ScreenshotResultWindow::chooseModelAndRetry()
{
    if (!m_onRetryModel || m_modelOptions.isEmpty()) {
        return;
    }
    QStringList titles;
    int currentIndex = 0;
    const QString displayedModelId = normalizeModelId(
        m_currentModel,
        m_currentModel
    );
    for (int index = 0; index < m_modelOptions.size(); ++index) {
        titles.append(m_modelOptions.at(index).second);
        if (m_modelOptions.at(index).first == displayedModelId) {
            currentIndex = index;
        }
    }
    QInputDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("换模型重试"));
    dialog.setLabelText(QStringLiteral("模型"));
    dialog.setComboBoxItems(titles);
    dialog.setComboBoxEditable(false);
    QComboBox *models = dialog.findChild<QComboBox *>();
    if (!models) {
        return;
    }
    models->setCurrentIndex(currentIndex);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const int selectedIndex = models->currentIndex();
    if (selectedIndex >= 0
        && selectedIndex < m_modelOptions.size()) {
        m_onRetryModel(m_modelOptions.at(selectedIndex).first);
    }
}

void ScreenshotResultWindow::askFollowUp()
{
    if (!m_onFollowUp) {
        return;
    }
    bool accepted = false;
    const QString followUp = QInputDialog::getMultiLineText(
        this,
        QStringLiteral("继续追问"),
        QStringLiteral("补充要求或问题"),
        QString(),
        &accepted
    ).trimmed();
    if (accepted && !followUp.isEmpty()) {
        m_onFollowUp(followUp);
    }
}

void ScreenshotResultWindow::saveDraftIfNeeded()
{
    const QString current = resultText();
    if (!m_userEdited
        || m_draftSaved
        || m_busy
        || current.trimmed().isEmpty()
        || current == m_initialResult
        || !m_onDraft) {
        return;
    }
    m_draftSaved = true;
    m_onDraft(current);
}

void ScreenshotResultWindow::saveWindowPreference()
{
    if (m_onWindowPreferenceChanged) {
        m_onWindowPreferenceChanged(QRect(pos(), size()), m_opacityPercent);
    }
}
