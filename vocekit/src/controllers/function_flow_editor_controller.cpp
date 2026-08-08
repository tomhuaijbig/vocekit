#include "function_flow_editor_controller.h"

#include "../domain/function_flow_ports.h"

#include <QJsonObject>
#include <QSignalBlocker>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

#include <climits>

namespace {

bool recordingEqual(
    const FunctionFlowRecordingConfig &left,
    const FunctionFlowRecordingConfig &right)
{
    return left.triggerMode == right.triggerMode
        && left.longRecordingEnabled == right.longRecordingEnabled
        && left.segmentSeconds == right.segmentSeconds
        && left.maximumMinutes == right.maximumMinutes
        && left.countdownSeconds == right.countdownSeconds
        && left.beepEnabled == right.beepEnabled
        && left.beepPath == right.beepPath;
}

bool configEqual(
    const FunctionFlowNodeConfig &left,
    const FunctionFlowNodeConfig &right)
{
    return left.voice.speechProviderId
            == right.voice.speechProviderId
        && recordingEqual(
            left.voice.recording,
            right.voice.recording
        )
        && left.voice.acquisitionSequence
            == right.voice.acquisitionSequence
        && left.voice.networkPolicy == right.voice.networkPolicy
        && left.selection.inheritStrongSelection
            == right.selection.inheritStrongSelection
        && left.selection.acquisitionSequence
            == right.selection.acquisitionSequence
        && left.screenshot.ocrEngineId
            == right.screenshot.ocrEngineId
        && left.screenshot.timeoutMs == right.screenshot.timeoutMs
        && left.screenshot.triggerMode
            == right.screenshot.triggerMode
        && left.screenshot.separateShortcut
            == right.screenshot.separateShortcut
        && left.screenshot.acquisitionSequence
            == right.screenshot.acquisitionSequence
        && left.screenshot.networkPolicy
            == right.screenshot.networkPolicy
        && left.input.role == right.input.role
        && left.input.sequence == right.input.sequence
        && left.input.required == right.input.required
        && left.model.modelId == right.model.modelId
        && left.model.promptId == right.model.promptId
        && left.model.stream == right.model.stream
        && left.model.networkPolicy == right.model.networkPolicy
        && left.output.emptyResultPolicy
            == right.output.emptyResultPolicy
        && left.popup.resultTemplate
            == right.popup.resultTemplate
        && left.popup.resultActions
            == right.popup.resultActions
        && left.popup.displaySeconds
            == right.popup.displaySeconds
        && left.popup.opacity == right.popup.opacity
        && left.screenshotPanel.displaySeconds
            == right.screenshotPanel.displaySeconds
        && left.screenshotPanel.opacity
            == right.screenshotPanel.opacity
        && left.autoWrite.writeMode == right.autoWrite.writeMode
        && left.autoWrite.fallbackToPopup
            == right.autoWrite.fallbackToPopup;
}

bool nodeEqual(
    const FunctionFlowNode &left,
    const FunctionFlowNode &right)
{
    return left.id == right.id
        && left.type == right.type
        && left.title == right.title
        && left.position == right.position
        && left.enabled == right.enabled
        && configEqual(left.config, right.config)
        && left.retainedValues == right.retainedValues;
}

bool edgeEqual(
    const FunctionFlowEdge &left,
    const FunctionFlowEdge &right)
{
    return left.id == right.id
        && left.fromNodeId == right.fromNodeId
        && left.fromPortId == right.fromPortId
        && left.toNodeId == right.toNodeId
        && left.toPortId == right.toPortId
        && left.order == right.order
        && left.retainedValues == right.retainedValues;
}

bool graphEqual(
    const FunctionFlowGraph &leftInput,
    const FunctionFlowGraph &rightInput)
{
    const FunctionFlowGraph left =
        normalizeFunctionFlowGraph(leftInput);
    const FunctionFlowGraph right =
        normalizeFunctionFlowGraph(rightInput);
    if (left.schemaVersion != right.schemaVersion
        || left.retainedValues != right.retainedValues
        || left.nodes.size() != right.nodes.size()
        || left.edges.size() != right.edges.size()) {
        return false;
    }
    for (int index = 0; index < left.nodes.size(); ++index) {
        if (!nodeEqual(left.nodes.at(index), right.nodes.at(index))) {
            return false;
        }
    }
    for (int index = 0; index < left.edges.size(); ++index) {
        if (!edgeEqual(left.edges.at(index), right.edges.at(index))) {
            return false;
        }
    }
    return true;
}

int nodeIndex(
    const FunctionFlowGraph &graph,
    const QString &nodeId)
{
    for (int index = 0; index < graph.nodes.size(); ++index) {
        if (graph.nodes.at(index).id == nodeId) {
            return index;
        }
    }
    return -1;
}

int edgeIndex(
    const FunctionFlowGraph &graph,
    const QString &edgeId)
{
    for (int index = 0; index < graph.edges.size(); ++index) {
        if (graph.edges.at(index).id == edgeId) {
            return index;
        }
    }
    return -1;
}

const FunctionFlowPortSpec *portSpec(
    FunctionFlowNodeType type,
    const QString &portId,
    FunctionFlowPortDirection direction,
    FunctionFlowPortSpec *storage)
{
    const QVector<FunctionFlowPortSpec> specs =
        functionFlowPortSpecs(type);
    for (const FunctionFlowPortSpec &spec : specs) {
        if (spec.id == portId && spec.direction == direction) {
            if (storage) {
                *storage = spec;
                return storage;
            }
            return nullptr;
        }
    }
    return nullptr;
}

bool uniqueNodeType(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::VoiceSource
        || type == FunctionFlowNodeType::SelectionSource
        || type == FunctionFlowNodeType::ScreenshotSource
        || type == FunctionFlowNodeType::AutoWrite;
}

QStringList flowPopupActions(
    const QStringList &configuredActions)
{
    QStringList actions;
    for (const QString &configured : configuredActions) {
        const QString id = configured.trimmed();
        if (isFunctionFlowPopupActionSupported(id)
            && !actions.contains(id)) {
            actions.append(id);
        }
    }
    return actions.isEmpty()
        ? defaultFunctionFlowPopupActionIds()
        : actions;
}

QString defaultNodeTitle(FunctionFlowNodeType type)
{
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        return QString::fromUtf8("语音采集");
    case FunctionFlowNodeType::SelectionSource:
        return QString::fromUtf8("选中文字");
    case FunctionFlowNodeType::ScreenshotSource:
        return QString::fromUtf8("截图识别");
    case FunctionFlowNodeType::Input:
        return QString::fromUtf8("输入节点");
    case FunctionFlowNodeType::Model:
        return QString::fromUtf8("调用大模型");
    case FunctionFlowNodeType::Output:
        return QString::fromUtf8("输出节点");
    case FunctionFlowNodeType::ResultPopup:
        return QString::fromUtf8("结果小框");
    case FunctionFlowNodeType::ScreenshotPanel:
        return QString::fromUtf8("截图对照窗");
    case FunctionFlowNodeType::AutoWrite:
        return QString::fromUtf8("自动写入");
    }
    return QString::fromUtf8("节点");
}

int countType(
    const FunctionFlowGraph &graph,
    FunctionFlowNodeType type)
{
    int count = 0;
    for (const FunctionFlowNode &node : graph.nodes) {
        if (node.type == type) {
            ++count;
        }
    }
    return count;
}

OperationError errorWithCode(
    const QString &code,
    const QString &message = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    return error;
}

} // namespace

class FunctionFlowGraphCommand : public QUndoCommand
{
public:
    FunctionFlowGraphCommand(
        FunctionFlowEditorController *controller,
        const FunctionFlowGraph &before,
        const FunctionFlowGraph &after,
        const QString &text)
        : QUndoCommand(text),
          m_controller(controller),
          m_before(before),
          m_after(after)
    {
    }

    void undo() override
    {
        if (m_controller) {
            m_controller->applyCommandGraph(m_before);
        }
    }

    void redo() override
    {
        if (m_controller) {
            m_controller->applyCommandGraph(m_after);
        }
    }

private:
    FunctionFlowEditorController *m_controller = nullptr;
    FunctionFlowGraph m_before;
    FunctionFlowGraph m_after;
};

FunctionFlowEditorController::FunctionFlowEditorController(
    const FunctionFlowSettingsAccess &access,
    QObject *parent)
    : QObject(parent),
      m_access(access),
      m_undoStack(new QUndoStack(this)),
      m_saveTimer(new QTimer(this)),
      m_editorSaveTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    m_editorSaveTimer->setSingleShot(true);
    m_editorSaveTimer->setInterval(500);
    connect(
        m_saveTimer,
        &QTimer::timeout,
        this,
        [this]() { flushPendingSave(); }
    );
    connect(
        m_editorSaveTimer,
        &QTimer::timeout,
        this,
        [this]() { flushPendingEditorState(); }
    );
    connect(
        m_undoStack,
        &QUndoStack::indexChanged,
        this,
        [this](int) { handleUndoIndexChanged(); }
    );
    connect(
        m_undoStack,
        &QUndoStack::cleanChanged,
        this,
        [this](bool) { Q_EMIT stateChanged(); }
    );
}

FunctionFlowEditorController::~FunctionFlowEditorController()
{
    if (!m_discardOnDestruction) {
        flushAllPendingSaves();
    }
}

bool FunctionFlowEditorController::openFunction(
    const QString &functionId,
    const FunctionFlowPlacementDefaults &defaults)
{
    const QString normalized = functionId.trimmed();
    if (normalized == m_functionId && !normalized.isEmpty()) {
        m_defaults = defaults;
        return true;
    }
    if (!m_functionId.isEmpty() && !flushAllPendingSaves()) {
        return false;
    }
    if (normalized.isEmpty()) {
        clearForMissingFunction();
        return true;
    }
    if (!m_access.readState) {
        setLastError(errorWithCode(
            QStringLiteral("flow_state_read_unavailable"),
            QString::fromUtf8("流程状态读取服务不可用。")
        ));
        return false;
    }

    FunctionFlowState state;
    OperationError error;
    if (!m_access.readState(normalized, &state, &error)) {
        setLastError(error.isEmpty()
            ? errorWithCode(QStringLiteral("flow_state_read_failed"))
            : error);
        return false;
    }
    m_functionId = normalized;
    m_defaults = defaults;
    loadState(state);
    setLastError();
    return true;
}

bool FunctionFlowEditorController::refreshFromAccess()
{
    if (m_functionId.isEmpty() || !m_access.readState) {
        return false;
    }
    FunctionFlowState state;
    OperationError error;
    if (!m_access.readState(m_functionId, &state, &error)) {
        setLastError(error.isEmpty()
            ? errorWithCode(QStringLiteral("flow_state_read_failed"))
            : error);
        return false;
    }
    observeRemoteState(state);
    return true;
}

void FunctionFlowEditorController::observeRemoteState(
    const FunctionFlowState &state)
{
    if (m_functionId.isEmpty()) {
        return;
    }
    if (m_localSaveInProgress) {
        if (!m_hasDeferredState
            || state.draft.revision
                >= m_deferredState.draft.revision) {
            m_deferredState = state;
            m_hasDeferredState = true;
        }
        return;
    }
    applyRemoteState(state);
}

bool FunctionFlowEditorController::reloadRemoteDraft()
{
    if (m_functionId.isEmpty() || !m_access.readState) {
        return false;
    }
    FunctionFlowState state;
    OperationError error;
    if (!m_access.readState(m_functionId, &state, &error)) {
        setLastError(error.isEmpty()
            ? errorWithCode(QStringLiteral("flow_state_read_failed"))
            : error);
        return false;
    }
    loadState(state);
    setLastError();
    return true;
}

QString FunctionFlowEditorController::functionId() const
{
    return m_functionId;
}

const FunctionFlowGraph &FunctionFlowEditorController::graph() const
{
    return m_graph;
}

const FunctionFlowEditorState &
FunctionFlowEditorController::editorState() const
{
    return m_editorState;
}

const FunctionFlowDraftAnalysis &
FunctionFlowEditorController::analysis() const
{
    return m_analysis;
}

const FunctionFlowState &
FunctionFlowEditorController::flowState() const
{
    return m_state;
}

QUndoStack *FunctionFlowEditorController::undoStack() const
{
    return m_undoStack;
}

int FunctionFlowEditorController::baseDraftRevision() const
{
    return m_baseDraftRevision;
}

int FunctionFlowEditorController::observedRemoteRevision() const
{
    return m_observedRemoteRevision;
}

bool FunctionFlowEditorController::isDirty() const
{
    return m_undoStack && !m_undoStack->isClean();
}

bool FunctionFlowEditorController::editable() const
{
    return m_editable;
}

bool FunctionFlowEditorController::hasRemoteConflict() const
{
    return m_remoteConflict;
}

bool FunctionFlowEditorController::publicationBusy() const
{
    return m_publicationBusy;
}

QString FunctionFlowEditorController::unavailableCode() const
{
    return m_state.draft.unavailableCode;
}

OperationError FunctionFlowEditorController::lastError() const
{
    return m_lastError;
}

void FunctionFlowEditorController::setSaveDebounceMs(int milliseconds)
{
    m_saveTimer->setInterval(qMax(0, milliseconds));
}

void FunctionFlowEditorController::setEditorSaveDebounceMs(
    int milliseconds)
{
    m_editorSaveTimer->setInterval(qMax(0, milliseconds));
}

QString FunctionFlowEditorController::placeNode(
    FunctionFlowNodeType type,
    const QPointF &position)
{
    if (!m_editable
        || (uniqueNodeType(type) && countType(m_graph, type) > 0)) {
        return QString();
    }
    FunctionFlowGraph next = m_graph;
    const FunctionFlowNode node = defaultNode(type, position);
    next.nodes.append(node);
    if (!pushGraphChange(next, QString::fromUtf8("放置节点"))) {
        return QString();
    }
    return node.id;
}

bool FunctionFlowEditorController::addConnection(
    const FunctionFlowEndpoint &from,
    const FunctionFlowEndpoint &to)
{
    if (!m_editable) {
        return false;
    }
    const int fromIndex = nodeIndex(m_graph, from.nodeId.trimmed());
    const int toIndex = nodeIndex(m_graph, to.nodeId.trimmed());
    if (fromIndex < 0 || toIndex < 0) {
        return false;
    }
    const FunctionFlowNode &fromNode = m_graph.nodes.at(fromIndex);
    const FunctionFlowNode &toNode = m_graph.nodes.at(toIndex);
    if (!isFunctionFlowConnectionAllowed(
            fromNode.type,
            from.portId,
            toNode.type,
            to.portId)) {
        return false;
    }

    FunctionFlowPortSpec inputSpec;
    if (!portSpec(
            toNode.type,
            to.portId,
            FunctionFlowPortDirection::Input,
            &inputSpec)) {
        return false;
    }
    int maximumOrder = -1;
    for (const FunctionFlowEdge &edge : m_graph.edges) {
        if (edge.fromNodeId == from.nodeId
            && edge.fromPortId == from.portId
            && edge.toNodeId == to.nodeId
            && edge.toPortId == to.portId) {
            return false;
        }
        if (inputSpec.cardinality
                == FunctionFlowPortCardinality::One
            && edge.toNodeId == to.nodeId
            && edge.toPortId == to.portId) {
            return false;
        }
        if (edge.fromNodeId == from.nodeId
            && edge.fromPortId == from.portId) {
            maximumOrder = qMax(maximumOrder, edge.order);
        }
    }

    FunctionFlowEdge edge;
    edge.id = newFunctionFlowObjectId();
    edge.fromNodeId = from.nodeId.trimmed();
    edge.fromPortId = from.portId.trimmed();
    edge.toNodeId = to.nodeId.trimmed();
    edge.toPortId = to.portId.trimmed();
    edge.order = maximumOrder + 1;
    FunctionFlowGraph next = m_graph;
    next.edges.append(edge);
    return pushGraphChange(next, QString::fromUtf8("新增连线"));
}

bool FunctionFlowEditorController::removeNode(const QString &nodeId)
{
    if (!m_editable) {
        return false;
    }
    FunctionFlowGraph next = m_graph;
    const int index = nodeIndex(next, nodeId.trimmed());
    if (index < 0) {
        return false;
    }
    next.nodes.remove(index);
    for (int edge = next.edges.size() - 1; edge >= 0; --edge) {
        if (next.edges.at(edge).fromNodeId == nodeId
            || next.edges.at(edge).toNodeId == nodeId) {
            next.edges.remove(edge);
        }
    }
    return pushGraphChange(next, QString::fromUtf8("删除节点"));
}

bool FunctionFlowEditorController::removeEdge(const QString &edgeId)
{
    if (!m_editable) {
        return false;
    }
    FunctionFlowGraph next = m_graph;
    const int index = edgeIndex(next, edgeId.trimmed());
    if (index < 0) {
        return false;
    }
    next.edges.remove(index);
    return pushGraphChange(next, QString::fromUtf8("删除连线"));
}

bool FunctionFlowEditorController::commitNodePosition(
    const QString &nodeId,
    const QPointF &position)
{
    if (!m_editable) {
        return false;
    }
    FunctionFlowGraph next = m_graph;
    const int index = nodeIndex(next, nodeId.trimmed());
    if (index < 0 || next.nodes.at(index).position == position) {
        return false;
    }
    next.nodes[index].position = position;
    return pushGraphChange(next, QString::fromUtf8("移动节点"));
}

bool FunctionFlowEditorController::updateNode(
    const FunctionFlowNode &node)
{
    if (!m_editable) {
        return false;
    }
    FunctionFlowGraph next = m_graph;
    const int index = nodeIndex(next, node.id.trimmed());
    if (index < 0 || next.nodes.at(index).type != node.type) {
        return false;
    }
    FunctionFlowNode edited = next.nodes.at(index);
    edited.title = node.title;
    edited.enabled = node.enabled;
    edited.config = node.config;
    next.nodes[index] = edited;
    return pushGraphChange(
        next,
        QString::fromUtf8("修改节点设置")
    );
}

bool FunctionFlowEditorController::reorderOutputActions(
    const QString &outputNodeId,
    const QStringList &orderedEdgeIds)
{
    if (!m_editable) {
        return false;
    }
    const int outputIndex =
        nodeIndex(m_graph, outputNodeId.trimmed());
    if (outputIndex < 0
        || m_graph.nodes.at(outputIndex).type
            != FunctionFlowNodeType::Output) {
        return false;
    }

    QStringList currentIds;
    for (const FunctionFlowEdge &edge : m_graph.edges) {
        if (edge.fromNodeId == outputNodeId
            && edge.fromPortId == QStringLiteral("action_out")) {
            currentIds.append(edge.id);
        }
    }
    QStringList unique = orderedEdgeIds;
    unique.removeDuplicates();
    if (unique.size() != orderedEdgeIds.size()
        || unique.size() != currentIds.size()) {
        return false;
    }
    for (const QString &id : currentIds) {
        if (!unique.contains(id)) {
            return false;
        }
    }

    FunctionFlowGraph next = m_graph;
    for (int order = 0; order < orderedEdgeIds.size(); ++order) {
        const int index = edgeIndex(next, orderedEdgeIds.at(order));
        if (index < 0) {
            return false;
        }
        next.edges[index].order = order;
    }
    return pushGraphChange(
        next,
        QString::fromUtf8("调整结果动作顺序")
    );
}

bool FunctionFlowEditorController::flushPendingSave()
{
    m_saveTimer->stop();
    if (!isDirty()) {
        return true;
    }
    if (!m_editable || m_remoteConflict) {
        setLastError(errorWithCode(
            m_remoteConflict
                ? QStringLiteral("flow_draft_conflict")
                : QStringLiteral("flow_draft_unavailable"),
            m_remoteConflict
                ? QString::fromUtf8("草稿已在其它位置更新。")
                : QString::fromUtf8("当前草稿不可编辑。")
        ));
        return false;
    }
    if (!m_access.updateDraft) {
        setLastError(errorWithCode(
            QStringLiteral("flow_save_unavailable")
        ));
        return false;
    }

    const int expectedRevision = m_baseDraftRevision;
    const quint64 generation = m_graphGeneration;
    const FunctionFlowGraph graphToSave = m_graph;
    int savedRevision = 0;
    OperationError error;
    m_localSaveInProgress = true;
    m_hasDeferredState = false;
    const bool saved = m_access.updateDraft(
        m_functionId,
        expectedRevision,
        graphToSave,
        &savedRevision,
        &error
    );
    m_localSaveInProgress = false;

    if (saved) {
        m_discardOnDestruction = false;
        m_baseDraftRevision = savedRevision;
        m_observedRemoteRevision = qMax(
            m_observedRemoteRevision,
            savedRevision
        );
        m_state.draft.revision = savedRevision;
        m_state.draft.graph = graphToSave;
        m_state.draft.graphHash =
            functionFlowGraphHash(graphToSave);
        if (generation == m_graphGeneration) {
            m_undoStack->setClean();
        }
        setLastError();
    } else {
        setLastError(error.isEmpty()
            ? errorWithCode(QStringLiteral("flow_save_failed"))
            : error);
    }

    if (m_hasDeferredState) {
        const FunctionFlowState deferred = m_deferredState;
        m_hasDeferredState = false;
        applyRemoteState(deferred);
    } else if (!saved
               && m_lastError.code
                    == QStringLiteral("flow_draft_stale")) {
        refreshFromAccess();
    }
    if (saved && isDirty()) {
        scheduleDraftSave();
    }
    Q_EMIT stateChanged();
    return saved && !m_remoteConflict;
}

void FunctionFlowEditorController::updateEditorState(
    const FunctionFlowEditorState &editor)
{
    if (m_functionId.isEmpty()) {
        return;
    }
    const FunctionFlowEditorState normalized =
        normalizeFunctionFlowEditorState(editor);
    if (normalized.viewportCenter == m_editorState.viewportCenter
        && normalized.zoom == m_editorState.zoom
        && !m_editorSavePending) {
        return;
    }
    m_editorState = normalized;
    m_discardOnDestruction = false;
    m_editorSavePending = true;
    m_editorSaveTimer->start();
    Q_EMIT editorStateChanged();
    Q_EMIT stateChanged();
}

bool FunctionFlowEditorController::flushPendingEditorState()
{
    m_editorSaveTimer->stop();
    if (!m_editorSavePending) {
        return true;
    }
    if (!m_access.updateEditorState) {
        setLastError(errorWithCode(
            QStringLiteral("flow_editor_save_unavailable")
        ));
        return false;
    }
    OperationError error;
    if (!m_access.updateEditorState(
            m_functionId,
            m_editorState,
            &error)) {
        setLastError(error.isEmpty()
            ? errorWithCode(QStringLiteral("flow_editor_save_failed"))
            : error);
        return false;
    }
    m_state.editor = m_editorState;
    m_discardOnDestruction = false;
    m_editorSavePending = false;
    setLastError();
    Q_EMIT stateChanged();
    return true;
}

bool FunctionFlowEditorController::flushAllPendingSaves()
{
    if (!flushPendingSave()) {
        return false;
    }
    return flushPendingEditorState();
}

void FunctionFlowEditorController::discardPendingSaves()
{
    m_saveTimer->stop();
    m_editorSaveTimer->stop();
    m_editorSavePending = false;
    m_discardOnDestruction = true;
    Q_EMIT stateChanged();
}

FunctionFlowEditorPublishResult
FunctionFlowEditorController::publishFlow(
    bool replaceCorruptPublished)
{
    FunctionFlowEditorPublishResult result;
    if (m_publicationBusy) {
        result.outcome = FunctionFlowEditorPublishOutcome::Blocked;
        result.error = errorWithCode(
            QStringLiteral("flow_publish_busy")
        );
        return result;
    }
    if (!m_editable || m_remoteConflict) {
        result.outcome = FunctionFlowEditorPublishOutcome::Blocked;
        result.error = errorWithCode(
            m_remoteConflict
                ? QStringLiteral("flow_draft_conflict")
                : QStringLiteral("flow_draft_unavailable")
        );
        return result;
    }
    if (!flushPendingSave()) {
        result.outcome = FunctionFlowEditorPublishOutcome::Failed;
        result.error = m_lastError;
        return result;
    }
    if (!m_access.publish) {
        result.outcome = FunctionFlowEditorPublishOutcome::Failed;
        result.error = errorWithCode(
            QStringLiteral("flow_publish_unavailable")
        );
        setLastError(result.error);
        return result;
    }

    m_publicationBusy = true;
    Q_EMIT publicationBusyChanged(true);
    const FunctionFlowPublishResult published = m_access.publish(
        m_functionId,
        m_baseDraftRevision,
        replaceCorruptPublished
    );
    m_publicationBusy = false;
    Q_EMIT publicationBusyChanged(false);
    result.publishedRevision = published.publishedRevision;
    result.validation = published.validation;
    result.error = published.error;
    if (published.ok) {
        result.outcome = FunctionFlowEditorPublishOutcome::Succeeded;
        m_state.published.revision = published.publishedRevision;
        setLastError();
    } else if (published.error.code == QStringLiteral(
                   "flow_published_repair_confirmation_required")) {
        result.outcome =
            FunctionFlowEditorPublishOutcome::RepairConfirmationRequired;
        setLastError(published.error);
    } else {
        result.outcome = FunctionFlowEditorPublishOutcome::Failed;
        setLastError(published.error);
    }
    Q_EMIT stateChanged();
    return result;
}

void FunctionFlowEditorController::applyCommandGraph(
    const FunctionFlowGraph &graph)
{
    m_graph = normalizeFunctionFlowGraph(graph);
    Q_EMIT graphChanged();
}

bool FunctionFlowEditorController::pushGraphChange(
    const FunctionFlowGraph &graph,
    const QString &text)
{
    const FunctionFlowGraph normalized =
        normalizeFunctionFlowGraph(graph);
    if (graphEqual(m_graph, normalized)) {
        return false;
    }
    m_undoStack->push(new FunctionFlowGraphCommand(
        this,
        m_graph,
        normalized,
        text
    ));
    return true;
}

void FunctionFlowEditorController::handleUndoIndexChanged()
{
    m_discardOnDestruction = false;
    ++m_graphGeneration;
    analyzeWorkingGraph();
    scheduleDraftSave();
    Q_EMIT stateChanged();
}

void FunctionFlowEditorController::analyzeWorkingGraph()
{
    if (m_access.analyzeDraft && !m_functionId.isEmpty()) {
        m_analysis = m_access.analyzeDraft(
            m_functionId,
            m_graph
        );
    } else {
        m_analysis = FunctionFlowDraftAnalysis();
        m_analysis.graphHash = functionFlowGraphHash(m_graph);
    }
    Q_EMIT analysisChanged();
}

void FunctionFlowEditorController::scheduleDraftSave()
{
    if (!m_editable || m_remoteConflict || !isDirty()) {
        m_saveTimer->stop();
        return;
    }
    m_saveTimer->start();
}

void FunctionFlowEditorController::applyRemoteState(
    const FunctionFlowState &state)
{
    m_state.enabled = state.enabled;
    m_state.published = state.published;
    m_state.retainedValues = state.retainedValues;
    const int remoteRevision = state.draft.revision;
    m_observedRemoteRevision = qMax(
        m_observedRemoteRevision,
        remoteRevision
    );

    const bool sameRevisionDifferentGraph =
        remoteRevision == m_baseDraftRevision
        && !graphEqual(state.draft.graph, m_graph);
    if (remoteRevision > m_baseDraftRevision) {
        if (isDirty()) {
            m_remoteConflict = true;
            m_saveTimer->stop();
        } else {
            loadState(state);
            return;
        }
    } else if (sameRevisionDifferentGraph && !isDirty()) {
        loadState(state);
        return;
    } else if (!m_editorSavePending && !isDirty()) {
        const FunctionFlowEditorState editor =
            normalizeFunctionFlowEditorState(state.editor);
        if (editor.viewportCenter != m_editorState.viewportCenter
            || editor.zoom != m_editorState.zoom) {
            m_editorState = editor;
            Q_EMIT editorStateChanged();
        }
    }
    Q_EMIT stateChanged();
}

void FunctionFlowEditorController::loadState(
    const FunctionFlowState &state)
{
    m_saveTimer->stop();
    m_editorSaveTimer->stop();
    m_state = state;
    m_graph = normalizeFunctionFlowGraph(state.draft.graph);
    m_editorState =
        normalizeFunctionFlowEditorState(state.editor);
    m_baseDraftRevision = state.draft.revision;
    m_observedRemoteRevision = state.draft.revision;
    m_editable = state.draft.supported;
    m_remoteConflict = false;
    m_localSaveInProgress = false;
    m_hasDeferredState = false;
    m_editorSavePending = false;
    {
        const QSignalBlocker blocker(m_undoStack);
        m_undoStack->clear();
        m_undoStack->setClean();
    }
    ++m_graphGeneration;
    analyzeWorkingGraph();
    Q_EMIT graphChanged();
    Q_EMIT editorStateChanged();
    Q_EMIT stateChanged();
}

void FunctionFlowEditorController::clearForMissingFunction()
{
    m_functionId.clear();
    m_defaults = FunctionFlowPlacementDefaults();
    loadState(FunctionFlowState());
    m_editable = false;
}

FunctionFlowNode FunctionFlowEditorController::defaultNode(
    FunctionFlowNodeType type,
    const QPointF &position) const
{
    FunctionFlowNode node;
    node.id = newFunctionFlowObjectId();
    node.type = type;
    node.title = defaultNodeTitle(type);
    node.position = position;

    const FunctionSettings &function = m_defaults.function;
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        node.config.voice.speechProviderId =
            m_defaults.speechProviderId;
        node.config.voice.recording.triggerMode =
            function.recording.triggerMode;
        node.config.voice.recording.longRecordingEnabled =
            function.recording.longRecordingEnabled;
        node.config.voice.recording.segmentSeconds =
            function.recording.segmentSeconds;
        node.config.voice.recording.maximumMinutes =
            function.recording.maximumMinutes;
        node.config.voice.recording.countdownSeconds =
            function.recording.countdownSeconds;
        node.config.voice.recording.beepEnabled =
            function.recording.beepEnabled;
        node.config.voice.recording.beepPath =
            function.recording.beepPath;
        node.config.voice.acquisitionSequence =
            countType(m_graph, type);
        node.config.voice.networkPolicy =
            function.network.speech;
        break;
    case FunctionFlowNodeType::SelectionSource:
        node.config.selection.inheritStrongSelection = true;
        node.config.selection.acquisitionSequence =
            countType(m_graph, type);
        break;
    case FunctionFlowNodeType::ScreenshotSource:
        node.config.screenshot.ocrEngineId =
            m_defaults.ocrEngineId;
        node.config.screenshot.triggerMode =
            function.input.screenshotTriggerMode;
        node.config.screenshot.separateShortcut =
            function.input.screenshotShortcut;
        node.config.screenshot.acquisitionSequence =
            countType(m_graph, type);
        node.config.screenshot.networkPolicy =
            function.network.ocr;
        break;
    case FunctionFlowNodeType::Input:
        node.config.input.sequence = countType(m_graph, type);
        break;
    case FunctionFlowNodeType::Model:
        node.config.model.modelId = function.modelId;
        node.config.model.promptId = function.promptId;
        node.config.model.networkPolicy = function.network.model;
        break;
    case FunctionFlowNodeType::Output:
        break;
    case FunctionFlowNodeType::ResultPopup:
        node.config.popup.resultTemplate =
            function.output.resultTemplate;
        node.config.popup.resultActions =
            flowPopupActions(function.output.resultActions);
        node.config.popup.displaySeconds =
            function.output.resultPopupSeconds;
        node.config.popup.opacity =
            m_defaults.resultPopupOpacity;
        break;
    case FunctionFlowNodeType::ScreenshotPanel:
        node.config.screenshotPanel.displaySeconds =
            function.output.floatingBarSeconds;
        node.config.screenshotPanel.opacity =
            m_defaults.resultPopupOpacity;
        break;
    case FunctionFlowNodeType::AutoWrite:
        node.config.autoWrite.writeMode = QStringLiteral("insert");
        break;
    }
    return node;
}

void FunctionFlowEditorController::setLastError(
    const OperationError &error)
{
    m_lastError = error;
}
