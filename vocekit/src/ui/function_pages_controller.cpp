#include "function_pages_controller.h"

FunctionPagesController::FunctionPagesController(
    QWidget *parent,
    const FunctionPagesControllerAccess &access
)
    : m_parent(parent),
      m_access(access)
{
}

QWidget *FunctionPagesController::commandPage()
{
    return commandPageWidget();
}

FunctionCommandPage *FunctionPagesController::commandPageWidget()
{
    if (!m_commandPage) {
        m_commandPage = new FunctionCommandPage(pageAccess().command, m_parent);
        if (!m_currentFunctionId.isEmpty()) {
            m_commandPage->setFunctionId(m_currentFunctionId);
        }
    }
    return m_commandPage;
}

bool FunctionPagesController::commandPageCreated() const
{
    return !m_commandPage.isNull();
}

QWidget *FunctionPagesController::managementPage()
{
    return managementPageWidget();
}

FunctionManagementPage *FunctionPagesController::managementPageWidget()
{
    if (!m_managementPage) {
        m_managementPage = new FunctionManagementPage(
            pageAccess().management,
            m_parent
        );
    }
    return m_managementPage;
}

bool FunctionPagesController::managementPageCreated() const
{
    return !m_managementPage.isNull();
}

bool FunctionPagesController::setCurrentFunctionId(const QString &id)
{
    const QString normalized = id.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    m_currentFunctionId = normalized;
    if (m_commandPage) {
        m_commandPage->setFunctionId(normalized);
    }
    return true;
}

void FunctionPagesController::clearCurrentFunction()
{
    m_currentFunctionId.clear();
    if (m_commandPage) {
        m_commandPage->setFunctionId(QString());
    }
}

QString FunctionPagesController::currentFunctionId() const
{
    return m_currentFunctionId;
}

void FunctionPagesController::refreshCommandPage()
{
    if (m_commandPage) {
        m_commandPage->refresh();
    }
}

void FunctionPagesController::refreshManagementPage()
{
    if (m_managementPage) {
        m_managementPage->refresh();
    }
}

const FunctionPagesAccessAssembly &FunctionPagesController::pageAccess()
{
    if (!m_pageAccessLoaded) {
        m_pageAccess = m_access.accessProvider
            ? m_access.accessProvider()
            : FunctionPagesAccessAssembly();
        m_pageAccessLoaded = true;
    }
    return m_pageAccess;
}
