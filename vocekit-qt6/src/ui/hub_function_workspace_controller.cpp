#include "hub_function_workspace_controller.h"

class HubFunctionWorkspaceController::Impl
{
public:
    explicit Impl(const HubFunctionWorkspaceControllerAccess &controllerAccess)
        : access(controllerAccess)
    {
    }

    FunctionWorkspaceController *workspaceController()
    {
        if (!workspace) {
            FunctionWorkspaceControllerAccess access;
            access.settings = this->access.settings;
            access.prompts = this->access.prompts;
            access.flows = this->access.flows;
            access.saveSettings = this->access.saveSettings;
            access.functionRenamed = this->access.functionRenamed;
            access.functionRemoved = this->access.functionRemoved;
            access.operationFailed =
                this->access.operationFailed;
            workspace.reset(
                new FunctionWorkspaceController(this->access.pageParent, access)
            );
        }
        return workspace.data();
    }

    HubFunctionWorkspaceControllerAccess access;
    QScopedPointer<FunctionWorkspaceController> workspace;
};

HubFunctionWorkspaceController::HubFunctionWorkspaceController(
    const HubFunctionWorkspaceControllerAccess &access
)
    : m_impl(new Impl(access))
{
}

HubFunctionWorkspaceController::~HubFunctionWorkspaceController() = default;

QWidget *HubFunctionWorkspaceController::page()
{
    return m_impl->workspaceController()->commandPage();
}

QString HubFunctionWorkspaceController::currentFunctionId() const
{
    return m_impl->workspace
        ? m_impl->workspace->currentFunctionId()
        : QString();
}

bool HubFunctionWorkspaceController::selectFunction(const QString &id)
{
    return m_impl->workspaceController()->setCurrentFunctionId(id);
}

void HubFunctionWorkspaceController::clearFunction()
{
    if (m_impl->workspace) {
        m_impl->workspace->clearCurrentFunction();
    }
}

bool HubFunctionWorkspaceController::addFunction()
{
    return m_impl->workspaceController()->addCustomFunction();
}

bool HubFunctionWorkspaceController::editFunction(
    const QString &id,
    const QString &title,
    bool custom,
    const CustomFunctionDef &function
)
{
    return m_impl->workspaceController()->editFunction(
        id,
        title,
        custom,
        function
    );
}

void HubFunctionWorkspaceController::refreshActivePage()
{
    if (m_impl->workspace) {
        m_impl->workspace->refreshCommandPage();
    }
}

void HubFunctionWorkspaceController::refreshActiveCanvas()
{
    if (m_impl->workspace) {
        m_impl->workspace->refreshCanvasState();
    }
}

void HubFunctionWorkspaceController::refreshManagementPage()
{
    if (m_impl->workspace) {
        m_impl->workspace->refreshManagementPage();
    }
}

bool HubFunctionWorkspaceController::flushAllPendingFlowDrafts()
{
    return !m_impl->workspace
        || m_impl->workspace->flushPendingFlowDraft();
}

void HubFunctionWorkspaceController::discardAllPendingFlowDrafts()
{
    if (m_impl->workspace) {
        m_impl->workspace->discardPendingFlowDraft();
    }
}

bool HubFunctionWorkspaceController::canLeaveFunctionPage()
{
    return flushAllPendingFlowDrafts();
}

bool HubFunctionWorkspaceController::applyFunctionFlowRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    return m_impl->workspace
        && m_impl->workspace->applyFunctionFlowRuntimeEvent(event);
}

bool HubFunctionWorkspaceController::applyFunctionFlowRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    return m_impl->workspace
        && m_impl->workspace->applyFunctionFlowRunEvent(event);
}
