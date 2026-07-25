#ifndef VOCEKIT_HUB_NAVIGATION_CONTROLLER_H
#define VOCEKIT_HUB_NAVIGATION_CONTROLLER_H

#include <QPointer>
#include <QString>

#include <functional>

class CommandCenterShell;
class HubPageRouter;

struct HubNavigationControllerAccess
{
    std::function<QString()> currentFunctionId;
    std::function<bool(const QString &)> setCurrentFunctionId;
    std::function<void()> clearCurrentFunction;
    std::function<bool()> addFunction;
};

// 统一处理页面跳转、功能选中状态和命令中心导航同步。
class HubNavigationController
{
public:
    HubNavigationController(
        HubPageRouter *pageRouter,
        CommandCenterShell *commandShell,
        const HubNavigationControllerAccess &access
    );

    bool selectPage(const QString &pageId);
    bool openFunction(const QString &functionId);
    bool openTool(const QString &pageId);
    bool addFunction();
    void refreshFunctions();

    QString currentPageId() const;

private:
    QString currentFunctionId() const;
    void clearCurrentFunction();
    void synchronizeShell(const QString &pageId);

    QPointer<HubPageRouter> m_pageRouter;
    QPointer<CommandCenterShell> m_commandShell;
    HubNavigationControllerAccess m_access;
};

#endif // VOCEKIT_HUB_NAVIGATION_CONTROLLER_H
