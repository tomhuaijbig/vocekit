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
    QWidget *pageParent = nullptr;
    QWidget *dialogParent = nullptr;
    std::function<void()> saveSettings;
};

// 隔离主窗口与功能页面、编辑弹窗之间的组装细节。
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
    void refreshManagementPage();

private:
    Q_DISABLE_COPY(HubFunctionWorkspaceController)

    class Impl;
    QScopedPointer<Impl> m_impl;
};

#endif // VOCEKIT_HUB_FUNCTION_WORKSPACE_CONTROLLER_H
