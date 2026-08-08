#ifndef VOCEKIT_FUNCTION_FLOW_EDITOR_CONTROLLER_H
#define VOCEKIT_FUNCTION_FLOW_EDITOR_CONTROLLER_H

#include "../domain/function_flow_publication_types.h"
#include "../domain/function_settings.h"
#include "../ui/function_flow_settings_access.h"

#include <QObject>
#include <QStringList>

class FunctionFlowGraphCommand;
class QTimer;
class QUndoStack;

struct FunctionFlowPlacementDefaults
{
    FunctionSettings function;
    QString speechProviderId;
    QString ocrEngineId;
    int resultPopupOpacity = 100;
};

enum class FunctionFlowEditorPublishOutcome
{
    Succeeded,
    RepairConfirmationRequired,
    Failed,
    Blocked
};

struct FunctionFlowEditorPublishResult
{
    FunctionFlowEditorPublishOutcome outcome =
        FunctionFlowEditorPublishOutcome::Failed;
    int publishedRevision = 0;
    FunctionFlowValidationResult validation;
    OperationError error;
};

// 持有单个功能的草稿工作副本、撤销栈和保存/发布状态机。
class FunctionFlowEditorController : public QObject
{
    Q_OBJECT

public:
    explicit FunctionFlowEditorController(
        const FunctionFlowSettingsAccess &access,
        QObject *parent = nullptr
    );
    ~FunctionFlowEditorController() override;

    bool openFunction(
        const QString &functionId,
        const FunctionFlowPlacementDefaults &defaults =
            FunctionFlowPlacementDefaults()
    );
    bool refreshFromAccess();
    void observeRemoteState(const FunctionFlowState &state);
    bool reloadRemoteDraft();

    QString functionId() const;
    const FunctionFlowGraph &graph() const;
    const FunctionFlowEditorState &editorState() const;
    const FunctionFlowDraftAnalysis &analysis() const;
    const FunctionFlowState &flowState() const;
    QUndoStack *undoStack() const;

    int baseDraftRevision() const;
    int observedRemoteRevision() const;
    bool isDirty() const;
    bool editable() const;
    bool hasRemoteConflict() const;
    bool publicationBusy() const;
    QString unavailableCode() const;
    OperationError lastError() const;

    void setSaveDebounceMs(int milliseconds);
    void setEditorSaveDebounceMs(int milliseconds);

    QString placeNode(
        FunctionFlowNodeType type,
        const QPointF &position
    );
    bool addConnection(
        const FunctionFlowEndpoint &from,
        const FunctionFlowEndpoint &to
    );
    bool removeNode(const QString &nodeId);
    bool removeEdge(const QString &edgeId);
    bool commitNodePosition(
        const QString &nodeId,
        const QPointF &position
    );
    bool updateNode(const FunctionFlowNode &node);
    bool reorderOutputActions(
        const QString &outputNodeId,
        const QStringList &orderedEdgeIds
    );

    bool flushPendingSave();
    void updateEditorState(const FunctionFlowEditorState &editor);
    bool flushPendingEditorState();
    bool flushAllPendingSaves();
    void discardPendingSaves();

    FunctionFlowEditorPublishResult publishFlow(
        bool replaceCorruptPublished = false
    );

signals:
    void graphChanged();
    void editorStateChanged();
    void analysisChanged();
    void stateChanged();
    void publicationBusyChanged(bool busy);

private:
    friend class FunctionFlowGraphCommand;

    void applyCommandGraph(const FunctionFlowGraph &graph);
    bool pushGraphChange(
        const FunctionFlowGraph &graph,
        const QString &text
    );
    void handleUndoIndexChanged();
    void analyzeWorkingGraph();
    void scheduleDraftSave();
    void applyRemoteState(const FunctionFlowState &state);
    void loadState(const FunctionFlowState &state);
    void clearForMissingFunction();
    FunctionFlowNode defaultNode(
        FunctionFlowNodeType type,
        const QPointF &position
    ) const;
    void setLastError(
        const OperationError &error = OperationError()
    );

    FunctionFlowSettingsAccess m_access;
    FunctionFlowPlacementDefaults m_defaults;
    QString m_functionId;
    FunctionFlowState m_state;
    FunctionFlowGraph m_graph;
    FunctionFlowEditorState m_editorState;
    FunctionFlowDraftAnalysis m_analysis;
    QUndoStack *m_undoStack = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_editorSaveTimer = nullptr;
    int m_baseDraftRevision = 0;
    int m_observedRemoteRevision = 0;
    quint64 m_graphGeneration = 0;
    bool m_editable = false;
    bool m_remoteConflict = false;
    bool m_localSaveInProgress = false;
    bool m_hasDeferredState = false;
    FunctionFlowState m_deferredState;
    bool m_editorSavePending = false;
    bool m_publicationBusy = false;
    bool m_discardOnDestruction = false;
    OperationError m_lastError;
};

#endif // VOCEKIT_FUNCTION_FLOW_EDITOR_CONTROLLER_H
