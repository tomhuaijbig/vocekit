#ifndef VOCEKIT_FUNCTION_FLOW_PLAN_CACHE_H
#define VOCEKIT_FUNCTION_FLOW_PLAN_CACHE_H

#include "../config/app_settings_data.h"
#include "../domain/function_flow_compiler.h"

#include <QMap>
#include <QSharedPointer>

class FunctionFlowPlanCache
{
public:
    void rebuildAll(const AppSettingsData &settings);
    void rebuildFunction(
        const AppSettingsData &settings,
        const QString &functionId
    );
    QSharedPointer<const FunctionFlowExecutionPlan> plan(
        const QString &functionId
    ) const;
    OperationError error(const QString &functionId) const;

private:
    QMap<
        QString,
        QSharedPointer<const FunctionFlowExecutionPlan>
    > m_plans;
    QMap<QString, OperationError> m_errors;
};

#endif // VOCEKIT_FUNCTION_FLOW_PLAN_CACHE_H
