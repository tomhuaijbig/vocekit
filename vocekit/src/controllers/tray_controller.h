#ifndef TRAY_CONTROLLER_H
#define TRAY_CONTROLLER_H

#include <QObject>
#include <QString>

#include <functional>

class QSystemTrayIcon;
class QWidget;

// 托盘控制器：负责后台常驻托盘、托盘菜单和常用开关，不直接依赖主窗口内部实现。
class TrayController : public QObject
{
public:
    struct Callbacks
    {
        std::function<QString()> speechProvider;
        std::function<bool()> useSystemProxy;
        std::function<bool()> floatingBarEnabled;
        std::function<void(const QString &)> setSpeechProvider;
        std::function<void(bool)> setUseSystemProxy;
        std::function<void(bool)> setFloatingBarEnabled;
        std::function<void()> showFloatingBarTest;
        std::function<void()> openSettings;
        std::function<void()> requestApplicationQuit;
    };

    TrayController(
        QWidget *hubWindow,
        const Callbacks &callbacks,
        QObject *parent = nullptr
    );

private:
    void applyQuickSetting(const QString &message);
    void showHubWindow();

    QWidget *m_hubWindow;
    Callbacks m_callbacks;
    QSystemTrayIcon *m_tray = nullptr;
};

#endif // TRAY_CONTROLLER_H
