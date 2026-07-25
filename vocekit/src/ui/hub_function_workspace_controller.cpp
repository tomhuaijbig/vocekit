#include "hub_function_workspace_controller.h"

#include "function_editor_dialog.h"

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
            access.saveSettings = this->access.saveSettings;
            access.openEditorDialog = [this](
                const FunctionEditorDialogRequest &request,
                const FunctionEditorDialogAccess &dialogAccess
            ) {
                QWidget *parent = this->access.dialogParent
                    ? this->access.dialogParent
                    : this->access.pageParent;
                return runFunctionEditorDialog(request, dialogAccess, parent);
            };
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

void HubFunctionWorkspaceController::refreshManagementPage()
{
    if (m_impl->workspace) {
        m_impl->workspace->refreshManagementPage();
    }
}
