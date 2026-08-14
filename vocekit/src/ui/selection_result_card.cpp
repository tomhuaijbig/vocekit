#include "selection_result_card.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

Qt::WindowFlags passiveCardFlags()
{
    return Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
        | Qt::WindowDoesNotAcceptFocus;
}

Qt::WindowFlags focusedCardFlags()
{
    return Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint;
}

int minimumButtonHeight(const QWidget *widget)
{
    return qMax(40, widget->fontMetrics().height() + 16);
}

} // namespace

SelectionResultCard::SelectionResultCard(QWidget *parent)
    : QWidget(parent, passiveCardFlags())
{
    setObjectName(QStringLiteral("selectionResultCard"));
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(QString::fromUtf8(
        "QWidget#selectionResultCard { background: #ffffff;"
        " border: 1px solid #d0d5dd; border-radius: 12px; }"
        "QLabel#selectionResultStatus { color: #667085; }"
        "QPlainTextEdit { background: #f8fafc; border: 1px solid #e4e7ec;"
        " border-radius: 8px; padding: 8px; color: #101828; }"
        "QPushButton, QToolButton { min-width: 0px; padding: 0px 10px;"
        " border: 1px solid #d0d5dd; border-radius: 8px;"
        " background: #ffffff; color: #101828; }"
        "QPushButton:hover, QToolButton:hover { background: #eef2ff; }"
        "QPushButton:disabled, QToolButton:disabled { color: #98a2b3; }"
    ));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    QHBoxLayout *header = new QHBoxLayout;
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("selectionResultTitle"));
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    header->addWidget(m_title, 1);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("selectionResultStatus"));
    header->addWidget(m_status);
    root->addLayout(header);

    m_committed = new QPlainTextEdit(this);
    m_committed->setObjectName(QStringLiteral("selectionResultCommittedText"));
    m_committed->setReadOnly(true);
    m_committed->setMinimumHeight(120);
    m_committed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_committed, 3);

    m_provisional = new QPlainTextEdit(this);
    m_provisional->setObjectName(QStringLiteral("selectionResultProvisionalText"));
    m_provisional->setReadOnly(true);
    m_provisional->setMinimumHeight(64);
    m_provisional->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QPalette provisionalPalette = m_provisional->palette();
    provisionalPalette.setColor(QPalette::Text, QColor(QStringLiteral("#2563eb")));
    m_provisional->setPalette(provisionalPalette);
    root->addWidget(m_provisional, 1);

    m_longTextConfirmation = new QWidget(this);
    m_longTextConfirmation->setObjectName(
        QStringLiteral("selectionLongTextConfirmation")
    );
    QHBoxLayout *confirmationLayout = new QHBoxLayout(m_longTextConfirmation);
    confirmationLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *confirmation = new QLabel(
        QString::fromUtf8("文本较长，是否处理全文？"),
        m_longTextConfirmation
    );
    confirmation->setWordWrap(true);
    confirmationLayout->addWidget(confirmation, 1);
    m_longTextProcess = new QPushButton(
        QString::fromUtf8("处理全文"),
        m_longTextConfirmation
    );
    m_longTextProcess->setObjectName(
        QStringLiteral("selectionLongTextProcessButton")
    );
    confirmationLayout->addWidget(m_longTextProcess);
    m_longTextCancel = new QPushButton(
        QString::fromUtf8("取消"),
        m_longTextConfirmation
    );
    m_longTextCancel->setObjectName(
        QStringLiteral("selectionLongTextCancelButton")
    );
    confirmationLayout->addWidget(m_longTextCancel);
    root->addWidget(m_longTextConfirmation);

    QWidget *actions = new QWidget(this);
    actions->setObjectName(QStringLiteral("selectionResultActions"));
    m_actionLayout = new QGridLayout(actions);
    m_actionLayout->setContentsMargins(0, 0, 0, 0);
    m_actionLayout->setHorizontalSpacing(6);
    m_actionLayout->setVerticalSpacing(6);

    m_cancel = new QPushButton(QString::fromUtf8("取消"), actions);
    m_cancel->setObjectName(QStringLiteral("selectionResultCancelButton"));
    m_regenerate = new QPushButton(QString::fromUtf8("重新生成"), actions);
    m_regenerate->setObjectName(
        QStringLiteral("selectionResultRegenerateButton")
    );
    m_copy = new QPushButton(QString::fromUtf8("复制"), actions);
    m_copy->setObjectName(QStringLiteral("selectionResultCopyButton"));
    m_replace = new QPushButton(QString::fromUtf8("替换"), actions);
    m_replace->setObjectName(QStringLiteral("selectionResultReplaceButton"));
    m_pin = new QToolButton(actions);
    m_pin->setObjectName(QStringLiteral("selectionResultPinButton"));
    m_pin->setText(QString::fromUtf8("固定"));
    m_pin->setCheckable(true);
    m_close = new QPushButton(QString::fromUtf8("关闭"), actions);
    m_close->setObjectName(QStringLiteral("selectionResultCloseButton"));
    root->addWidget(actions);

    QHBoxLayout *followUp = new QHBoxLayout;
    m_followUpInput = new QLineEdit(this);
    m_followUpInput->setObjectName(
        QStringLiteral("selectionResultFollowUpInput")
    );
    m_followUpInput->setPlaceholderText(QString::fromUtf8("继续追问…"));
    m_followUpInput->installEventFilter(this);
    followUp->addWidget(m_followUpInput, 1);
    m_followUpButton = new QPushButton(QString::fromUtf8("发送"), this);
    m_followUpButton->setObjectName(
        QStringLiteral("selectionResultFollowUpButton")
    );
    followUp->addWidget(m_followUpButton);
    root->addLayout(followUp);

    const QList<QWidget *> buttons = QList<QWidget *>()
        << m_longTextProcess << m_longTextCancel
        << m_cancel << m_regenerate << m_copy << m_replace
        << m_pin << m_close << m_followUpButton;
    for (QWidget *button : buttons) {
        button->setMinimumHeight(minimumButtonHeight(button));
    }

    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        invokeTerminal(m_callbacks.cancelRequested);
    });
    connect(m_regenerate, &QPushButton::clicked, this, [this]() {
        invokeAction(m_callbacks.regenerateRequested);
    });
    connect(m_copy, &QPushButton::clicked, this, [this]() {
        invokeAction(m_callbacks.copyRequested);
    });
    connect(m_replace, &QPushButton::clicked, this, [this]() {
        invokeAction(m_callbacks.replaceRequested);
    });
    connect(m_pin, &QToolButton::clicked, this, [this](bool checked) {
        m_state.pinned = checked;
        const std::function<void(bool)> callback = m_callbacks.pinChanged;
        if (!callback) {
            return;
        }
        QPointer<SelectionResultCard> guard(this);
        callback(checked);
        if (!guard) {
            return;
        }
    });
    connect(m_close, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.closeRequested) {
            invokeTerminal(m_callbacks.closeRequested);
        } else {
            hide();
            restorePassiveWindowMode();
        }
    });
    connect(m_followUpButton, &QPushButton::clicked, this, [this]() {
        const QString question = m_followUpInput->text().trimmed();
        const std::function<void(const QString &)> callback =
            m_callbacks.followUpRequested;
        if (question.isEmpty() || !callback) {
            return;
        }
        QPointer<SelectionResultCard> guard(this);
        callback(question);
        if (!guard) {
            return;
        }
    });
    connect(m_longTextProcess, &QPushButton::clicked, this, [this]() {
        m_state.requiresLongTextConfirmation = false;
        m_longTextConfirmation->hide();
        invokeTerminal(m_callbacks.processFullTextRequested);
    });
    connect(m_longTextCancel, &QPushButton::clicked, this, [this]() {
        m_state.requiresLongTextConfirmation = false;
        m_longTextConfirmation->hide();
        invokeTerminal(m_callbacks.cancelRequested);
    });

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(40);
    connect(m_renderTimer, &QTimer::timeout, this, [this]() {
        renderNow();
    });
    setProperty("selectionRenderCount", 0);
    resize(600, 460);
    updateControls();
    relayoutActionButtons();
}

SelectionResultCard::~SelectionResultCard() = default;

void SelectionResultCard::setCallbacks(
    const SelectionResultCardCallbacks &callbacks)
{
    m_callbacks = callbacks;
    updateControls();
}

void SelectionResultCard::setState(const SelectionResultCardState &state)
{
    m_state = state;
    updateControls();
    scheduleRender();
}

SelectionResultCardState SelectionResultCard::state() const
{
    return m_state;
}

void SelectionResultCard::showAt(
    const QPoint &topLeft,
    const QRect &availableGeometry)
{
    restorePassiveWindowMode();
    m_availableGeometry = availableGeometry;
    if (m_renderTimer->isActive()) {
        m_renderTimer->stop();
        renderNow();
    }
    const int maximumWidth = qMax(1, availableGeometry.width() - 16);
    const int maximumHeight = qMax(1, availableGeometry.height() - 16);
    resize(qMin(600, maximumWidth), qMin(480, maximumHeight));
    relayoutActionButtons();
    move(topLeft);
    clampToAvailableGeometry();
    show();
    raise();
}

void SelectionResultCard::closeIfUnpinned()
{
    if (!m_state.pinned) {
        hide();
        restorePassiveWindowMode();
    }
}

bool SelectionResultCard::ownsNativeWindow(
    SelectedTextNativeWindowHandle window) const
{
    if (!window) {
        return false;
    }
#ifdef Q_OS_WIN
    HWND candidate = static_cast<HWND>(window);
    if (candidate && IsWindow(candidate)) {
        const HWND root = GetAncestor(candidate, GA_ROOT);
        if (root) {
            candidate = root;
        }
    }
    return candidate == reinterpret_cast<HWND>(quintptr(winId()));
#else
    return WId(quintptr(window)) == winId();
#endif
}

bool SelectionResultCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_followUpInput
        && event
        && event->type() == QEvent::MouseButtonPress) {
        enableFollowUpFocus();
    }
    return QWidget::eventFilter(watched, event);
}

void SelectionResultCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutActionButtons();
}

void SelectionResultCard::updateControls()
{
    m_title->setText(m_state.title);
    m_status->setText(m_state.statusText);
    m_cancel->setVisible(m_state.running);
    m_cancel->setEnabled(m_state.running && bool(m_callbacks.cancelRequested));
    m_regenerate->setVisible(!m_state.running);
    m_copy->setEnabled(
        !m_state.running && !m_state.committedText.trimmed().isEmpty()
    );
    m_replace->setEnabled(
        !m_state.running
        && m_state.replaceEnabled
        && !m_state.committedText.trimmed().isEmpty()
    );
    {
        const QSignalBlocker blocker(m_pin);
        m_pin->setChecked(m_state.pinned);
    }
    m_longTextConfirmation->setVisible(
        m_state.requiresLongTextConfirmation
    );
    relayoutActionButtons();
}

void SelectionResultCard::scheduleRender()
{
    if (!m_renderTimer->isActive()) {
        m_renderTimer->start();
    }
}

void SelectionResultCard::renderNow()
{
    setProperty(
        "selectionRenderCount",
        property("selectionRenderCount").toInt() + 1
    );
    m_committed->setPlainText(m_state.committedText);
    m_provisional->setPlainText(m_state.provisionalText);
    m_provisional->setVisible(!m_state.provisionalText.isEmpty());
    if (isVisible()) {
        relayoutActionButtons();
        clampToAvailableGeometry();
    }
}

void SelectionResultCard::relayoutActionButtons()
{
    if (!m_actionLayout) {
        return;
    }
    const QList<QWidget *> all = QList<QWidget *>()
        << m_cancel << m_regenerate << m_copy
        << m_replace << m_pin << m_close;
    for (QWidget *button : all) {
        m_actionLayout->removeWidget(button);
    }
    const int available = qMax(1, width() - 24);
    int row = 0;
    int column = 0;
    int rowWidth = 0;
    for (QWidget *button : all) {
        if (!button || button->isHidden()) {
            continue;
        }
        const int buttonWidth = button->sizeHint().width();
        const int spacing = column == 0 ? 0 : m_actionLayout->horizontalSpacing();
        if (column > 0 && rowWidth + spacing + buttonWidth > available) {
            ++row;
            column = 0;
            rowWidth = 0;
        }
        m_actionLayout->addWidget(button, row, column);
        rowWidth += (column == 0 ? 0 : spacing) + buttonWidth;
        ++column;
    }
    m_actionLayout->invalidate();
}

void SelectionResultCard::clampToAvailableGeometry()
{
    if (!m_availableGeometry.isValid()) {
        return;
    }
    const int widthLimit = m_availableGeometry.width();
    const int heightLimit = m_availableGeometry.height();
    if (width() > widthLimit || height() > heightLimit) {
        resize(qMin(width(), widthLimit), qMin(height(), heightLimit));
    }
    const int maximumX = m_availableGeometry.right() - width() + 1;
    const int maximumY = m_availableGeometry.bottom() - height() + 1;
    move(
        qBound(m_availableGeometry.left(), x(), maximumX),
        qBound(m_availableGeometry.top(), y(), maximumY)
    );
}

void SelectionResultCard::enableFollowUpFocus()
{
    if (m_followUpFocusEnabled) {
        return;
    }
    m_followUpFocusEnabled = true;
    const QRect previousGeometry = geometry();
    const bool wasVisible = isVisible();
    setWindowFlags(focusedCardFlags());
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    if (wasVisible) {
        setGeometry(previousGeometry);
        show();
        activateWindow();
        m_followUpInput->setFocus(Qt::MouseFocusReason);
    }
}

void SelectionResultCard::restorePassiveWindowMode()
{
    if (!m_followUpFocusEnabled
        && (windowFlags() & Qt::WindowDoesNotAcceptFocus)) {
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        return;
    }
    m_followUpFocusEnabled = false;
    setWindowFlags(passiveCardFlags());
    setAttribute(Qt::WA_ShowWithoutActivating, true);
}

void SelectionResultCard::invokeAction(
    const std::function<void()> &callback)
{
    if (!callback) {
        return;
    }
    QPointer<SelectionResultCard> guard(this);
    callback();
    if (!guard) {
        return;
    }
}

void SelectionResultCard::invokeTerminal(std::function<void()> &callback)
{
    const std::function<void()> requested = callback;
    callback = std::function<void()>();
    if (!requested) {
        return;
    }
    QPointer<SelectionResultCard> guard(this);
    requested();
    if (!guard) {
        return;
    }
}
