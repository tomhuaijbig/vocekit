#ifndef VOCEKIT_HUB_UTILITY_PAGES_CONTROLLER_H
#define VOCEKIT_HUB_UTILITY_PAGES_CONTROLLER_H

#include <QScopedPointer>
#include <QString>

#include <functional>

class FloatingBar;
class HubSettingsState;
class QWidget;
struct LogPaginationSnapshot;
struct PromptSettingsAccess;

// 辅助页面装配输入：主窗口只提供共享状态和跨页面动作。
struct HubUtilityPagesControllerAccess
{
    HubSettingsState *settings = nullptr;
    FloatingBar *floatingBar = nullptr;
    QWidget *popupFallbackParent = nullptr;
    std::function<void()> notifySettingsChanged;
    std::function<void(const QString &)> selectPage;
};

// 集中管理提示词、日志、测试工具、设置和常见问题页面及其相互刷新。
class HubUtilityPagesController
{
public:
    explicit HubUtilityPagesController(
        const HubUtilityPagesControllerAccess &access
    );
    ~HubUtilityPagesController();

    QWidget *promptsPage();
    QWidget *diagnosticsPage();
    QWidget *logsPage();
    QWidget *settingsPage();
    QWidget *faqPage();

    PromptSettingsAccess promptSettingsAccess() const;

    void refreshPrompts();
    void refreshDiagnostics();
    void refreshLogs(bool reloadFromDisk = true);
    void refreshSettings();
    void updateLogPagination(const LogPaginationSnapshot &snapshot);

    void openSettings(int tabIndex);
    void openFaq(const QString &faqId);
    void openDiagnostics(const QString &keyword);

private:
    HubUtilityPagesController(const HubUtilityPagesController &) = delete;
    HubUtilityPagesController &operator=(
        const HubUtilityPagesController &
    ) = delete;

    class Impl;
    QScopedPointer<Impl> m_impl;
};

#endif // VOCEKIT_HUB_UTILITY_PAGES_CONTROLLER_H
