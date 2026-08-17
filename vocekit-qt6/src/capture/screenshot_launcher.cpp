#include "screenshot_launcher.h"
#include "../ui/screen_position.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ScreenshotLauncher::ScreenshotLauncher(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(
        Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
    );
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("screenshotLauncher"));
    setStyleSheet(QStringLiteral(
        "QWidget#screenshotLauncher { background:#111827; border-radius:7px; }"
        "QPushButton { background:transparent; color:#ffffff; border:0;"
        " padding:0 14px; font-family:'Microsoft YaHei UI'; font-size:14px;"
        " font-weight:600; }"
        "QPushButton:hover { background:#263244; border-radius:7px; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_button = new QPushButton(QStringLiteral("截图"), this);
    m_button->setMinimumSize(72, 40);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->installEventFilter(this);
    layout->addWidget(m_button);
    resize(72, 40);

    connect(m_button, &QPushButton::clicked, this, [this]() {
        if (!m_dragging) {
            showFunctionMenu();
        }
    });
}

void ScreenshotLauncher::setFunctions(
    const QVector<QPair<QString, QString>> &functions)
{
    m_functions = functions;
    setVisible(!m_functions.isEmpty());
}

void ScreenshotLauncher::setSavedPosition(
    const QPoint &position,
    bool hasPosition)
{
    const QRect screen = primaryAvailableScreenGeometry();
    if (hasPosition && screen.contains(position)) {
        move(position);
        return;
    }
    move(screen.right() - width() - 24, screen.center().y() - height() / 2);
}

bool ScreenshotLauncher::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_button) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            rememberTargetWindow();
            m_dragging = false;
            m_dragOffset =
                mouse->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->buttons() & Qt::LeftButton) {
            const QPoint next =
                mouse->globalPosition().toPoint() - m_dragOffset;
            if ((next - pos()).manhattanLength() > 2) {
                m_dragging = true;
            }
            move(next);
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_dragging && positionChangedCallback) {
            positionChangedCallback(pos());
        }
        QTimer::singleShot(0, this, [this]() {
            m_dragging = false;
        });
    }
    return QWidget::eventFilter(watched, event);
}

void ScreenshotLauncher::closeEvent(QCloseEvent *event)
{
    if (positionChangedCallback) {
        positionChangedCallback(pos());
    }
    QWidget::closeEvent(event);
}

void ScreenshotLauncher::rememberTargetWindow()
{
    m_rememberedTargetWindow = captureTargetWindowCallback
        ? captureTargetWindowCallback()
        : nullptr;
}

void ScreenshotLauncher::showFunctionMenu()
{
    if (m_functions.isEmpty()) {
        return;
    }
    if (!m_rememberedTargetWindow) {
        rememberTargetWindow();
    }
    const ScreenshotLauncherTargetWindowHandle targetWindow =
        m_rememberedTargetWindow;
    if (m_functions.size() == 1) {
        if (functionTriggeredCallback) {
            functionTriggeredCallback(
                m_functions.first().first,
                targetWindow
            );
        }
        return;
    }

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    for (const auto &function : m_functions) {
        QAction *action = menu->addAction(function.second);
        connect(
            action,
            &QAction::triggered,
            menu,
            [this, function, targetWindow]() {
            if (functionTriggeredCallback) {
                functionTriggeredCallback(
                    function.first,
                    targetWindow
                );
            }
        });
    }
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(mapToGlobal(QPoint(0, height() + 4)));
}
