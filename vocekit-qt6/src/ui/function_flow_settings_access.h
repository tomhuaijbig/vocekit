#ifndef VOCEKIT_FUNCTION_FLOW_SETTINGS_ACCESS_H
#define VOCEKIT_FUNCTION_FLOW_SETTINGS_ACCESS_H

#include "../domain/function_flow_publication_types.h"
#include "../domain/function_settings.h"

#include <functional>

// 画布 UI 只依赖这些窄操作，不持有发布服务或设置存储。
struct FunctionFlowSettingsAccess
{
    std::function<bool(
        const QString &,
        FunctionFlowState *,
        OperationError *
    )> readState;
    std::function<FunctionFlowDraftAnalysis(
        const QString &,
        const FunctionFlowGraph &
    )> analyzeDraft;
    std::function<bool(
        const FunctionSettings &,
        OperationError *
    )> addCustomFunction;
    std::function<bool(
        const QString &,
        int,
        const FunctionFlowGraph &,
        int *,
        OperationError *
    )> updateDraft;
    std::function<bool(
        const QString &,
        const FunctionFlowEditorState &,
        OperationError *
    )> updateEditorState;
    std::function<FunctionFlowPublishResult(
        const QString &,
        int,
        bool
    )> publish;
    std::function<bool(
        const QString &,
        FunctionExecutionMode,
        OperationError *
    )> setExecutionMode;
    std::function<bool(
        const QString &,
        const QString &,
        OperationError *
    )> renameCustomFunction;
    std::function<bool(
        const QString &,
        OperationError *
    )> removeCustomFunction;
};

#endif // VOCEKIT_FUNCTION_FLOW_SETTINGS_ACCESS_H
