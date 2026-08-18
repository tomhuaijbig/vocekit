#ifndef VOCEKIT_HUB_FUNCTION_WORKSPACE_CONTROLLER_H
#define VOCEKIT_HUB_FUNCTION_WORKSPACE_CONTROLLER_H

#include "function_workspace_controller.h"

#include <QScopedPointer>

#include <functional>

class HubSettingsState;
class QWidget;

struct HubFunctionWorkspaceControllerAccess
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    FunctionFlowSettingsAccess flows;
    QWidget *pageParent = nullptr;
    std::function<void()> saveSettings;
    std::function<void(const QString &)> functionRenamed;
    std::function<void(const QString &)> functionRemoved;
    std::function<void(const OperationError &)> operationFailed;
};

// 隔离主窗口与内置功能页面之间的组装细节。
class HubFunctionWorkspaceController
{
public:
    explicit HubFunctionWorkspaceController(
        const HubFunctionWorkspaceControllerAccess &access
    );
    ~HubFunctionWorkspaceController();

    QWidget *page();
    QString currentFunctionId() const;
    bool selectFunction(const QString &id);
    void clearFunction();
    bool addFunction();
    bool editFunction(
        const QString &id,
        const QString &title,
        bool custom,
        const CustomFunctionDef &function
    );

    void refreshActivePage();
    void refreshActiveCanvas();
    void refreshManagementPage();
    bool flushAllPendingFlowDrafts();
    void discardAllPendingFlowDrafts();
    bool canLeaveFunctionPage();
    bool applyFunctionFlowRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    bool applyFunctionFlowRunEvent(
        const FunctionFlowRunExecutionEvent &event
    );

private:
    Q_DISABLE_COPY(HubFunctionWorkspaceController)

    class Impl;
    QScopedPointer<Impl> m_impl;
};

#endif // VOCEKIT_HUB_FUNCTION_WORKSPACE_CONTROLLER_H
