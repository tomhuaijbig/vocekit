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
    return pagesController()->setCurrentFunctionId(id);
}

void FunctionWorkspaceController::clearCurrentFunction()
{
    if (m_pagesController) {
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

void FunctionWorkspaceController::refreshManagementPage()
{
    if (m_pagesController) {
        m_pagesController->refreshManagementPage();
    }
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
    return runFunctionEditorCoordinator(
        id,
        title,
        custom,
        function,
        editorActions()
    );
}

bool FunctionWorkspaceController::addCustomFunction()
{
    CustomFunctionCreationActions actions;
    actions.settings = m_access.settings;
    actions.saveSettings = m_access.saveSettings;
    actions.editFunction = [this](const CustomFunctionDef &function) {
        return editFunction(function.id, function.name, true, function);
    };
    return createAndEditCustomFunction(actions);
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
    dependencies.saveSettings = m_access.saveSettings;
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
    return createFunctionPagesAccess(dependencies);
}

FunctionEditorCoordinatorActions FunctionWorkspaceController::editorActions()
{
    FunctionEditorCoordinatorActions actions;
    actions.settings = m_access.settings;
    actions.prompts = m_access.prompts;
    actions.saveSettings = m_access.saveSettings;
    actions.openDialog = m_access.openEditorDialog;
    return actions;
}
