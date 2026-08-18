#include "function_workspace_controller.h"

#include "custom_function_creation_coordinator.h"
#include "function_pages_access_factory.h"

FunctionWorkspaceController::FunctionWorkspaceController(
    QWidget *parent,
    const FunctionWorkspaceControllerAccess &access
)
    : m_parent(parent),
      m_access(access)
{
}

QWidget *FunctionWorkspaceController::commandPage()
{
    return pagesController()->commandPage();
}

FunctionCommandPage *FunctionWorkspaceController::commandPageWidget()
{
    return pagesController()->commandPageWidget();
}

bool FunctionWorkspaceController::commandPageCreated() const
{
    return m_pagesController && m_pagesController->commandPageCreated();
}

QWidget *FunctionWorkspaceController::managementPage()
{
    return pagesController()->managementPage();
}

FunctionManagementPage *FunctionWorkspaceController::managementPageWidget()
{
    return pagesController()->managementPageWidget();
}

bool FunctionWorkspaceController::managementPageCreated() const
{
    return m_pagesController && m_pagesController->managementPageCreated();
}

bool FunctionWorkspaceController::setCurrentFunctionId(const QString &id)
{
    const QString normalized = id.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (normalized != currentFunctionId()
        && !flushPendingFlowDraft()) {
        return false;
    }
    return pagesController()->setCurrentFunctionId(normalized);
}

void FunctionWorkspaceController::clearCurrentFunction()
{
    if (m_pagesController) {
        if (!flushPendingFlowDraft()) {
            return;
        }
        m_pagesController->clearCurrentFunction();
    }
}

QString FunctionWorkspaceController::currentFunctionId() const
{
    return m_pagesController
        ? m_pagesController->currentFunctionId()
        : QString();
}

void FunctionWorkspaceController::refreshCommandPage()
{
    if (m_pagesController) {
        m_pagesController->refreshCommandPage();
    }
}

void FunctionWorkspaceController::refreshCanvasState()
{
    if (m_pagesController) {
        m_pagesController->refreshCanvasState();
    }
}

void FunctionWorkspaceController::refreshManagementPage()
{
    if (m_pagesController) {
        m_pagesController->refreshManagementPage();
    }
}

bool FunctionWorkspaceController::flushPendingFlowDraft()
{
    if (!m_pagesController
        || !m_pagesController->commandPageCreated()) {
        return true;
    }
    return m_pagesController->commandPageWidget()
        ->flushPendingFlowDraft();
}

void FunctionWorkspaceController::discardPendingFlowDraft()
{
    if (m_pagesController
        && m_pagesController->commandPageCreated()) {
        m_pagesController->commandPageWidget()
            ->discardPendingFlowDraft();
    }
}

bool FunctionWorkspaceController::canLeaveFunctionPage()
{
    return flushPendingFlowDraft();
}

bool FunctionWorkspaceController::applyFunctionFlowRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    return m_pagesController
        && currentFunctionId() == event.functionId
        && m_pagesController->applyFunctionFlowRuntimeEvent(event);
}

bool FunctionWorkspaceController::applyFunctionFlowRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    return m_pagesController
        && currentFunctionId() == event.functionId
        && m_pagesController->applyFunctionFlowRunEvent(event);
}

QString FunctionWorkspaceController::summaryText(
    const QString &id,
    const QString &shortcut
)
{
    return functionEditorSummaryText(id, shortcut, editorActions());
}

bool FunctionWorkspaceController::editFunction(
    const QString &id,
    const QString &title,
    bool custom,
    const CustomFunctionDef &function
)
{
    Q_UNUSED(title);
    Q_UNUSED(custom);
    Q_UNUSED(function);
    return setCurrentFunctionId(id);
}

bool FunctionWorkspaceController::addCustomFunction()
{
    if (!canLeaveFunctionPage()) {
        return false;
    }
    CustomFunctionCreationActions actions;
    actions.settings = m_access.settings;
    actions.flows = m_access.flows;
    OperationError error;
    const QString id = createCustomFunction(actions, &error);
    if (id.isEmpty() && m_access.operationFailed) {
        m_access.operationFailed(error);
    }
    return !id.isEmpty() && setCurrentFunctionId(id);
}

FunctionPagesController *FunctionWorkspaceController::pagesController()
{
    if (!m_pagesController) {
        FunctionPagesControllerAccess access;
        access.accessProvider = [this]() {
            return buildPagesAccess();
        };
        m_pagesController.reset(new FunctionPagesController(m_parent, access));
    }
    return m_pagesController.data();
}

FunctionPagesAccessAssembly FunctionWorkspaceController::buildPagesAccess()
{
    FunctionPagesAccessDependencies dependencies;
    dependencies.settings = m_access.settings;
    dependencies.prompts = m_access.prompts;
    dependencies.flows = m_access.flows;
    dependencies.saveSettings = m_access.saveSettings;
    dependencies.functionRenamed = m_access.functionRenamed;
    dependencies.functionRemoved = [this](const QString &id) {
        pagesController()->clearCurrentFunction();
        if (m_access.functionRemoved) {
            m_access.functionRemoved(id);
        }
    };
    dependencies.summaryProvider = [this](
        const QString &id,
        const QString &shortcut
    ) {
        return summaryText(id, shortcut);
    };
    dependencies.addFunction = [this]() {
        addCustomFunction();
    };
    dependencies.editFunction = [this](
        const QString &id,
        const QString &title,
        bool custom,
        const CustomFunctionDef &function
    ) {
        editFunction(id, title, custom, function);
    };
    dependencies.operationFailed = m_access.operationFailed;
    return createFunctionPagesAccess(dependencies);
}

FunctionEditorCoordinatorActions FunctionWorkspaceController::editorActions()
{
    FunctionEditorCoordinatorActions actions;
    actions.settings = m_access.settings;
    actions.prompts = m_access.prompts;
    return actions;
}
