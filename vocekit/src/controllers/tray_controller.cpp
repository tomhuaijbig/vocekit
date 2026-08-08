#include "tray_controller.h"

#include "../config/app_settings_defaults.h"

#include <QtWidgets>

TrayController::TrayController(
    QWidget *hubWindow,
    const Callbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_hubWindow(hubWindow),
      m_callbacks(callbacks)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaVolume));
    m_tray->setToolTip(QStringLiteral("vocekit"));

    auto *menu = new QMenu(m_hubWindow);
    auto *openHub = menu->addAction(QString::fromUtf8("打开主界面"));
    auto *settingsAction = menu->addAction(QString::fromUtf8("设置"));
    menu->addSeparator();

    auto *speechMenu = menu->addMenu(QString::fromUtf8("语音识别服务"));
    auto *speechGroup = new QActionGroup(speechMenu);
    speechGroup->setExclusive(true);
    auto *baiduSpeech = speechMenu->addAction(QString::fromUtf8("百度语音识别"));
    baiduSpeech->setCheckable(true);
    baiduSpeech->setActionGroup(speechGroup);
    auto *xfyunSpeech = speechMenu->addAction(QString::fromUtf8("讯飞语音听写"));
    xfyunSpeech->setCheckable(true);
    xfyunSpeech->setActionGroup(speechGroup);

    auto *proxyAction = menu->addAction(QString::fromUtf8("网络代理：直连"));
    proxyAction->setCheckable(true);
    auto *floatingBarAction = menu->addAction(QString::fromUtf8("浮动条：语音时显示"));
    floatingBarAction->setCheckable(true);
    menu->addSeparator();

    auto *showBar = menu->addAction(QString::fromUtf8("测试浮动条"));
    menu->addSeparator();
    auto *quit = menu->addAction(QString::fromUtf8("退出"));
    quit->setObjectName(QStringLiteral("trayQuitAction"));

    connect(openHub, &QAction::triggered, this, [this]() {
        showHubWindow();
    });
    connect(
        menu,
        &QMenu::aboutToShow,
        this,
        [this, baiduSpeech, xfyunSpeech, proxyAction, floatingBarAction]() {
            const QString provider = m_callbacks.speechProvider
                ? m_callbacks.speechProvider()
                : QString();
            baiduSpeech->setChecked(provider == speechProviderBaidu());
            xfyunSpeech->setChecked(provider == speechProviderXfyun());

            const bool useProxy = m_callbacks.useSystemProxy
                && m_callbacks.useSystemProxy();
            proxyAction->setChecked(useProxy);
            proxyAction->setText(useProxy
                ? QString::fromUtf8("网络代理：使用系统代理")
                : QString::fromUtf8("网络代理：直连"));

            const bool floatingEnabled = !m_callbacks.floatingBarEnabled
                || m_callbacks.floatingBarEnabled();
            floatingBarAction->setChecked(floatingEnabled);
            floatingBarAction->setText(floatingEnabled
                ? QString::fromUtf8("浮动条：语音时显示")
                : QString::fromUtf8("浮动条：已关闭"));
        }
    );
    connect(baiduSpeech, &QAction::triggered, this, [this]() {
        if (m_callbacks.speechProvider
            && m_callbacks.speechProvider() == speechProviderBaidu()) {
            return;
        }
        if (m_callbacks.setSpeechProvider) {
            m_callbacks.setSpeechProvider(speechProviderBaidu());
        }
        applyQuickSetting(QString::fromUtf8("已切换为百度语音识别"));
    });
    connect(xfyunSpeech, &QAction::triggered, this, [this]() {
        if (m_callbacks.speechProvider
            && m_callbacks.speechProvider() == speechProviderXfyun()) {
            return;
        }
        if (m_callbacks.setSpeechProvider) {
            m_callbacks.setSpeechProvider(speechProviderXfyun());
        }
        applyQuickSetting(QString::fromUtf8("已切换为讯飞语音听写"));
    });
    connect(proxyAction, &QAction::triggered, this, [this](bool checked) {
        if (m_callbacks.setUseSystemProxy) {
            m_callbacks.setUseSystemProxy(checked);
        }
        applyQuickSetting(checked
            ? QString::fromUtf8("网络代理已切换为系统代理")
            : QString::fromUtf8("网络代理已切换为直连"));
    });
    connect(floatingBarAction, &QAction::triggered, this, [this](bool checked) {
        if (m_callbacks.setFloatingBarEnabled) {
            m_callbacks.setFloatingBarEnabled(checked);
        }
        applyQuickSetting(checked
            ? QString::fromUtf8("浮动条已启用")
            : QString::fromUtf8("浮动条已关闭"));
    });
    connect(showBar, &QAction::triggered, this, [this]() {
        if (m_callbacks.showFloatingBarTest) {
            m_callbacks.showFloatingBarTest();
        }
    });
    connect(settingsAction, &QAction::triggered, this, [this]() {
        if (m_callbacks.openSettings) {
            m_callbacks.openSettings();
        }
    });
    connect(quit, &QAction::triggered, this, [this]() {
        if (m_callbacks.requestApplicationQuit) {
            m_callbacks.requestApplicationQuit();
        }
    });
    connect(
        m_tray,
        &QSystemTrayIcon::activated,
        this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                showHubWindow();
            }
        }
    );

    m_tray->setContextMenu(menu);
    m_tray->show();
}

void TrayController::applyQuickSetting(const QString &message)
{
    if (m_tray && !message.trimmed().isEmpty()) {
        m_tray->showMessage(
            QString::fromUtf8("语音助手"),
            message,
            QSystemTrayIcon::Information,
            1600
        );
    }
}

void TrayController::showHubWindow()
{
    if (!m_hubWindow) {
        return;
    }
    m_hubWindow->showNormal();
    m_hubWindow->raise();
    m_hubWindow->activateWindow();
}
