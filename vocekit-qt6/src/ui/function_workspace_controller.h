#ifndef VOCEKIT_FUNCTION_WORKSPACE_CONTROLLER_H
#define VOCEKIT_FUNCTION_WORKSPACE_CONTROLLER_H

#include "function_editor_coordinator.h"
#include "function_pages_controller.h"

#include <QScopedPointer>

#include <functional>

class HubSettingsState;
class QWidget;

struct FunctionWorkspaceControllerAccess
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    FunctionFlowSettingsAccess flows;
    std::function<void()> saveSettings;
    std::function<void(const QString &)> functionRenamed;
    std::function<void(const QString &)> functionRemoved;
    std::function<void(const OperationError &)> operationFailed;
};

// 集中管理内置功能页面及新增流程，不再打开独立编辑窗口。
class FunctionWorkspaceController
{
public:
    FunctionWorkspaceController(
        QWidget *parent,
        const FunctionWorkspaceControllerAccess &access
    );

    QWidget *commandPage();
    FunctionCommandPage *commandPageWidget();
    bool commandPageCreated() const;

    QWidget *managementPage();
    FunctionManagementPage *managementPageWidget();
    bool managementPageCreated() const;

    bool setCurrentFunctionId(const QString &id);
    void clearCurrentFunction();
    QString currentFunctionId() const;

    void refreshCommandPage();
    void refreshCanvasState();
    void refreshManagementPage();
    bool flushPendingFlowDraft();
    void discardPendingFlowDraft();
    bool canLeaveFunctionPage();
    bool applyFunctionFlowRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    bool applyFunctionFlowRunEvent(
        const FunctionFlowRunExecutionEvent &event
    );

    QString summaryText(const QString &id, const QString &shortcut);
    bool editFunction(
        const QString &id,
        const QString &title,
        bool custom,
        const CustomFunctionDef &function
    );
    bool addCustomFunction();

private:
    FunctionPagesController *pagesController();
    FunctionPagesAccessAssembly buildPagesAccess();
    FunctionEditorCoordinatorActions editorActions();

    QWidget *m_parent = nullptr;
    FunctionWorkspaceControllerAccess m_access;
    QScopedPointer<FunctionPagesController> m_pagesController;
};

#endif // VOCEKIT_FUNCTION_WORKSPACE_CONTROLLER_H
