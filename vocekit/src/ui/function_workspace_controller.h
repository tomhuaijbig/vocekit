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
    std::function<void()> saveSettings;
    std::function<bool(
        const FunctionEditorDialogRequest &,
        const FunctionEditorDialogAccess &
    )> openEditorDialog;
};

// 集中管理功能页面、功能新增和功能编辑流程，主窗口只负责提供外部动作。
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
    void refreshManagementPage();

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
