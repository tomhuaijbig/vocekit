#include "../../src/controllers/function_flow_editor_controller.h"

#include <QUndoStack>
#include <QtTest>

namespace {

FunctionFlowNode nodeById(
    const FunctionFlowGraph &graph,
    const QString &id,
    bool *found = nullptr)
{
    for (const FunctionFlowNode &node : graph.nodes) {
        if (node.id == id) {
            if (found) {
                *found = true;
            }
            return node;
        }
    }
    if (found) {
        *found = false;
    }
    return FunctionFlowNode();
}

FunctionFlowEdge edgeById(
    const FunctionFlowGraph &graph,
    const QString &id,
    bool *found = nullptr)
{
    for (const FunctionFlowEdge &edge : graph.edges) {
        if (edge.id == id) {
            if (found) {
                *found = true;
            }
            return edge;
        }
    }
    if (found) {
        *found = false;
    }
    return FunctionFlowEdge();
}

struct FakeFlowSettings
{
    QString functionId = QStringLiteral("custom_1");
    FunctionFlowState state;
    int readCalls = 0;
    int analyzeCalls = 0;
    int updateDraftCalls = 0;
    int updateEditorCalls = 0;
    int publishCalls = 0;
    int setEnabledCalls = 0;
    int lastExpectedRevision = -1;
    int lastPublishedRevision = -1;
    bool lastReplaceCorrupt = false;
    bool failDraftSave = false;
    bool failEditorSave = false;
    bool failEnabled = false;
    bool emitDraftEventDuringSave = false;
    bool emitHigherDraftEventDuringSave = false;
    bool emitEditorEventDuringSave = false;
    FunctionFlowGraph lastSavedGraph;
    FunctionFlowEditorState lastSavedEditor;
    FunctionFlowPublishResult nextPublishResult;
    FunctionFlowEditorController *controller = nullptr;

    FakeFlowSettings()
    {
        state.draft.revision = 3;
        state.draft.graphHash =
            functionFlowGraphHash(state.draft.graph);
        state.published.graphHash =
            functionFlowGraphHash(state.published.graph);
        nextPublishResult.ok = true;
        nextPublishResult.publishedRevision = 7;
    }

    FunctionFlowSettingsAccess access()
    {
        FunctionFlowSettingsAccess access;
        access.readState = [this](
            const QString &id,
            FunctionFlowState *target,
            OperationError *error
        ) {
            ++readCalls;
            if (id != functionId || !target) {
                if (error) {
                    error->code = QStringLiteral("flow_function_not_found");
                }
                return false;
            }
            *target = state;
            return true;
        };
        access.analyzeDraft = [this](
            const QString &,
            const FunctionFlowGraph &graph
        ) {
            ++analyzeCalls;
            FunctionFlowDraftAnalysis analysis;
            analysis.graphHash = functionFlowGraphHash(graph);
            analysis.validation.ok = true;
            analysis.triggerAvailability.insert(
                FunctionFlowTrigger::MainHotkey,
                true
            );
            return analysis;
        };
        access.updateDraft = [this](
            const QString &,
            int expectedRevision,
            const FunctionFlowGraph &graph,
            int *savedRevision,
            OperationError *error
        ) {
            ++updateDraftCalls;
            lastExpectedRevision = expectedRevision;
            lastSavedGraph = graph;
            if (failDraftSave) {
                if (error) {
                    error->code = QStringLiteral("flow_save_failed");
                }
                return false;
            }
            state.draft.graph = graph;
            state.draft.revision = expectedRevision + 1;
            state.draft.graphHash = functionFlowGraphHash(graph);
            if (emitHigherDraftEventDuringSave) {
                state.draft.revision = expectedRevision + 2;
            }
            if (emitDraftEventDuringSave && controller) {
                controller->observeRemoteState(state);
            }
            if (savedRevision) {
                *savedRevision = expectedRevision + 1;
            }
            return true;
        };
        access.updateEditorState = [this](
            const QString &,
            const FunctionFlowEditorState &editor,
            OperationError *error
        ) {
            ++updateEditorCalls;
            lastSavedEditor = editor;
            if (failEditorSave) {
                if (error) {
                    error->code =
                        QStringLiteral("flow_editor_save_failed");
                }
                return false;
            }
            state.editor = editor;
            if (emitEditorEventDuringSave && controller) {
                controller->observeRemoteState(state);
            }
            return true;
        };
        access.publish = [this](
            const QString &,
            int expectedRevision,
            bool replaceCorrupt
        ) {
            ++publishCalls;
            lastPublishedRevision = expectedRevision;
            lastReplaceCorrupt = replaceCorrupt;
            return nextPublishResult;
        };
        access.setExecutionMode = [this](
            const QString &,
            FunctionExecutionMode mode,
            OperationError *error
        ) {
            ++setEnabledCalls;
            if (failEnabled) {
                if (error) {
                    error->code = QStringLiteral("flow_enable_failed");
                }
                return false;
            }
            state.enabled = mode == FunctionExecutionMode::Canvas;
            return true;
        };
        return access;
    }
};

FunctionFlowPlacementDefaults placementDefaults()
{
    FunctionFlowPlacementDefaults defaults;
    defaults.function.id = QStringLiteral("custom_1");
    defaults.function.modelId = QStringLiteral("deepseek-v4-flash");
    defaults.function.promptId = QStringLiteral("prompt_2");
    defaults.function.recording.triggerMode = QStringLiteral("hold");
    defaults.function.recording.countdownSeconds = 2;
    defaults.function.recording.beepEnabled = true;
    defaults.function.recording.beepPath =
        QStringLiteral("C:/sounds/beep.wav");
    defaults.function.input.screenshotTriggerMode =
        QStringLiteral("separateAndLauncher");
    defaults.function.input.screenshotShortcut =
        QStringLiteral("Ctrl+Alt+8");
    defaults.function.output.resultTemplate =
        QStringLiteral("detail");
    defaults.function.output.resultActions =
        QStringList() << QStringLiteral("copy")
                      << QStringLiteral("write");
    defaults.function.output.resultPopupSeconds = 9;
    defaults.function.output.floatingBarSeconds = 4;
    defaults.function.network.speech = QStringLiteral("direct");
    defaults.function.network.ocr = QStringLiteral("systemProxy");
    defaults.function.network.model = QStringLiteral("inherit");
    defaults.speechProviderId = QStringLiteral("xfyun");
    defaults.ocrEngineId = QStringLiteral("rapid");
    defaults.resultPopupOpacity = 88;
    return defaults;
}

} // namespace

class FunctionFlowEditorControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void editsGraphWithOneUndoCommandPerIntent();
    void placingPopupUsesFlowActionsInsteadOfUnsupportedClassicActions();
    void rejectsInvalidDuplicateAndFullSingleInputConnections();
    void allowsMultipleSourcesToConnectToOneInput();
    void preservesRetainedValuesAcrossEditsAndUndo();
    void reordersOutputActionsThroughEdgeOrder();
    void debouncesDraftSavesAndTracksTheCleanGeneration();
    void keepsDirtyStateAfterSaveFailureAndRetries();
    void ignoresItsOwnSynchronousDraftEvent();
    void ignoresEditorStateEchoWhileTheDraftIsDirty();
    void detectsHigherRemoteRevisionWithoutOverwritingLocalGraph();
    void reloadsRemoteDraftWhenClean();
    void flushesBeforeSwitchingFunctions();
    void flushesBeforePublishingAndUsesTheSavedRevision();
    void publishingDoesNotEnableTheLegacyFlowState();
    void requiresExplicitRepairRetryAndBlocksDuplicatePublication();
    void savesViewportIndependentlyFromTheDraft();
    void analyzesEveryGraphCommandWithoutAddingUndoEntries();
    void exposesUnsupportedDraftAsReadOnly();
};

void FunctionFlowEditorControllerTests::
editsGraphWithOneUndoCommandPerIntent()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));

    const QString voiceId = controller.placeNode(
        FunctionFlowNodeType::VoiceSource,
        QPointF(20.0, 30.0)
    );
    const QString inputId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF(260.0, 30.0)
    );
    QVERIFY(!voiceId.isEmpty());
    QVERIFY(!inputId.isEmpty());
    QCOMPARE(controller.undoStack()->count(), 2);

    FunctionFlowEndpoint from;
    from.nodeId = voiceId;
    from.portId = QStringLiteral("text_out");
    FunctionFlowEndpoint to;
    to.nodeId = inputId;
    to.portId = QStringLiteral("text_in");
    QVERIFY(controller.addConnection(from, to));
    QCOMPARE(controller.undoStack()->count(), 3);
    QCOMPARE(controller.graph().edges.size(), 1);

    QVERIFY(controller.commitNodePosition(
        inputId,
        QPointF(410.0, 115.0)
    ));
    QCOMPARE(controller.undoStack()->count(), 4);
    QCOMPARE(nodeById(controller.graph(), inputId).position,
             QPointF(410.0, 115.0));

    FunctionFlowNode input = nodeById(controller.graph(), inputId);
    input.title = QString::fromUtf8("主要输入");
    input.enabled = false;
    input.config.input.role = QStringLiteral("system");
    input.config.input.sequence = 4;
    input.config.input.required = false;
    QVERIFY(controller.updateNode(input));
    QCOMPARE(controller.undoStack()->count(), 5);
    QCOMPARE(nodeById(controller.graph(), inputId).title,
             QString::fromUtf8("主要输入"));

    QVERIFY(controller.removeNode(voiceId));
    QCOMPARE(controller.graph().nodes.size(), 1);
    QVERIFY(controller.graph().edges.isEmpty());
    QCOMPARE(controller.undoStack()->count(), 6);

    controller.undoStack()->undo();
    QCOMPARE(controller.graph().nodes.size(), 2);
    QCOMPARE(controller.graph().edges.size(), 1);
    controller.undoStack()->undo();
    QVERIFY(nodeById(controller.graph(), inputId).enabled);
    controller.undoStack()->redo();
    QVERIFY(!nodeById(controller.graph(), inputId).enabled);
}

void FunctionFlowEditorControllerTests::
placingPopupUsesFlowActionsInsteadOfUnsupportedClassicActions()
{
    FakeFlowSettings fake;
    FunctionFlowPlacementDefaults defaults = placementDefaults();
    defaults.function.output.resultActions = QStringList()
        << QStringLiteral("regenerate")
        << QStringLiteral("retryModel")
        << QStringLiteral("followUp")
        << QStringLiteral("expand")
        << QStringLiteral("vocabulary")
        << QStringLiteral("copy")
        << QStringLiteral("write")
        << QStringLiteral("replace");
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(fake.functionId, defaults));

    const QString popupId = controller.placeNode(
        FunctionFlowNodeType::ResultPopup,
        QPointF()
    );
    QVERIFY(!popupId.isEmpty());
    const FunctionFlowNode popup = nodeById(
        controller.graph(),
        popupId
    );
    QCOMPARE(
        popup.config.popup.resultActions,
        defaultFunctionFlowPopupActionIds()
    );
    QVERIFY(!popup.config.popup.resultActions.contains(
        QStringLiteral("regenerate")
    ));
    QVERIFY(!popup.config.popup.resultActions.contains(
        QStringLiteral("retryModel")
    ));
    QVERIFY(!popup.config.popup.resultActions.contains(
        QStringLiteral("followUp")
    ));
}

void FunctionFlowEditorControllerTests::
rejectsInvalidDuplicateAndFullSingleInputConnections()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const QString voiceId = controller.placeNode(
        FunctionFlowNodeType::VoiceSource,
        QPointF()
    );
    const QString secondInputId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    const QString inputId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    const QString popupId = controller.placeNode(
        FunctionFlowNodeType::ResultPopup,
        QPointF()
    );
    const QString outputId = controller.placeNode(
        FunctionFlowNodeType::Output,
        QPointF()
    );

    FunctionFlowEndpoint voiceOut{voiceId, QStringLiteral("text_out")};
    FunctionFlowEndpoint inputOut{
        inputId,
        QStringLiteral("text_out")
    };
    FunctionFlowEndpoint secondInputOut{
        secondInputId,
        QStringLiteral("text_out")
    };
    FunctionFlowEndpoint inputIn{inputId, QStringLiteral("text_in")};
    FunctionFlowEndpoint outputIn{outputId, QStringLiteral("text_in")};
    FunctionFlowEndpoint popupIn{
        popupId,
        QStringLiteral("action_in")
    };
    const int before = controller.undoStack()->count();
    QVERIFY(!controller.addConnection(voiceOut, popupIn));
    QCOMPARE(controller.undoStack()->count(), before);
    QVERIFY(controller.addConnection(voiceOut, inputIn));
    const int afterValid = controller.undoStack()->count();
    QVERIFY(!controller.addConnection(voiceOut, inputIn));
    QCOMPARE(controller.undoStack()->count(), afterValid);
    QVERIFY(controller.addConnection(inputOut, outputIn));
    const int afterSingleInput =
        controller.undoStack()->count();
    QVERIFY(!controller.addConnection(secondInputOut, outputIn));
    QCOMPARE(
        controller.undoStack()->count(),
        afterSingleInput
    );
}

void FunctionFlowEditorControllerTests::
allowsMultipleSourcesToConnectToOneInput()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const QString voiceId = controller.placeNode(
        FunctionFlowNodeType::VoiceSource,
        QPointF()
    );
    const QString selectionId = controller.placeNode(
        FunctionFlowNodeType::SelectionSource,
        QPointF()
    );
    const QString inputId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );

    const FunctionFlowEndpoint inputIn{
        inputId,
        QStringLiteral("text_in")
    };
    QVERIFY(controller.addConnection(
        FunctionFlowEndpoint{
            voiceId,
            QStringLiteral("text_out")
        },
        inputIn
    ));
    QVERIFY(controller.addConnection(
        FunctionFlowEndpoint{
            selectionId,
            QStringLiteral("text_out")
        },
        inputIn
    ));
    QCOMPARE(controller.graph().edges.size(), 2);
}

void FunctionFlowEditorControllerTests::
preservesRetainedValuesAcrossEditsAndUndo()
{
    FakeFlowSettings fake;
    FunctionFlowNode voice;
    voice.id = QStringLiteral("voice_1");
    voice.type = FunctionFlowNodeType::VoiceSource;
    voice.retainedValues.insert(QStringLiteral("futureNode"), 17);
    FunctionFlowNode input;
    input.id = QStringLiteral("input_1");
    input.type = FunctionFlowNodeType::Input;
    FunctionFlowEdge edge;
    edge.id = QStringLiteral("edge_1");
    edge.fromNodeId = voice.id;
    edge.fromPortId = QStringLiteral("text_out");
    edge.toNodeId = input.id;
    edge.toPortId = QStringLiteral("text_in");
    edge.retainedValues.insert(QStringLiteral("futureEdge"), true);
    fake.state.draft.graph.nodes << voice << input;
    fake.state.draft.graph.edges << edge;
    fake.state.draft.graph.retainedValues.insert(
        QStringLiteral("futureGraph"),
        QStringLiteral("kept")
    );
    fake.state.draft.graphHash =
        functionFlowGraphHash(fake.state.draft.graph);

    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    FunctionFlowNode edited = nodeById(
        controller.graph(),
        QStringLiteral("voice_1")
    );
    edited.title = QString::fromUtf8("语音 A");
    edited.config.voice.acquisitionSequence = 8;
    QVERIFY(controller.updateNode(edited));

    QCOMPARE(
        nodeById(controller.graph(), edited.id)
            .retainedValues.value(QStringLiteral("futureNode")).toInt(),
        17
    );
    QCOMPARE(
        edgeById(controller.graph(), edge.id)
            .retainedValues.value(QStringLiteral("futureEdge")).toBool(),
        true
    );
    QCOMPARE(
        controller.graph().retainedValues
            .value(QStringLiteral("futureGraph")).toString(),
        QStringLiteral("kept")
    );

    QVERIFY(controller.removeNode(edited.id));
    controller.undoStack()->undo();
    QCOMPARE(
        nodeById(controller.graph(), edited.id)
            .retainedValues.value(QStringLiteral("futureNode")).toInt(),
        17
    );
    QCOMPARE(controller.graph().edges.size(), 1);
}

void FunctionFlowEditorControllerTests::
reordersOutputActionsThroughEdgeOrder()
{
    FakeFlowSettings fake;
    FunctionFlowNode output;
    output.id = QStringLiteral("output");
    output.type = FunctionFlowNodeType::Output;
    FunctionFlowNode popup;
    popup.id = QStringLiteral("popup");
    popup.type = FunctionFlowNodeType::ResultPopup;
    FunctionFlowNode writer;
    writer.id = QStringLiteral("writer");
    writer.type = FunctionFlowNodeType::AutoWrite;
    fake.state.draft.graph.nodes << output << popup << writer;
    for (int index = 0; index < 2; ++index) {
        FunctionFlowEdge edge;
        edge.id = QStringLiteral("edge_%1").arg(index);
        edge.fromNodeId = output.id;
        edge.fromPortId = QStringLiteral("action_out");
        edge.toNodeId = index == 0 ? popup.id : writer.id;
        edge.toPortId = QStringLiteral("action_in");
        edge.order = index;
        fake.state.draft.graph.edges.append(edge);
    }
    fake.state.draft.graphHash =
        functionFlowGraphHash(fake.state.draft.graph);

    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    QVERIFY(controller.reorderOutputActions(
        output.id,
        QStringList() << QStringLiteral("edge_1")
                      << QStringLiteral("edge_0")
    ));
    QCOMPARE(
        edgeById(controller.graph(), QStringLiteral("edge_1")).order,
        0
    );
    QCOMPARE(
        edgeById(controller.graph(), QStringLiteral("edge_0")).order,
        1
    );
    controller.undoStack()->undo();
    QCOMPARE(
        edgeById(controller.graph(), QStringLiteral("edge_0")).order,
        0
    );
}

void FunctionFlowEditorControllerTests::
debouncesDraftSavesAndTracksTheCleanGeneration()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    fake.controller = &controller;
    controller.setSaveDebounceMs(20);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const QString nodeId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    QVERIFY(controller.commitNodePosition(nodeId, QPointF(20.0, 20.0)));
    QVERIFY(controller.commitNodePosition(nodeId, QPointF(40.0, 20.0)));
    QTRY_COMPARE(fake.updateDraftCalls, 1);
    QCOMPARE(fake.lastExpectedRevision, 3);
    QCOMPARE(controller.baseDraftRevision(), 4);
    QVERIFY(!controller.isDirty());

    controller.undoStack()->undo();
    QVERIFY(controller.isDirty());
    controller.undoStack()->redo();
    QVERIFY(!controller.isDirty());
    QTest::qWait(35);
    QCOMPARE(fake.updateDraftCalls, 1);
}

void FunctionFlowEditorControllerTests::
keepsDirtyStateAfterSaveFailureAndRetries()
{
    FakeFlowSettings fake;
    fake.failDraftSave = true;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(10);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    controller.placeNode(FunctionFlowNodeType::Input, QPointF());
    QTRY_COMPARE(fake.updateDraftCalls, 1);
    QVERIFY(controller.isDirty());
    QCOMPARE(controller.lastError().code,
             QStringLiteral("flow_save_failed"));

    fake.failDraftSave = false;
    QVERIFY(controller.flushPendingSave());
    QCOMPARE(fake.updateDraftCalls, 2);
    QVERIFY(!controller.isDirty());
}

void FunctionFlowEditorControllerTests::
ignoresItsOwnSynchronousDraftEvent()
{
    FakeFlowSettings fake;
    fake.emitDraftEventDuringSave = true;
    FunctionFlowEditorController controller(fake.access());
    fake.controller = &controller;
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    controller.placeNode(FunctionFlowNodeType::Input, QPointF());
    QVERIFY(controller.flushPendingSave());
    QVERIFY(!controller.hasRemoteConflict());
    QVERIFY(!controller.isDirty());
    QCOMPARE(controller.baseDraftRevision(), 4);

    fake.emitHigherDraftEventDuringSave = true;
    controller.placeNode(FunctionFlowNodeType::Model, QPointF());
    QVERIFY(controller.flushPendingSave());
    QVERIFY(!controller.hasRemoteConflict());
    QCOMPARE(controller.baseDraftRevision(), 6);
    QCOMPARE(controller.observedRemoteRevision(), 6);
}

void FunctionFlowEditorControllerTests::
ignoresEditorStateEchoWhileTheDraftIsDirty()
{
    FakeFlowSettings fake;
    fake.emitEditorEventDuringSave = true;
    FunctionFlowEditorController controller(fake.access());
    fake.controller = &controller;
    controller.setSaveDebounceMs(60000);
    controller.setEditorSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const QString localId = controller.placeNode(
        FunctionFlowNodeType::Model,
        QPointF(320.0, 240.0)
    );
    QVERIFY(!localId.isEmpty());

    FunctionFlowEditorState editor;
    editor.viewportCenter = QPointF(900.0, 700.0);
    editor.zoom = 1.75;
    controller.updateEditorState(editor);
    QVERIFY(controller.flushPendingEditorState());

    QVERIFY(!controller.hasRemoteConflict());
    QVERIFY(controller.isDirty());
    bool found = false;
    nodeById(controller.graph(), localId, &found);
    QVERIFY(found);
    QVERIFY(controller.flushPendingSave());
    QVERIFY(!controller.isDirty());
}

void FunctionFlowEditorControllerTests::
detectsHigherRemoteRevisionWithoutOverwritingLocalGraph()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const QString localId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );

    FunctionFlowState remote = fake.state;
    remote.draft.revision = 8;
    remote.draft.graph.nodes.clear();
    remote.draft.graphHash =
        functionFlowGraphHash(remote.draft.graph);
    controller.observeRemoteState(remote);

    QVERIFY(controller.hasRemoteConflict());
    QCOMPARE(controller.baseDraftRevision(), 3);
    QCOMPARE(controller.observedRemoteRevision(), 8);
    bool found = false;
    nodeById(controller.graph(), localId, &found);
    QVERIFY(found);
    QVERIFY(!controller.flushPendingSave());
    QCOMPARE(fake.updateDraftCalls, 0);
}

void FunctionFlowEditorControllerTests::reloadsRemoteDraftWhenClean()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    FunctionFlowNode remoteNode;
    remoteNode.id = QStringLiteral("remote");
    remoteNode.type = FunctionFlowNodeType::Input;
    FunctionFlowState remote = fake.state;
    remote.draft.revision = 5;
    remote.draft.graph.nodes.append(remoteNode);
    remote.draft.graphHash =
        functionFlowGraphHash(remote.draft.graph);

    controller.observeRemoteState(remote);
    QCOMPARE(controller.baseDraftRevision(), 5);
    QCOMPARE(controller.observedRemoteRevision(), 5);
    QCOMPARE(controller.graph().nodes.size(), 1);
    QVERIFY(!controller.isDirty());
    QCOMPARE(controller.undoStack()->count(), 0);
}

void FunctionFlowEditorControllerTests::flushesBeforeSwitchingFunctions()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    controller.placeNode(FunctionFlowNodeType::Input, QPointF());
    fake.failDraftSave = true;
    QVERIFY(!controller.openFunction(
        QStringLiteral("custom_2"),
        placementDefaults()
    ));
    QCOMPARE(controller.functionId(), fake.functionId);
    QVERIFY(controller.isDirty());
}

void FunctionFlowEditorControllerTests::
flushesBeforePublishingAndUsesTheSavedRevision()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    controller.placeNode(FunctionFlowNodeType::Input, QPointF());

    const FunctionFlowEditorPublishResult result =
        controller.publishFlow(false);
    QCOMPARE(
        result.outcome,
        FunctionFlowEditorPublishOutcome::Succeeded
    );
    QCOMPARE(fake.updateDraftCalls, 1);
    QCOMPARE(fake.publishCalls, 1);
    QCOMPARE(fake.lastPublishedRevision, 4);
}

void FunctionFlowEditorControllerTests::
publishingDoesNotEnableTheLegacyFlowState()
{
    FakeFlowSettings fake;
    fake.state.enabled = false;
    FunctionFlowEditorController controller(fake.access());
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));

    const FunctionFlowEditorPublishResult result =
        controller.publishFlow(false);

    QCOMPARE(
        result.outcome,
        FunctionFlowEditorPublishOutcome::Succeeded
    );
    QVERIFY(!fake.state.enabled);
    QVERIFY(!controller.flowState().enabled);
    QCOMPARE(fake.setEnabledCalls, 0);
}

void FunctionFlowEditorControllerTests::
requiresExplicitRepairRetryAndBlocksDuplicatePublication()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    fake.nextPublishResult.ok = false;
    fake.nextPublishResult.error.code = QStringLiteral(
        "flow_published_repair_confirmation_required"
    );
    FunctionFlowEditorPublishResult first =
        controller.publishFlow(false);
    QCOMPARE(
        first.outcome,
        FunctionFlowEditorPublishOutcome::RepairConfirmationRequired
    );
    QVERIFY(!fake.lastReplaceCorrupt);

    fake.nextPublishResult.ok = true;
    FunctionFlowEditorPublishResult repaired =
        controller.publishFlow(true);
    QCOMPARE(
        repaired.outcome,
        FunctionFlowEditorPublishOutcome::Succeeded
    );
    QVERIFY(fake.lastReplaceCorrupt);
    QCOMPARE(fake.lastPublishedRevision, 3);

    bool nestedBlocked = false;
    FunctionFlowEditorController *reentrantController = nullptr;
    fake.nextPublishResult.ok = true;
    FunctionFlowSettingsAccess access = fake.access();
    access.publish = [&](
        const QString &,
        int,
        bool
    ) {
        const FunctionFlowEditorPublishResult nested =
            reentrantController->publishFlow(false);
        nestedBlocked =
            nested.outcome
            == FunctionFlowEditorPublishOutcome::Blocked;
        FunctionFlowPublishResult result;
        result.ok = true;
        return result;
    };
    FunctionFlowEditorController reentrant(access);
    reentrantController = &reentrant;
    QVERIFY(reentrant.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const FunctionFlowEditorPublishResult outer =
        reentrant.publishFlow(false);
    QCOMPARE(
        outer.outcome,
        FunctionFlowEditorPublishOutcome::Succeeded
    );
    // The controller's own busy state is also exposed for the UI.
    QVERIFY(!controller.publicationBusy());
    QVERIFY(nestedBlocked);
}

void FunctionFlowEditorControllerTests::
savesViewportIndependentlyFromTheDraft()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setEditorSaveDebounceMs(10);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    FunctionFlowEditorState editor;
    editor.viewportCenter = QPointF(300.0, 220.0);
    editor.zoom = 1.6;
    controller.updateEditorState(editor);
    QTRY_COMPARE(fake.updateEditorCalls, 1);
    QCOMPARE(fake.updateDraftCalls, 0);
    QCOMPARE(controller.undoStack()->count(), 0);
    QCOMPARE(fake.state.draft.revision, 3);
    QCOMPARE(fake.lastSavedEditor.viewportCenter, editor.viewportCenter);
    QCOMPARE(fake.lastSavedEditor.zoom, editor.zoom);
}

void FunctionFlowEditorControllerTests::
analyzesEveryGraphCommandWithoutAddingUndoEntries()
{
    FakeFlowSettings fake;
    FunctionFlowEditorController controller(fake.access());
    controller.setSaveDebounceMs(60000);
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    const int initialAnalyses = fake.analyzeCalls;
    const QString nodeId = controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    QCOMPARE(fake.analyzeCalls, initialAnalyses + 1);
    QCOMPARE(controller.undoStack()->count(), 1);
    controller.commitNodePosition(nodeId, QPointF(10.0, 20.0));
    QCOMPARE(fake.analyzeCalls, initialAnalyses + 2);
    QCOMPARE(controller.undoStack()->count(), 2);
}

void FunctionFlowEditorControllerTests::
exposesUnsupportedDraftAsReadOnly()
{
    FakeFlowSettings fake;
    fake.state.draft.supported = false;
    fake.state.draft.unavailableCode =
        QStringLiteral("flow_schema_newer");
    FunctionFlowEditorController controller(fake.access());
    QVERIFY(controller.openFunction(
        fake.functionId,
        placementDefaults()
    ));
    QVERIFY(!controller.editable());
    QCOMPARE(
        controller.unavailableCode(),
        QStringLiteral("flow_schema_newer")
    );
    QVERIFY(controller.placeNode(
        FunctionFlowNodeType::Input,
        QPointF()
    ).isEmpty());
    QCOMPARE(fake.updateDraftCalls, 0);
    QCOMPARE(fake.publishCalls, 0);
}

QTEST_MAIN(FunctionFlowEditorControllerTests)

#include "function_flow_editor_controller_tests.moc"
