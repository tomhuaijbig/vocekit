#ifndef VOCEKIT_FUNCTION_PAGES_CONTROLLER_H
#define VOCEKIT_FUNCTION_PAGES_CONTROLLER_H

#include "function_pages_access_factory.h"
#include "../domain/function_flow_runtime_types.h"

#include <QPointer>
#include <QString>

#include <functional>

class QWidget;

struct FunctionPagesControllerAccess
{
    std::function<FunctionPagesAccessAssembly()> accessProvider;
};

// 管理功能配置页和功能管理页的创建、缓存、当前功能及刷新生命周期。
class FunctionPagesController
{
public:
    FunctionPagesController(
        QWidget *parent,
        const FunctionPagesControllerAccess &access
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
    bool applyFunctionFlowRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    bool applyFunctionFlowRunEvent(
        const FunctionFlowRunExecutionEvent &event
    );

private:
    const FunctionPagesAccessAssembly &pageAccess();

    QWidget *m_parent = nullptr;
    FunctionPagesControllerAccess m_access;
    FunctionPagesAccessAssembly m_pageAccess;
    bool m_pageAccessLoaded = false;
    QPointer<FunctionCommandPage> m_commandPage;
    QPointer<FunctionManagementPage> m_managementPage;
    QString m_currentFunctionId;
};

#endif // VOCEKIT_FUNCTION_PAGES_CONTROLLER_H
