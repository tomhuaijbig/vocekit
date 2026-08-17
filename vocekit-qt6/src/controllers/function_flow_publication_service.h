#ifndef VOCEKIT_FUNCTION_FLOW_PUBLICATION_SERVICE_H
#define VOCEKIT_FUNCTION_FLOW_PUBLICATION_SERVICE_H

#include "../config/app_settings_data.h"
#include "../domain/function_flow_compiler.h"
#include "../domain/function_flow_publication_types.h"

#include <functional>

struct FunctionFlowPublicationAccess
{
    std::function<AppSettingsData()> settingsSnapshotProvider;
    std::function<bool(
        const AppSettingsData &,
        OperationError *
    )> replaceAndSave;
    std::function<FunctionFlowValidationContext(
        const AppSettingsData &,
        const QString &
    )> validationContextProvider;
    std::function<FunctionFlowCompileResult(
        const FunctionFlowGraph &,
        int,
        const QString &
    )> compileGraph;
    std::function<void(
        const QString &,
        const QString &
    )> publishSettingsChanged;
};

class FunctionFlowPublicationService
{
public:
    explicit FunctionFlowPublicationService(
        const FunctionFlowPublicationAccess &access =
            FunctionFlowPublicationAccess()
    );

    bool readState(
        const QString &functionId,
        FunctionFlowState *state,
        OperationError *error
    ) const;

    FunctionFlowDraftAnalysis analyzeDraft(
        const QString &functionId,
        const FunctionFlowGraph &draft
    ) const;

    bool addCustomFunction(
        const FunctionSettings &functionWithoutFlow,
        OperationError *error
    );

    bool updateDraft(
        const QString &functionId,
        int expectedRevision,
        const FunctionFlowGraph &draft,
        int *savedRevision,
        OperationError *error
    );

    bool updateEditorState(
        const QString &functionId,
        const FunctionFlowEditorState &editor,
        OperationError *error
    );

    FunctionFlowPublishResult publish(
        const QString &functionId,
        int expectedDraftRevision,
        bool replaceCorruptPublished = false
    );

    bool setExecutionMode(
        const QString &functionId,
        FunctionExecutionMode mode,
        OperationError *error
    );

    bool removeCustomFunction(
        const QString &functionId,
        OperationError *error
    );

private:
    FunctionFlowPublicationAccess m_access;
};

#endif // VOCEKIT_FUNCTION_FLOW_PUBLICATION_SERVICE_H
