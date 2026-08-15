#include "selection_context_toolbar.h"

#include "selection_context_placement.h"
#include "../domain/selection_context_actions.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QSet>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>
#include <QVariant>
#include <QKeyEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

class SelectionContextToolButton : public QToolButton
{
public:
    SelectionContextToolButton(
        const std::function<void()> &escapeRequested,
        QWidget *parent)
        : QToolButton(parent),
          m_escapeRequested(escapeRequested)
    {
    }

    void setEscapeRequested(const std::function<void()> &callback)
    {
        m_escapeRequested = callback;
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event && event->key() == Qt::Key_Escape
            && m_escapeRequested) {
            const std::function<void()> callback = m_escapeRequested;
            event->accept();
            callback();
            return;
        }
        QToolButton::keyPressEvent(event);
    }

private:
    std::function<void()> m_escapeRequested;
};

QString actionButtonObjectName(const QString &id)
{
    if (id == selectionContextActionAiSearch()) {
        return QStringLiteral("selectionActionAiSearchButton");
    }
    if (id == selectionContextActionTranslate()) {
        return QStringLiteral("selectionActionTranslateButton");
    }
    if (id == selectionContextActionExplain()) {
        return QStringLiteral("selectionActionExplainButton");
    }
    if (id == selectionContextActionSave()) {
        return QStringLiteral("selectionActionSaveButton");
    }
    if (id == selectionContextActionCopy()) {
        return QStringLiteral("selectionActionCopyButton");
    }
    return QString();
}

Qt::WindowFlags normalWindowFlags()
{
    return Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
        | Qt::WindowDoesNotAcceptFocus;
}

Qt::WindowFlags keyboardWindowFlags()
{
    return Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint;
}

} // namespace

SelectionContextToolbar::SelectionContextToolbar(QWidget *parent)
    : QWidget(parent, normalWindowFlags()),
      m_actionOrder(defaultSelectionContextActionOrder())
{
    setObjectName(QStringLiteral("selectionContextToolbar"));
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet(QString::fromUtf8(
        "QWidget#selectionContextToolbar {"
        " background: #ffffff; border: 1px solid #d0d5dd;"
        " border-radius: 12px; }"
        "QToolButton { background: transparent; border: 0;"
        " color: #111827; padding: 0px 10px; border-radius: 8px; }"
        "QToolButton:hover { background: #eef2ff; color: #1d4ed8; }"
        "QToolButton:pressed { background: #dbeafe; }"
        "QToolButton:disabled { color: #98a2b3; }"
        "QLabel#selectionContextIdentity { color: #475467;"
        " padding: 0px 6px; }"
        "QWidget#selectionContextDragHandle { color: #98a2b3; }"
    ));

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(2);

    QLabel *drag = new QLabel(QString::fromUtf8("⠿"), this);
    drag->setObjectName(QStringLiteral("selectionContextDragHandle"));
    drag->setCursor(Qt::SizeAllCursor);
    drag->setAlignment(Qt::AlignCenter);
    drag->setMinimumWidth(22);
    drag->installEventFilter(this);
    m_dragHandle = drag;
    m_layout->addWidget(drag);

    m_identity = new QLabel(QStringLiteral("AI"), this);
    m_identity->setObjectName(QStringLiteral("selectionContextIdentity"));
    m_identity->setAlignment(Qt::AlignCenter);
    m_identity->setAccessibleName(QString::fromUtf8("AI 助手"));
    m_layout->addWidget(m_identity);

    for (const QString &id : defaultSelectionContextActionOrder()) {
        m_actionEnabled.insert(id, true);
    }

    m_moreButton = new SelectionContextToolButton(
        [this]() { requestClose(); },
        this
    );
    m_moreButton->setObjectName(QStringLiteral("selectionContextMoreButton"));
    m_moreButton->setText(QString::fromUtf8("更多"));
    m_moreButton->setToolTip(QString::fromUtf8("更多操作"));
    m_moreButton->setAccessibleName(QString::fromUtf8("更多操作"));
    m_moreButton->setFocusPolicy(Qt::StrongFocus);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_layout->addWidget(m_moreButton);

    m_closeButton = new SelectionContextToolButton(
        [this]() { requestClose(); },
        this
    );
    m_closeButton->setObjectName(QStringLiteral("selectionToolbarCloseButton"));
    m_closeButton->setText(QString::fromUtf8("×"));
    m_closeButton->setToolTip(QString::fromUtf8("关闭"));
    m_closeButton->setAccessibleName(QString::fromUtf8("关闭"));
    m_closeButton->setFocusPolicy(Qt::StrongFocus);
    connect(m_closeButton, &QToolButton::clicked, this, [this]() {
        requestClose();
    });
    m_layout->addWidget(m_closeButton);

    m_moreMenu = new QMenu(m_moreButton);
    m_moreMenu->setObjectName(QStringLiteral("selectionContextMoreMenu"));
    m_moreMenu->setStyleSheet(QString::fromUtf8(
        "QMenu { background: #ffffff; border: 1px solid #d0d5dd;"
        " padding: 6px; }"
        "QMenu::item { padding: 8px 28px 8px 12px; color: #111827; }"
        "QMenu::item:selected { background: #eef2ff; }"
    ));
    m_moreButton->setMenu(m_moreMenu);

    setActionPresentation(
        defaultSelectionContextActionOrder(),
        defaultSelectionContextActionCustomizations()
    );
    applyButtonMetrics();
}

SelectionContextToolbar::~SelectionContextToolbar() = default;

void SelectionContextToolbar::paintEvent(QPaintEvent *event)
{
    QStyleOption option;
    option.init(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
    QWidget::paintEvent(event);
}

void SelectionContextToolbar::setCallbacks(
    const SelectionContextToolbarCallbacks &callbacks)
{
    m_callbacks = callbacks;
}

void SelectionContextToolbar::setActionPresentation(
    const QStringList &actionIds,
    const SelectionContextActionCustomizationMap &customizations)
{
    const bool refreshVisibleToolbar = isVisible()
        && m_availableGeometry.isValid();
    const QPoint previousCenter = geometry().center();
    QPointer<QWidget> previousFocus = focusWidget();
    m_actionOrder = visibleSelectionContextActionOrder(
        actionIds,
        customizations
    );
    m_actionTitles.clear();
    for (const QString &id : m_actionOrder) {
        m_actionTitles.insert(
            id,
            selectionContextActionDisplayName(id, customizations)
        );
    }
    m_overflowActionIds.clear();
    rebuildActionButtons();
    if (refreshVisibleToolbar) {
        resizeForAvailableGeometry(m_availableGeometry);
        move(
            previousCenter.x() - width() / 2,
            previousCenter.y() - height() / 2
        );
        keepInsideAvailableGeometry(m_availableGeometry);
    } else {
        rebuildMoreMenu();
    }
    if (previousFocus) {
        QWidget *focusTarget = previousFocus.data();
        if (!focusTarget->isVisible() || !focusTarget->isEnabled()) {
            focusTarget = firstVisibleActionButton();
        }
        if (focusTarget) {
            focusTarget->setFocus(Qt::OtherFocusReason);
        }
    }
}

void SelectionContextToolbar::setActionOrder(const QStringList &actionIds)
{
    setActionPresentation(
        actionIds,
        defaultSelectionContextActionCustomizations()
    );
}

void SelectionContextToolbar::setMoreActions(
    const QVector<SelectionContextMenuItem> &items)
{
    m_moreActions = items;
    rebuildMoreMenu();
}

void SelectionContextToolbar::setBusyAction(const QString &actionId)
{
    m_busyActionId = actionId.trimmed();
    for (auto it = m_actionButtons.begin();
         it != m_actionButtons.end();
         ++it) {
        const bool baseEnabled = m_actionEnabled.value(it.key(), true);
        const bool presented = m_actionOrder.contains(it.key());
        it.value()->setEnabled(
            presented && m_busyActionId.isEmpty() && baseEnabled
        );
    }
    m_moreButton->setEnabled(m_busyActionId.isEmpty());
    updateMoreMenuEnabledState();
}

void SelectionContextToolbar::setActionEnabled(
    const QString &actionId,
    bool enabled)
{
    if (!m_actionButtons.contains(actionId)) {
        return;
    }
    m_actionEnabled.insert(actionId, enabled);
    m_actionButtons.value(actionId)->setEnabled(
        m_actionOrder.contains(actionId)
            && m_busyActionId.isEmpty()
            && enabled
    );
    updateMoreMenuEnabledState();
}

void SelectionContextToolbar::showForSnapshot(
    const SelectionSnapshot &snapshot,
    const QRect &availableGeometry,
    bool keyboardNavigationMode)
{
    m_availableGeometry = availableGeometry;
    applyWindowMode(keyboardNavigationMode);
    resizeForAvailableGeometry(availableGeometry);
    const SelectionSurfacePlacement placement = placeSelectionSurfaces(
        snapshot.anchorRect,
        snapshot.cursorPosition,
        size(),
        QSize(),
        availableGeometry,
        8
    );
    move(placement.toolbarTopLeft);
    show();
    raise();
    if (keyboardNavigationMode) {
        activateWindow();
        if (QToolButton *button = firstVisibleActionButton()) {
            button->setFocus(Qt::ShortcutFocusReason);
        }
    }
}

void SelectionContextToolbar::hideToolbar()
{
    if (m_moreMenu) {
        m_moreMenu->hide();
    }
    hide();
    applyWindowMode(false);
}

bool SelectionContextToolbar::ownsNativeWindow(
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
    HWND own = reinterpret_cast<HWND>(quintptr(winId()));
    if (candidate == own) {
        return true;
    }
    if (m_moreMenu) {
        HWND menu = reinterpret_cast<HWND>(quintptr(m_moreMenu->winId()));
        return candidate == menu;
    }
    return false;
#else
    const WId candidate = WId(quintptr(window));
    return candidate == winId()
        || (m_moreMenu && candidate == m_moreMenu->winId());
#endif
}

bool SelectionContextToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_dragHandle) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragOffset = mouse->globalPos() - frameGeometry().topLeft();
            m_dragHandle->grabMouse();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_dragging) {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        move(mouse->globalPos() - m_dragOffset);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            m_dragHandle->releaseMouse();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SelectionContextToolbar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event && event->type() == QEvent::FontChange) {
        applyButtonMetrics();
    }
}

void SelectionContextToolbar::applyWindowMode(bool keyboardNavigationMode)
{
    const Qt::WindowFlags wanted = keyboardNavigationMode
        ? keyboardWindowFlags()
        : normalWindowFlags();
    if (windowFlags() != wanted) {
        setWindowFlags(wanted);
    }
    setAttribute(Qt::WA_ShowWithoutActivating, !keyboardNavigationMode);
    setFocusPolicy(keyboardNavigationMode ? Qt::StrongFocus : Qt::NoFocus);
}

void SelectionContextToolbar::applyButtonMetrics()
{
    const int minimum = qMax(40, fontMetrics().height() + 16);
    for (QToolButton *button : m_actionButtons) {
        button->setMinimumHeight(minimum);
    }
    if (m_moreButton) {
        m_moreButton->setMinimumHeight(minimum);
    }
    if (m_closeButton) {
        m_closeButton->setMinimumHeight(minimum);
        m_closeButton->setMinimumWidth(minimum);
    }
    if (m_dragHandle) {
        m_dragHandle->setMinimumHeight(minimum);
    }
    if (m_identity) {
        m_identity->setMinimumHeight(minimum);
    }
}

void SelectionContextToolbar::rebuildActionButtons()
{
    if (!m_layout || !m_moreButton) {
        return;
    }
    for (const QString &id : defaultSelectionContextActionOrder()) {
        if (m_actionButtons.contains(id)) {
            continue;
        }
        QToolButton *button = new SelectionContextToolButton(
            std::function<void()>(),
            this
        );
        button->setFocusPolicy(Qt::StrongFocus);
        connect(button, &QToolButton::clicked, this, [this, id]() {
            requestAction(id);
        });
        m_actionButtons.insert(id, button);
    }
    applyButtonMetrics();
    rebuildActionLayout();
}

void SelectionContextToolbar::rebuildActionLayout()
{
    if (!m_layout || !m_moreButton) {
        return;
    }
    for (QToolButton *button : m_actionButtons) {
        m_layout->removeWidget(button);
        button->hide();
        button->setEnabled(false);
        button->setObjectName(QString());
        button->setProperty("selectionActionId", QVariant());
        button->setText(QString());
        button->setIcon(QIcon());
        button->setToolTip(QString());
        button->setAccessibleName(QString());
        SelectionContextToolButton *inactive =
            static_cast<SelectionContextToolButton *>(button);
        inactive->setEscapeRequested(std::function<void()>());
    }
    int insertion = m_layout->indexOf(m_moreButton);
    for (const QString &id : m_actionOrder) {
        QToolButton *button = m_actionButtons.value(id, nullptr);
        if (!button) {
            continue;
        }
        const QString title = m_actionTitles.value(
            id,
            selectionContextActionTitle(id)
        );
        button->setObjectName(actionButtonObjectName(id));
        button->setProperty("selectionActionId", id);
        button->setText(title);
        button->setIcon(QIcon());
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setToolTip(title);
        button->setAccessibleName(title);
        SelectionContextToolButton *active =
            static_cast<SelectionContextToolButton *>(button);
        active->setEscapeRequested([this]() { requestClose(); });
        button->setEnabled(
            m_busyActionId.isEmpty() && m_actionEnabled.value(id, true)
        );
        button->show();
        m_layout->insertWidget(insertion++, button);
    }
    m_layout->invalidate();
}

void SelectionContextToolbar::rebuildMoreMenu()
{
    if (!m_moreMenu) {
        return;
    }
    m_moreMenu->clear();
    QSet<QString> added;
    auto addItem = [this, &added](
        const QString &id,
        const QString &title,
        bool enabled) {
        if (id.trimmed().isEmpty() || added.contains(id)) {
            return;
        }
        added.insert(id);
        QAction *action = m_moreMenu->addAction(title);
        action->setData(id);
        action->setEnabled(m_busyActionId.isEmpty() && enabled);
        connect(action, &QAction::triggered, this, [this, id]() {
            requestAction(id);
        });
    };

    for (const QString &id : m_overflowActionIds) {
        addItem(id, m_actionTitles.value(id, selectionContextActionTitle(id)),
                m_actionEnabled.value(id, true));
    }
    if (!m_overflowActionIds.isEmpty()
        && (!m_moreActions.isEmpty())) {
        m_moreMenu->addSeparator();
    }
    for (const SelectionContextMenuItem &item : m_moreActions) {
        addItem(item.actionId, item.title, item.enabled);
    }
    if (!added.isEmpty()) {
        m_moreMenu->addSeparator();
    }
    addItem(
        selectionContextMenuBlockApplication(),
        selectionContextActionTitle(selectionContextMenuBlockApplication()),
        true
    );
    addItem(
        selectionContextMenuOpenSettings(),
        selectionContextActionTitle(selectionContextMenuOpenSettings()),
        true
    );
}

void SelectionContextToolbar::updateMoreMenuEnabledState()
{
    if (!m_moreMenu) {
        return;
    }
    for (QAction *action : m_moreMenu->actions()) {
        if (!action || action->isSeparator()) {
            continue;
        }
        const QString id = action->data().toString();
        bool enabled = true;
        if (m_actionEnabled.contains(id)) {
            enabled = m_actionEnabled.value(id, true);
        } else {
            for (const SelectionContextMenuItem &item : m_moreActions) {
                if (item.actionId == id) {
                    enabled = item.enabled;
                    break;
                }
            }
        }
        action->setEnabled(m_busyActionId.isEmpty() && enabled);
    }
}

void SelectionContextToolbar::resizeForAvailableGeometry(
    const QRect &availableGeometry)
{
    applyButtonMetrics();
    configureForAvailableWidth(qMax(1, availableGeometry.width() - 16));
    const int width = qMin(
        visibleRowWidth(),
        qMax(1, availableGeometry.width() - 16)
    );
    const int height = qMax(sizeHint().height(), minimumSizeHint().height());
    resize(width, height);
    updateGeometry();
}

void SelectionContextToolbar::keepInsideAvailableGeometry(
    const QRect &availableGeometry)
{
    if (!availableGeometry.isValid()) {
        return;
    }
    const int maximumX = qMax(
        availableGeometry.left(),
        availableGeometry.right() - width() + 1
    );
    const int maximumY = qMax(
        availableGeometry.top(),
        availableGeometry.bottom() - height() + 1
    );
    move(
        qBound(availableGeometry.left(), x(), maximumX),
        qBound(availableGeometry.top(), y(), maximumY)
    );
}

void SelectionContextToolbar::configureForAvailableWidth(int width)
{
    m_overflowActionIds.clear();
    m_identity->show();
    rebuildActionLayout();
    m_moreButton->show();
    m_closeButton->show();
    m_layout->activate();
    if (visibleRowWidth() > width) {
        m_identity->hide();
        m_layout->activate();
    }
    for (int index = m_actionOrder.size() - 1;
         index > 0 && visibleRowWidth() > width;
         --index) {
        const QString id = m_actionOrder.at(index);
        QToolButton *button = m_actionButtons.value(id, nullptr);
        if (button && !button->isHidden()) {
            button->hide();
            m_overflowActionIds.prepend(id);
            m_layout->activate();
        }
    }
    if (visibleRowWidth() > width && !m_actionOrder.isEmpty()) {
        QToolButton *first = m_actionButtons.value(m_actionOrder.constFirst());
        if (first) {
            const QString title = m_actionTitles.value(
                m_actionOrder.constFirst(),
                selectionContextActionTitle(m_actionOrder.constFirst())
            );
            first->setText(QString());
            first->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
            first->setToolButtonStyle(Qt::ToolButtonIconOnly);
            first->setToolTip(title);
            first->setAccessibleName(title);
        }
    }
    rebuildMoreMenu();
    m_layout->invalidate();
    m_layout->activate();
}

int SelectionContextToolbar::visibleRowWidth() const
{
    if (!m_layout) {
        return 0;
    }
    QMargins margins = m_layout->contentsMargins();
    int width = margins.left() + margins.right();
    int visibleCount = 0;
    for (int i = 0; i < m_layout->count(); ++i) {
        QWidget *widget = m_layout->itemAt(i)->widget();
        if (!widget || widget->isHidden()) {
            continue;
        }
        width += widget->sizeHint().width();
        ++visibleCount;
    }
    if (visibleCount > 1) {
        width += (visibleCount - 1) * m_layout->spacing();
    }
    return width;
}

QToolButton *SelectionContextToolbar::firstVisibleActionButton() const
{
    for (const QString &id : m_actionOrder) {
        QToolButton *button = m_actionButtons.value(id, nullptr);
        if (button && !button->isHidden() && button->isEnabled()) {
            return button;
        }
    }
    return m_moreButton && m_moreButton->isEnabled()
        ? m_moreButton
        : m_closeButton;
}

void SelectionContextToolbar::requestAction(const QString &actionId)
{
    const std::function<void(const QString &)> callback =
        m_callbacks.actionRequested;
    if (!callback) {
        return;
    }
    QPointer<SelectionContextToolbar> guard(this);
    callback(actionId);
    if (!guard) {
        return;
    }
}

void SelectionContextToolbar::requestClose()
{
    const std::function<void()> callback = m_callbacks.closeRequested;
    if (!callback) {
        hideToolbar();
        return;
    }
    QPointer<SelectionContextToolbar> guard(this);
    callback();
    if (!guard) {
        return;
    }
}
