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
            if (!m_commandPage->setFunctionId(m_currentFunctionId)) {
                m_currentFunctionId.clear();
            }
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
    if (m_commandPage
        && !m_commandPage->setFunctionId(normalized)) {
        return false;
    }
    m_currentFunctionId = normalized;
    return true;
}

void FunctionPagesController::clearCurrentFunction()
{
    if (m_commandPage
        && !m_commandPage->setFunctionId(QString())) {
        return;
    }
    m_currentFunctionId.clear();
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

void FunctionPagesController::refreshCanvasState()
{
    if (m_commandPage) {
        m_commandPage->refreshCanvasState();
    }
}

void FunctionPagesController::refreshManagementPage()
{
    if (m_managementPage) {
        m_managementPage->refresh();
    }
}

bool FunctionPagesController::applyFunctionFlowRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    return m_commandPage
        && m_currentFunctionId == event.functionId
        && m_commandPage->applyFunctionFlowRuntimeEvent(event);
}

bool FunctionPagesController::applyFunctionFlowRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    return m_commandPage
        && m_currentFunctionId == event.functionId
        && m_commandPage->applyFunctionFlowRunEvent(event);
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
