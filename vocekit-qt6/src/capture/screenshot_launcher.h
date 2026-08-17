#ifndef VOCEKIT_SCREENSHOT_LAUNCHER_H
#define VOCEKIT_SCREENSHOT_LAUNCHER_H

#include <QPair>
#include <QPoint>
#include <QVector>
#include <QWidget>

#include <functional>

class QPushButton;

using ScreenshotLauncherTargetWindowHandle = void *;

class ScreenshotLauncher : public QWidget
{
public:
    explicit ScreenshotLauncher(QWidget *parent = nullptr);

    void setFunctions(const QVector<QPair<QString, QString>> &functions);
    void setSavedPosition(const QPoint &position, bool hasPosition);

    std::function<ScreenshotLauncherTargetWindowHandle()>
        captureTargetWindowCallback;
    std::function<void(
        const QString &,
        ScreenshotLauncherTargetWindowHandle
    )> functionTriggeredCallback;
    std::function<void(const QPoint &)> positionChangedCallback;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void rememberTargetWindow();
    void showFunctionMenu();

    QPushButton *m_button = nullptr;
    QVector<QPair<QString, QString>> m_functions;
    bool m_dragging = false;
    QPoint m_dragOffset;
    ScreenshotLauncherTargetWindowHandle m_rememberedTargetWindow =
        nullptr;
};

#endif // VOCEKIT_SCREENSHOT_LAUNCHER_H
