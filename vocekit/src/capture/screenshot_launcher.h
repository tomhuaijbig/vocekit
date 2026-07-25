#ifndef VOCEKIT_SCREENSHOT_LAUNCHER_H
#define VOCEKIT_SCREENSHOT_LAUNCHER_H

#include <QPair>
#include <QPoint>
#include <QVector>
#include <QWidget>

#include <functional>

class QPushButton;

class ScreenshotLauncher : public QWidget
{
public:
    explicit ScreenshotLauncher(QWidget *parent = nullptr);

    void setFunctions(const QVector<QPair<QString, QString>> &functions);
    void setSavedPosition(const QPoint &position, bool hasPosition);

    std::function<void(const QString &)> functionTriggeredCallback;
    std::function<void(const QPoint &)> positionChangedCallback;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void showFunctionMenu();

    QPushButton *m_button = nullptr;
    QVector<QPair<QString, QString>> m_functions;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // VOCEKIT_SCREENSHOT_LAUNCHER_H
