#include "hub_navigation_controller.h"

#include "command_center_shell.h"
#include "hub_page_router.h"

HubNavigationController::HubNavigationController(
    HubPageRouter *pageRouter,
    CommandCenterShell *commandShell,
    const HubNavigationControllerAccess &access
)
    : m_pageRouter(pageRouter),
      m_commandShell(commandShell),
      m_access(access)
{
}

bool HubNavigationController::selectPage(const QString &pageId)
{
    const QString id = pageId.trimmed();
    if (id.isEmpty()
        || !m_pageRouter
        || !canLeaveCurrentFunctionPage(id)
        || !m_pageRouter->selectPage(id)) {
        return false;
    }

    if (id != QStringLiteral("function")) {
        clearCurrentFunction();
    }
    synchronizeShell(id);
    return true;
}

bool HubNavigationController::openFunction(const QString &functionId)
{
    const QString id = functionId.trimmed();
    if (id.isEmpty()) {
        return false;
    }

    if (id == QStringLiteral("home")) {
        return selectPage(id);
    }

    const QString previousFunctionId = currentFunctionId();
    if (m_pageRouter
        && m_pageRouter->currentPageId() == QStringLiteral("function")
        && previousFunctionId == id) {
        return false;
    }
    if (!m_access.setCurrentFunctionId
        || !m_access.setCurrentFunctionId(id)) {
        return false;
    }
    if (selectPage(QStringLiteral("function"))) {
        return true;
    }

    if (previousFunctionId.isEmpty()) {
        clearCurrentFunction();
    } else {
        m_access.setCurrentFunctionId(previousFunctionId);
    }
    return false;
}

bool HubNavigationController::openTool(const QString &pageId)
{
    return selectPage(pageId);
}

bool HubNavigationController::addFunction()
{
    if (!m_access.addFunction || !m_access.addFunction()) {
        return false;
    }
    refreshFunctions();
    return selectPage(QStringLiteral("function"));
}

void HubNavigationController::refreshFunctions()
{
    if (m_commandShell) {
        m_commandShell->refreshFunctions();
    }
}

QString HubNavigationController::currentPageId() const
{
    return m_pageRouter ? m_pageRouter->currentPageId() : QString();
}

QString HubNavigationController::currentFunctionId() const
{
    return m_access.currentFunctionId
        ? m_access.currentFunctionId().trimmed()
        : QString();
}

bool HubNavigationController::canLeaveCurrentFunctionPage(
    const QString &targetPageId
) const
{
    if (!m_pageRouter
        || m_pageRouter->currentPageId() != QStringLiteral("function")
        || targetPageId == QStringLiteral("function")) {
        return true;
    }
    return !m_access.canLeaveFunctionPage
        || m_access.canLeaveFunctionPage();
}

void HubNavigationController::clearCurrentFunction()
{
    if (m_access.clearCurrentFunction) {
        m_access.clearCurrentFunction();
    }
}

void HubNavigationController::synchronizeShell(const QString &pageId)
{
    if (m_commandShell) {
        m_commandShell->setActivePage(pageId, currentFunctionId());
    }
}
