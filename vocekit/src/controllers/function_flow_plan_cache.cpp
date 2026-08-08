#include "function_flow_plan_cache.h"

namespace {

OperationError cacheError(const QString &code)
{
    OperationError error;
    error.code = code;
    return error;
}

} // namespace

void FunctionFlowPlanCache::rebuildAll(
    const AppSettingsData &settings)
{
    m_plans.clear();
    m_errors.clear();
    for (const FunctionSettings &function : settings.functions) {
        rebuildFunction(settings, function.id);
    }
}

void FunctionFlowPlanCache::rebuildFunction(
    const AppSettingsData &settings,
    const QString &functionId)
{
    m_plans.remove(functionId);
    m_errors.remove(functionId);

    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        return;
    }
    const FunctionSettings &function =
        settings.functions.at(index);
    if (function.executionMode
        != FunctionExecutionMode::Canvas) {
        return;
    }
    const FunctionFlowState &flow = function.flow;

    const VersionedFunctionFlowGraph &published =
        flow.published;
    if (!published.supported) {
        m_errors.insert(
            functionId,
            cacheError(
                published.unavailableCode.trimmed().isEmpty()
                    ? QStringLiteral("flow_published_unavailable")
                    : published.unavailableCode
            )
        );
        return;
    }
    if (published.revision <= 0) {
        m_errors.insert(
            functionId,
            cacheError(QStringLiteral("flow_published_unavailable"))
        );
        return;
    }

    const QString actualHash =
        functionFlowGraphHash(published.graph);
    if (published.graphHash != actualHash) {
        m_errors.insert(
            functionId,
            cacheError(
                QStringLiteral("flow_published_hash_mismatch")
            )
        );
        return;
    }

    const FunctionFlowCompileResult compiled =
        FunctionFlowCompiler::compile(
            published.graph,
            published.revision,
            published.graphHash
        );
    if (!compiled.ok) {
        m_errors.insert(functionId, compiled.error);
        return;
    }

    FunctionFlowExecutionPlan plan = compiled.plan;
    plan.functionId = functionId;
    m_plans.insert(
        functionId,
        QSharedPointer<const FunctionFlowExecutionPlan>(
            new FunctionFlowExecutionPlan(plan)
        )
    );
}

QSharedPointer<const FunctionFlowExecutionPlan>
FunctionFlowPlanCache::plan(const QString &functionId) const
{
    return m_plans.value(functionId);
}

OperationError FunctionFlowPlanCache::error(
    const QString &functionId) const
{
    return m_errors.value(functionId);
}
