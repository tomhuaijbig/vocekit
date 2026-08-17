#include "../../src/ui/function_canvas_editor.h"

#include "../../src/ui/function_canvas_inspector.h"
#include "../../src/ui/function_canvas_node_item.h"
#include "../../src/ui/function_canvas_palette.h"
#include "../../src/ui/function_canvas_scene.h"
#include "../../src/ui/function_canvas_view.h"
#include "../../src/ui/function_canvas_visual_style.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsProxyWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTextItem>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>

QString normalizeModelId(
    const QString &value,
    const QString &fallback)
{
    const QString trimmed = value.trimmed();
    if (trimmed == QStringLiteral("claude:claude-sonnet-4-6")) {
        return QStringLiteral("claude:claude-sonnet-5");
    }
    return trimmed.isEmpty() ? fallback : trimmed;
}

namespace {

struct FakeEditorSettings
{
    FunctionFlowState state;
    FunctionExecutionMode executionMode =
        FunctionExecutionMode::Classic;
    int draftSaves = 0;
    int editorSaves = 0;
    int publishCalls = 0;
    bool requireRepair = false;
    bool lastRepair = false;

    FakeEditorSettings()
    {
        state.draft.graphHash =
            functionFlowGraphHash(state.draft.graph);
        state.published.graphHash =
            functionFlowGraphHash(state.published.graph);
    }

    FunctionFlowSettingsAccess flowAccess()
    {
        FunctionFlowSettingsAccess access;
        access.readState = [this](
            const QString &,
            FunctionFlowState *target,
            OperationError *
        ) {
            *target = state;
            return true;
        };
        access.analyzeDraft = [](
            const QString &,
            const FunctionFlowGraph &graph
        ) {
            FunctionFlowDraftAnalysis analysis;
            analysis.graphHash = functionFlowGraphHash(graph);
            analysis.validation.ok = !graph.nodes.isEmpty();
            analysis.triggerAvailability.insert(
                FunctionFlowTrigger::MainHotkey,
                !graph.nodes.isEmpty()
            );
            return analysis;
        };
        access.updateDraft = [this](
            const QString &,
            int expected,
            const FunctionFlowGraph &graph,
            int *savedRevision,
            OperationError *
        ) {
            ++draftSaves;
            state.draft.graph = graph;
            state.draft.revision = expected + 1;
            state.draft.graphHash = functionFlowGraphHash(graph);
            *savedRevision = state.draft.revision;
            return true;
        };
        access.updateEditorState = [this](
            const QString &,
            const FunctionFlowEditorState &editor,
            OperationError *
        ) {
            ++editorSaves;
            state.editor = editor;
            return true;
        };
        access.publish = [this](
            const QString &,
            int,
            bool repair
        ) {
            ++publishCalls;
            lastRepair = repair;
            FunctionFlowPublishResult result;
            if (requireRepair && !repair) {
                result.error.code = QStringLiteral(
                    "flow_published_repair_confirmation_required"
                );
                return result;
            }
            result.ok = true;
            result.publishedRevision = 2;
            return result;
        };
        access.setExecutionMode = [this](
            const QString &,
            FunctionExecutionMode mode,
            OperationError *
        ) {
            state.enabled = mode == FunctionExecutionMode::Canvas;
            return true;
        };
        return access;
    }
};

FunctionCanvasInspectorOptions inspectorOptions()
{
    FunctionCanvasInspectorOptions options;
    options.models.append(qMakePair(
        QStringLiteral("model_1"),
        QString::fromUtf8("模型一")
    ));
    options.prompts.append(qMakePair(
        QStringLiteral("prompt_1"),
        QString::fromUtf8("提示词一")
    ));
    options.speechProviders.append(qMakePair(
        QStringLiteral("baidu"),
        QString::fromUtf8("百度")
    ));
    options.ocrEngines.append(qMakePair(
        QStringLiteral("automatic"),
        QString::fromUtf8("自动")
    ));
    return options;
}

FunctionFlowPlacementDefaults defaults()
{
    FunctionFlowPlacementDefaults defaults;
    defaults.function.id = QStringLiteral("custom_1");
    defaults.function.modelId = QStringLiteral("model_1");
    defaults.function.promptId = QStringLiteral("prompt_1");
    defaults.speechProviderId = QStringLiteral("baidu");
    defaults.ocrEngineId = QStringLiteral("automatic");
    defaults.resultPopupOpacity = 90;
    return defaults;
}

class TextRecordingPaintEngine : public QPaintEngine
{
public:
    TextRecordingPaintEngine()
        : QPaintEngine(QPaintEngine::AllFeatures)
    {
    }

    bool begin(QPaintDevice *) override
    {
        setActive(true);
        return true;
    }

    bool end() override
    {
        setActive(false);
        return true;
    }

    void updateState(const QPaintEngineState &) override
    {
    }

    void drawPixmap(
        const QRectF &,
        const QPixmap &,
        const QRectF &) override
    {
    }

    void drawPath(const QPainterPath &) override
    {
    }

    void drawRects(const QRectF *, int) override
    {
    }

    void drawLines(const QLineF *, int) override
    {
    }

    void drawEllipse(const QRectF &) override
    {
    }

    void drawPolygon(
        const QPointF *,
        int,
        PolygonDrawMode) override
    {
    }

    void drawTextItem(
        const QPointF &,
        const QTextItem &item) override
    {
        texts.append(item.text());
    }

    Type type() const override
    {
        return QPaintEngine::User;
    }

    QStringList texts;
};

class TextRecordingPaintDevice : public QPaintDevice
{
public:
    QPaintEngine *paintEngine() const override
    {
        return &engine;
    }

    mutable TextRecordingPaintEngine engine;

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return 400;
        case PdmHeight:
            return 300;
        case PdmWidthMM:
            return 106;
        case PdmHeightMM:
            return 79;
        case PdmNumColors:
            return 16777216;
        case PdmDepth:
            return 32;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return 96;
        case PdmDevicePixelRatio:
            return 1;
        case PdmDevicePixelRatioScaled:
            return 65536;
        }
        return QPaintDevice::metric(metric);
    }
};

QStringList paintedNodeTexts(FunctionFlowNodeType type)
{
    FunctionFlowNode node;
    node.id = QStringLiteral("node");
    node.type = type;
    FunctionCanvasNodeItem item(node);
    TextRecordingPaintDevice device;
    QPainter painter(&device);
    item.paint(&painter, nullptr, nullptr);
    painter.end();
    return device.engine.texts;
}

QString visibleText(QWidget *widget)
{
    QStringList parts;
    for (QLabel *label : widget->findChildren<QLabel *>()) {
        parts.append(label->text());
    }
    for (QCheckBox *box : widget->findChildren<QCheckBox *>()) {
        parts.append(box->text());
    }
    for (QLineEdit *edit : widget->findChildren<QLineEdit *>()) {
        parts.append(edit->text());
    }
    for (QComboBox *box : widget->findChildren<QComboBox *>()) {
        parts.append(box->currentText());
    }
    return parts.join(QStringLiteral("\n"));
}

struct EditorInvariantSnapshot
{
    QString graphHash;
    qreal zoom = 1.0;
    QPointF viewportCenter;
    int undoCount = 0;
};

EditorInvariantSnapshot editorInvariantSnapshot(
    const FunctionCanvasEditor &editor)
{
    EditorInvariantSnapshot snapshot;
    snapshot.graphHash = functionFlowGraphHash(
        editor.controller()->graph()
    );
    snapshot.zoom = editor.canvasView()->zoomLevel();
    snapshot.viewportCenter =
        editor.canvasView()->viewportCenter();
    snapshot.undoCount =
        editor.controller()->undoStack()->count();
    return snapshot;
}

bool requestNodeSettings(FunctionCanvasNodeItem *item)
{
    return item
        && QMetaObject::invokeMethod(
            item,
            "settingsRequested",
            Qt::DirectConnection,
            Q_ARG(QString, item->nodeId())
        );
}

} // namespace

class FunctionCanvasEditorTests : public QObject
{
    Q_OBJECT

private slots:
    void paletteListsExactlyTheNineSupportedNodeTypes();
    void paletteUsesThreeSearchAwareCategories();
    void paletteEntriesUseChineseGlyphsAndStableTooltips();
    void repeatedPaletteDragsDoNotRetainDragObjects();
    void paletteNodeCanBeDroppedAtTheRequestedCanvasPosition();
    void editorShellWidgetsStayOutsideTheGraphicsScene();
    void footerUsesOneRowAndClearButtonRoles();
    void footerDoesNotShowTriggerAvailabilityLine();
    void compactEditorKeepsToolbarAndFooterControlsInside();
    void denseFooterKeepsLongCountsAndRevisionsInside();
    void compactEditorKeepsOnlyTheLastOpenedSidePanel();
    void wideEditorAllowsBothSidePanels();
    void resizingAcrossSidePanelThresholdPreservesEditorState();
    void zoomLabelTracksRestoredAndToolbarZoom();
    void nodeCardsUseChinesePortLabels();
    void inspectorUsesStableSectionsForEveryNodeType();
    void inspectorScalesChineseTextAndUsesTheAppFont();
    void inspectorShowsOnlyTheSpecifiedTypedSettings();
    void inspectorDisplaysMigratedModelWithoutChangingTheDraft();
    void modelSamplingControlsEditTypedNodeWithoutInitialWrite();
    void inputRoleShowsChineseButEmitsTheStableRoleId();
    void popupActionsShowChineseButKeepTheStableActionIds();
    void inspectorEmitsAWholeTypedNodeWithoutDroppingRetainedValues();
    void inspectorOpacityControlsCannotProduceInvalidValues();
    void editorWiresSceneIntentsIntoTheUndoableWorkingGraph();
    void manyInputKeepsVoiceAndSelectionConnectionsInFinalGraph();
    void nodeInspectorRequiresDoubleClick();
    void inspectorTextEditingOwnsDeleteAndBackspace();
    void hidingInspectorPreservesGraphAndEditorState();
    void nodeClickAndDragDoesNotPanTheCanvas();
    void viewportChangesStayOutsideTheGraphUndoStack();
    void unsupportedDraftIsReadOnlyAndNeverOverwritesRawData();
    void publishRepairRequiresExplicitConfirmation();
    void classicPublishShowsActivationInformationWithoutChangingMode();
    void canvasPublishDoesNotShowClassicActivationInformation();
    void publishFailureUsesThePublishFlowTitle();
};

void FunctionCanvasEditorTests::
modelSamplingControlsEditTypedNodeWithoutInitialWrite()
{
    FunctionCanvasInspector inspector;
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("model_sampling");
    node.type = FunctionFlowNodeType::Model;
    node.config.model.sampling.temperatureEnabled = true;
    node.config.model.sampling.temperature = 0.8;
    graph.nodes.append(node);

    int changed = 0;
    FunctionFlowNode emitted;
    QObject::connect(
        &inspector,
        &FunctionCanvasInspector::nodeChanged,
        &inspector,
        [&](const FunctionFlowNode &value) {
            ++changed;
            emitted = value;
        }
    );
    inspector.setGraphAndSelection(graph, node.id);

    QCheckBox *temperatureEnabled = inspector.findChild<QCheckBox *>(
        QStringLiteral("flowTemperatureEnabled")
    );
    QDoubleSpinBox *temperature = inspector.findChild<QDoubleSpinBox *>(
        QStringLiteral("flowTemperatureSpin")
    );
    QCheckBox *topPEnabled = inspector.findChild<QCheckBox *>(
        QStringLiteral("flowTopPEnabled")
    );
    QDoubleSpinBox *topP = inspector.findChild<QDoubleSpinBox *>(
        QStringLiteral("flowTopPSpin")
    );
    QVERIFY(temperatureEnabled);
    QVERIFY(temperature);
    QVERIFY(topPEnabled);
    QVERIFY(topP);
    QCOMPARE(changed, 0);
    QVERIFY(temperatureEnabled->isChecked());
    QCOMPARE(temperature->value(), 0.8);
    QVERIFY(!topPEnabled->isChecked());
    QVERIFY(!topP->isEnabled());

    topPEnabled->setChecked(true);
    topP->setValue(0.55);
    QVERIFY(changed >= 2);
    QVERIFY(emitted.config.model.sampling.topPEnabled);
    QCOMPARE(emitted.config.model.sampling.topP, 0.55);
}

void FunctionCanvasEditorTests::
inspectorDisplaysMigratedModelWithoutChangingTheDraft()
{
    FunctionCanvasInspectorOptions options;
    options.models.append(qMakePair(
        QStringLiteral("deepseek-v4-flash"),
        QStringLiteral("DeepSeek V4 Flash")
    ));
    options.models.append(qMakePair(
        QStringLiteral("claude:claude-sonnet-5"),
        QStringLiteral("Claude Sonnet 5")
    ));
    FunctionCanvasInspector inspector(options);

    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("model_legacy");
    node.type = FunctionFlowNodeType::Model;
    node.config.model.modelId =
        QStringLiteral("claude:claude-sonnet-4-6");
    graph.nodes.append(node);

    int nodeChangeCount = 0;
    QObject::connect(
        &inspector,
        &FunctionCanvasInspector::nodeChanged,
        &inspector,
        [&nodeChangeCount](const FunctionFlowNode &) {
            ++nodeChangeCount;
        }
    );

    inspector.setGraphAndSelection(graph, node.id);

    QComboBox *model = inspector.findChild<QComboBox *>(
        QStringLiteral("flowModelId")
    );
    QVERIFY(model);
    QCOMPARE(
        model->currentData().toString(),
        QStringLiteral("claude:claude-sonnet-5")
    );
    QCOMPARE(
        model->findData(
            QStringLiteral("claude:claude-sonnet-4-6")
        ),
        -1
    );
    QCOMPARE(
        graph.nodes.first().config.model.modelId,
        QStringLiteral("claude:claude-sonnet-4-6")
    );
    QCOMPARE(nodeChangeCount, 0);
}

void FunctionCanvasEditorTests::
paletteListsExactlyTheNineSupportedNodeTypes()
{
    FunctionCanvasPalette palette;
    QCOMPARE(palette.nodeTypes().size(), 9);
    QCOMPARE(palette.visibleNodeCount(), 9);
    QVERIFY(palette.minimumWidth() >= 224);
    QVERIFY(palette.maximumWidth() <= 248);
    palette.setFilterText(QString::fromUtf8("大模型"));
    QCOMPARE(palette.visibleNodeCount(), 1);
    palette.setFilterText(QString::fromUtf8("不存在"));
    QCOMPARE(palette.visibleNodeCount(), 0);
    palette.setFilterText(QString());
    QCOMPARE(palette.visibleNodeCount(), 9);
}

void FunctionCanvasEditorTests::
paletteUsesThreeSearchAwareCategories()
{
    FunctionCanvasPalette palette;
    palette.resize(240, 640);
    palette.show();
    QCoreApplication::processEvents();

    QWidget *sources = palette.findChild<QWidget *>(
        QStringLiteral("flowPaletteCategorySources")
    );
    QWidget *processing = palette.findChild<QWidget *>(
        QStringLiteral("flowPaletteCategoryProcessing")
    );
    QWidget *actions = palette.findChild<QWidget *>(
        QStringLiteral("flowPaletteCategoryActions")
    );
    QLabel *empty = palette.findChild<QLabel *>(
        QStringLiteral("flowPaletteEmptyLabel")
    );
    QVERIFY(sources);
    QVERIFY(processing);
    QVERIFY(actions);
    QVERIFY(empty);
    QVERIFY(sources->isVisibleTo(&palette));
    QVERIFY(processing->isVisibleTo(&palette));
    QVERIFY(actions->isVisibleTo(&palette));
    QVERIFY(!empty->isVisibleTo(&palette));

    palette.setFilterText(QString::fromUtf8("语音"));
    QCoreApplication::processEvents();
    QCOMPARE(palette.visibleNodeCount(), 1);
    QVERIFY(sources->isVisibleTo(&palette));
    QVERIFY(!processing->isVisibleTo(&palette));
    QVERIFY(!actions->isVisibleTo(&palette));
    QVERIFY(!empty->isVisibleTo(&palette));

    palette.setFilterText(QString::fromUtf8("绝对不存在的节点"));
    QCoreApplication::processEvents();
    QCOMPARE(palette.visibleNodeCount(), 0);
    QVERIFY(!sources->isVisibleTo(&palette));
    QVERIFY(!processing->isVisibleTo(&palette));
    QVERIFY(!actions->isVisibleTo(&palette));
    QVERIFY(empty->isVisibleTo(&palette));
    QCOMPARE(empty->text(), QString::fromUtf8("没有匹配的节点"));

    palette.setFilterText(QString());
    QCoreApplication::processEvents();
    QCOMPARE(palette.visibleNodeCount(), 9);
    QVERIFY(sources->isVisibleTo(&palette));
    QVERIFY(processing->isVisibleTo(&palette));
    QVERIFY(actions->isVisibleTo(&palette));
    QVERIFY(!empty->isVisibleTo(&palette));
}

void FunctionCanvasEditorTests::
paletteEntriesUseChineseGlyphsAndStableTooltips()
{
    FunctionCanvasPalette palette;
    const QVector<FunctionFlowNodeType> types = palette.nodeTypes();
    QCOMPARE(types.size(), 9);
    for (FunctionFlowNodeType type : types) {
        QPushButton *button = palette.findChild<QPushButton *>(
            QStringLiteral("flowPalette_%1")
                .arg(functionFlowNodeTypeId(type))
        );
        QVERIFY2(button, qPrintable(functionFlowNodeTypeId(type)));
        QVERIFY(button->text().contains(
            functionCanvasNodeGlyph(type)
        ));
        QVERIFY(button->text().contains(
            functionCanvasNodeDisplayName(type)
        ));
        QCOMPARE(
            button->toolTip(),
            QString::fromUtf8("点击放置，或拖到画布")
        );
        QVERIFY(button->styleSheet().contains(
            functionCanvasNodeAccent(type).name()
        ));
    }
}

void FunctionCanvasEditorTests::
repeatedPaletteDragsDoNotRetainDragObjects()
{
    int runnerCalls = 0;
    bool everyDragHasExpectedMime = true;
    bool everyDragHasExpectedType = true;
    bool everyDragUsesTheButtonAsSource = true;
    QPushButton *button = nullptr;
    FunctionCanvasPalette palette(
        nullptr,
        [&runnerCalls,
         &everyDragHasExpectedMime,
         &everyDragHasExpectedType,
         &everyDragUsesTheButtonAsSource,
         &button](QDrag &drag) {
            ++runnerCalls;
            everyDragHasExpectedMime =
                everyDragHasExpectedMime
                && drag.mimeData()
                && drag.mimeData()->hasFormat(
                    functionCanvasNodeMimeType()
                );
            everyDragHasExpectedType =
                everyDragHasExpectedType
                && drag.mimeData()
                && drag.mimeData()->data(
                    functionCanvasNodeMimeType()
                ) == functionFlowNodeTypeId(
                    FunctionFlowNodeType::VoiceSource
                ).toUtf8();
            everyDragUsesTheButtonAsSource =
                everyDragUsesTheButtonAsSource
                && drag.source() == button;
            return Qt::IgnoreAction;
        }
    );
    palette.show();
    QCoreApplication::processEvents();
    button = palette.findChild<QPushButton *>(
        QStringLiteral("flowPalette_%1").arg(
            functionFlowNodeTypeId(
                FunctionFlowNodeType::VoiceSource
            )
        )
    );
    QVERIFY(button);
    int chosenCalls = 0;
    connect(
        &palette,
        &FunctionCanvasPalette::nodeTypeChosen,
        &palette,
        [&chosenCalls](FunctionFlowNodeType) {
            ++chosenCalls;
        }
    );

    for (int attempt = 0; attempt < 3; ++attempt) {
        const QPoint start = button->rect().center();
        QMouseEvent press(
            QEvent::MouseButtonPress,
            start,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(button, &press);
        QMouseEvent move(
            QEvent::MouseMove,
            start + QPoint(
                QApplication::startDragDistance() + 4,
                0
            ),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(button, &move);
        QMouseEvent repeatedMove(
            QEvent::MouseMove,
            start + QPoint(
                QApplication::startDragDistance() + 12,
                0
            ),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(button, &repeatedMove);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            start,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(button, &release);
        QCOMPARE(button->findChildren<QDrag *>().size(), 0);
    }

    QCOMPARE(runnerCalls, 3);
    QVERIFY(everyDragHasExpectedMime);
    QVERIFY(everyDragHasExpectedType);
    QVERIFY(everyDragUsesTheButtonAsSource);
    QCOMPARE(chosenCalls, 0);

    button->click();
    QCOMPARE(chosenCalls, 1);
}

void FunctionCanvasEditorTests::
paletteNodeCanBeDroppedAtTheRequestedCanvasPosition()
{
    FakeEditorSettings fake;
    fake.state.editor.zoom = 1.75;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(900, 700);
    editor.show();
    QCoreApplication::processEvents();

    QWidget *viewport = editor.canvasView()->viewport();
    QVERIFY(viewport);
    QMimeData mime;
    mime.setData(
        functionCanvasNodeMimeType(),
        functionFlowNodeTypeId(FunctionFlowNodeType::Model).toUtf8()
    );
    const QPoint dropPosition(320, 240);
    QDragEnterEvent enter(
        QPoint(40, 40),
        Qt::CopyAction,
        &mime,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(viewport, &enter);
    QVERIFY(enter.isAccepted());

    QDragMoveEvent move(
        dropPosition,
        Qt::CopyAction,
        &mime,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(viewport, &move);
    QVERIFY2(
        move.isAccepted(),
        "canvas rejected drag movement after accepting the node"
    );

    const QPointF expectedScenePosition =
        editor.canvasView()->mapToScene(dropPosition);
    QDropEvent drop(
        QPointF(dropPosition),
        Qt::CopyAction,
        &mime,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(viewport, &drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(editor.controller()->graph().nodes.size(), 1);
    const FunctionFlowNode &node =
        editor.controller()->graph().nodes.first();
    QCOMPARE(node.type, FunctionFlowNodeType::Model);
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(node.id);
    QVERIFY(item);
    QVERIFY(
        QLineF(
            item->mapToScene(item->boundingRect().center()),
            expectedScenePosition
        ).length() < 1.0
    );
}

void FunctionCanvasEditorTests::
editorShellWidgetsStayOutsideTheGraphicsScene()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);

    const QStringList shellObjects = QStringList()
        << QStringLiteral("flowCanvasToolbar")
        << QStringLiteral("functionCanvasPalette")
        << QStringLiteral("functionCanvasSurface")
        << QStringLiteral("flowInspectorScroll")
        << QStringLiteral("flowCanvasFooter");
    for (const QString &name : shellObjects) {
        QWidget *widget = editor.findChild<QWidget *>(name);
        QVERIFY2(widget, qPrintable(name));
        QVERIFY2(
            &editor == widget || editor.isAncestorOf(widget),
            qPrintable(name)
        );
    }
    for (QGraphicsItem *item : editor.canvasScene()->items()) {
        QVERIFY(!dynamic_cast<QGraphicsProxyWidget *>(item));
    }
}

void FunctionCanvasEditorTests::
footerUsesOneRowAndClearButtonRoles()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);

    QWidget *footer = editor.findChild<QWidget *>(
        QStringLiteral("flowCanvasFooter")
    );
    QVERIFY(footer);
    QHBoxLayout *row = qobject_cast<QHBoxLayout *>(footer->layout());
    QVERIFY(row);
    const QStringList rowObjects = QStringList()
        << QStringLiteral("flowCountsLabel")
        << QStringLiteral("flowDraftStatusLabel")
        << QStringLiteral("flowPublishedStatusLabel")
        << QStringLiteral("flowRunStatusLabel")
        << QStringLiteral("flowPublishButton");
    for (const QString &name : rowObjects) {
        QWidget *widget = footer->findChild<QWidget *>(name);
        QVERIFY2(widget, qPrintable(name));
        QVERIFY2(row->indexOf(widget) >= 0, qPrintable(name));
    }
    QPushButton *publish = footer->findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    );
    QCOMPARE(
        publish->property("buttonRole").toString(),
        QStringLiteral("primary")
    );
    QCOMPARE(publish->text(), QString::fromUtf8("发布流程"));
    QVERIFY(!footer->findChild<QPushButton *>(
        QStringLiteral("flowDisableButton")
    ));
    QVERIFY(!visibleText(footer).contains(QString::fromUtf8("快捷键")));
}

void FunctionCanvasEditorTests::
footerDoesNotShowTriggerAvailabilityLine()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(900, 700);
    editor.show();
    QCoreApplication::processEvents();

    QLabel *triggerStatus = editor.findChild<QLabel *>(
        QStringLiteral("flowTriggerStatusLabel")
    );
    QVERIFY(!triggerStatus || !triggerStatus->isVisibleTo(&editor));
}

void FunctionCanvasEditorTests::
compactEditorKeepsToolbarAndFooterControlsInside()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(760, 640);
    editor.show();
    QCoreApplication::processEvents();

    QToolButton *place = editor.findChild<QToolButton *>(
        QStringLiteral("flowPlaceButton")
    );
    QVERIFY(place);
    place->setChecked(true);
    QCoreApplication::processEvents();

    QVERIFY2(
        editor.minimumSizeHint().width() <= 760,
        qPrintable(QStringLiteral("minimum width is %1")
            .arg(editor.minimumSizeHint().width()))
    );
    QVERIFY2(
        editor.minimumSizeHint().height() <= 640,
        qPrintable(QStringLiteral("minimum height is %1")
            .arg(editor.minimumSizeHint().height()))
    );
    QCOMPARE(
        editor.canvasView()->horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );
    QCOMPARE(
        editor.canvasView()->verticalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );
    const QStringList requiredControls = QStringList()
        << QStringLiteral("flowZoomOutButton")
        << QStringLiteral("flowZoomInButton")
        << QStringLiteral("flowFitButton")
        << QStringLiteral("flowPublishButton");
    for (const QString &name : requiredControls) {
        QWidget *control = editor.findChild<QWidget *>(name);
        QVERIFY2(control, qPrintable(name));
        const QRect bounds(
            control->mapTo(&editor, QPoint()),
            control->size()
        );
        QVERIFY2(
            editor.rect().contains(bounds),
            qPrintable(QStringLiteral("%1 outside %2x%3 at %4,%5 %6x%7")
                .arg(name)
                .arg(editor.width())
                .arg(editor.height())
                .arg(bounds.x())
                .arg(bounds.y())
                .arg(bounds.width())
                .arg(bounds.height()))
        );
    }

    const QStringList statusLabels = QStringList()
        << QStringLiteral("flowCountsLabel")
        << QStringLiteral("flowDraftStatusLabel")
        << QStringLiteral("flowPublishedStatusLabel")
        << QStringLiteral("flowRunStatusLabel");
    for (const QString &name : statusLabels) {
        QLabel *label = editor.findChild<QLabel *>(name);
        QVERIFY2(label, qPrintable(name));
        QVERIFY2(
            label->sizePolicy().horizontalPolicy()
                != QSizePolicy::Ignored,
            qPrintable(QStringLiteral(
                "%1 uses an ignored horizontal size policy"
            ).arg(name))
        );
        if (label->isVisibleTo(&editor)
            && !label->text().isEmpty()) {
            const int textWidth =
                QFontMetrics(label->font()).horizontalAdvance(label->text());
            QVERIFY2(
                label->width() >= textWidth,
                qPrintable(QStringLiteral(
                    "%1 width %2 is smaller than text width %3"
                ).arg(name)
                 .arg(label->width())
                 .arg(textWidth))
            );
        }
    }
}

void FunctionCanvasEditorTests::
denseFooterKeepsLongCountsAndRevisionsInside()
{
    FakeEditorSettings fake;
    fake.state.draft.revision = 2147483646;
    fake.state.published.revision = 2147483647;
    for (int index = 0; index < 123; ++index) {
        FunctionFlowNode node;
        node.id = QStringLiteral("node_%1").arg(index);
        node.type = FunctionFlowNodeType::Input;
        node.position = QPointF(
            (index % 20) * 120.0,
            (index / 20) * 90.0
        );
        fake.state.draft.graph.nodes.append(node);
    }
    for (int index = 0; index < 456; ++index) {
        FunctionFlowEdge edge;
        edge.id = QStringLiteral("edge_%1").arg(index);
        edge.fromNodeId = QStringLiteral("node_0");
        edge.fromPortId = QStringLiteral("text_out");
        edge.toNodeId = QStringLiteral("node_1");
        edge.toPortId = QStringLiteral("text_in");
        fake.state.draft.graph.edges.append(edge);
    }
    fake.state.draft.graphHash =
        functionFlowGraphHash(fake.state.draft.graph);

    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    QCOMPARE(
        editor.controller()->flowState().draft.revision,
        2147483646
    );

    QWidget *footer = editor.findChild<QWidget *>(
        QStringLiteral("flowCanvasFooter")
    );
    QLabel *counts = editor.findChild<QLabel *>(
        QStringLiteral("flowCountsLabel")
    );
    QLabel *draft = editor.findChild<QLabel *>(
        QStringLiteral("flowDraftStatusLabel")
    );
    QLabel *published = editor.findChild<QLabel *>(
        QStringLiteral("flowPublishedStatusLabel")
    );
    QPushButton *publish = editor.findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    );
    QVERIFY(footer);
    QVERIFY(counts);
    QVERIFY(draft);
    QVERIFY(published);
    QVERIFY(publish);
    QCOMPARE(
        counts->text(),
        QString::fromUtf8("节点 123 · 连线 456")
    );
    QCOMPARE(draft->text(), QString::fromUtf8("已保存"));
    QCOMPARE(
        published->text(),
        QString::fromUtf8("版本 2147483647")
    );
    QCOMPARE(publish->text(), QString::fromUtf8("发布流程"));

    const QList<QSize> sizes =
        QList<QSize>() << QSize(760, 640) << QSize(1320, 760);
    const QList<QLabel *> labels =
        QList<QLabel *>() << counts << draft << published;
    for (const QSize &size : sizes) {
        editor.resize(size);
        editor.show();
        QCoreApplication::processEvents();
        QCOMPARE(editor.size(), size);
        QVERIFY(footer->isVisibleTo(&editor));
        QVERIFY(publish->isVisibleTo(&editor));
        const QRect footerBounds(
            footer->mapTo(&editor, QPoint()),
            footer->size()
        );
        QVERIFY2(
            editor.rect().contains(footerBounds),
            qPrintable(QStringLiteral(
                "footer outside %1x%2 at %3,%4 %5x%6"
            )
                .arg(size.width())
                .arg(size.height())
                .arg(footerBounds.x())
                .arg(footerBounds.y())
                .arg(footerBounds.width())
                .arg(footerBounds.height()))
        );
        for (QLabel *label : labels) {
            QVERIFY2(
                label->isVisibleTo(&editor),
                qPrintable(label->objectName())
            );
            const int requiredWidth =
                label->fontMetrics().horizontalAdvance(label->text());
            const QRect bounds(
                label->mapTo(footer, QPoint()),
                label->size()
            );
            QVERIFY2(
                footer->rect().contains(bounds),
                qPrintable(QStringLiteral(
                    "%1 outside footer at %2,%3 %4x%5"
                )
                    .arg(label->objectName())
                    .arg(bounds.x())
                    .arg(bounds.y())
                    .arg(bounds.width())
                    .arg(bounds.height()))
            );
            QVERIFY2(
                label->width() >= requiredWidth,
                qPrintable(QStringLiteral(
                    "%1 needs %2, has %3 in %4x%5"
                )
                    .arg(label->objectName())
                    .arg(requiredWidth)
                    .arg(label->width())
                    .arg(size.width())
                    .arg(size.height()))
            );
        }
        const QRect publishBounds(
            publish->mapTo(footer, QPoint()),
            publish->size()
        );
        QVERIFY2(
            footer->rect().contains(publishBounds),
            qPrintable(QStringLiteral(
                "publish outside footer at %1,%2 %3x%4 in %5x%6"
            )
                .arg(publishBounds.x())
                .arg(publishBounds.y())
                .arg(publishBounds.width())
                .arg(publishBounds.height())
                .arg(size.width())
                .arg(size.height()))
        );
        QVERIFY2(
            published->mapTo(footer, QPoint()).x()
                + published->width()
                <= publishBounds.x(),
            qPrintable(QStringLiteral(
                "published right %1 overlaps publish x %2"
            )
                .arg(published->mapTo(footer, QPoint()).x()
                    + published->width())
                .arg(publishBounds.x()))
        );
    }
}

void FunctionCanvasEditorTests::
compactEditorKeepsOnlyTheLastOpenedSidePanel()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(760, 640);
    editor.show();
    QCoreApplication::processEvents();

    QToolButton *place = editor.findChild<QToolButton *>(
        QStringLiteral("flowPlaceButton")
    );
    QScrollArea *inspectorScroll = editor.findChild<QScrollArea *>(
        QStringLiteral("flowInspectorScroll")
    );
    QVERIFY(place);
    QVERIFY(inspectorScroll);
    const EditorInvariantSnapshot beforePalette =
        editorInvariantSnapshot(editor);
    place->click();
    QCoreApplication::processEvents();
    QVERIFY(editor.palette()->isVisibleTo(&editor));
    QVERIFY(!inspectorScroll->isVisibleTo(&editor));
    const EditorInvariantSnapshot afterPalette =
        editorInvariantSnapshot(editor);
    QCOMPARE(afterPalette.graphHash, beforePalette.graphHash);
    QCOMPARE(afterPalette.zoom, beforePalette.zoom);
    QCOMPARE(afterPalette.viewportCenter, beforePalette.viewportCenter);
    QCOMPARE(afterPalette.undoCount, beforePalette.undoCount);

    const QString id = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(20.0, 20.0)
    );
    QVERIFY(!id.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(id);
    QVERIFY(item);
    const EditorInvariantSnapshot beforeInspector =
        editorInvariantSnapshot(editor);
    QVERIFY(requestNodeSettings(item));
    QCoreApplication::processEvents();
    QVERIFY(!editor.palette()->isVisibleTo(&editor));
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
    const EditorInvariantSnapshot afterInspector =
        editorInvariantSnapshot(editor);
    QCOMPARE(afterInspector.graphHash, beforeInspector.graphHash);
    QCOMPARE(afterInspector.zoom, beforeInspector.zoom);
    QCOMPARE(
        afterInspector.viewportCenter,
        beforeInspector.viewportCenter
    );
    QCOMPARE(afterInspector.undoCount, beforeInspector.undoCount);

    const EditorInvariantSnapshot beforePaletteAgain =
        editorInvariantSnapshot(editor);
    place->click();
    QCoreApplication::processEvents();
    QVERIFY(editor.palette()->isVisibleTo(&editor));
    QVERIFY(!inspectorScroll->isVisibleTo(&editor));
    const EditorInvariantSnapshot afterPaletteAgain =
        editorInvariantSnapshot(editor);
    QCOMPARE(
        afterPaletteAgain.graphHash,
        beforePaletteAgain.graphHash
    );
    QCOMPARE(afterPaletteAgain.zoom, beforePaletteAgain.zoom);
    QCOMPARE(
        afterPaletteAgain.viewportCenter,
        beforePaletteAgain.viewportCenter
    );
    QCOMPARE(
        afterPaletteAgain.undoCount,
        beforePaletteAgain.undoCount
    );
}

void FunctionCanvasEditorTests::
wideEditorAllowsBothSidePanels()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();

    QToolButton *place = editor.findChild<QToolButton *>(
        QStringLiteral("flowPlaceButton")
    );
    QScrollArea *inspectorScroll = editor.findChild<QScrollArea *>(
        QStringLiteral("flowInspectorScroll")
    );
    QVERIFY(place);
    QVERIFY(inspectorScroll);
    const EditorInvariantSnapshot beforePalette =
        editorInvariantSnapshot(editor);
    place->click();
    QCoreApplication::processEvents();
    const EditorInvariantSnapshot afterPalette =
        editorInvariantSnapshot(editor);
    QCOMPARE(afterPalette.graphHash, beforePalette.graphHash);
    QCOMPARE(afterPalette.zoom, beforePalette.zoom);
    QCOMPARE(afterPalette.viewportCenter, beforePalette.viewportCenter);
    QCOMPARE(afterPalette.undoCount, beforePalette.undoCount);

    const QString id = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(20.0, 20.0)
    );
    QVERIFY(!id.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(id);
    QVERIFY(item);
    const EditorInvariantSnapshot beforeInspector =
        editorInvariantSnapshot(editor);
    QVERIFY(requestNodeSettings(item));
    QCoreApplication::processEvents();
    const EditorInvariantSnapshot afterInspector =
        editorInvariantSnapshot(editor);

    QVERIFY(editor.palette()->isVisibleTo(&editor));
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
    QVERIFY(editor.canvasView()->width() >= 460);
    QCOMPARE(afterInspector.graphHash, beforeInspector.graphHash);
    QCOMPARE(afterInspector.zoom, beforeInspector.zoom);
    QCOMPARE(
        afterInspector.viewportCenter,
        beforeInspector.viewportCenter
    );
    QCOMPARE(afterInspector.undoCount, beforeInspector.undoCount);
}

void FunctionCanvasEditorTests::
resizingAcrossSidePanelThresholdPreservesEditorState()
{
    FakeEditorSettings fake;
    fake.state.editor.viewportCenter = QPointF(900.0, 700.0);
    fake.state.editor.zoom = 1.25;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();
    QCOMPARE(
        editor.canvasView()->viewportCenter(),
        editor.controller()->editorState().viewportCenter
    );
    QCOMPARE(
        editor.canvasView()->zoomLevel(),
        editor.controller()->editorState().zoom
    );

    QToolButton *place = editor.findChild<QToolButton *>(
        QStringLiteral("flowPlaceButton")
    );
    QScrollArea *inspectorScroll = editor.findChild<QScrollArea *>(
        QStringLiteral("flowInspectorScroll")
    );
    QVERIFY(place);
    QVERIFY(inspectorScroll);
    place->click();
    const QString id = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(20.0, 20.0)
    );
    QVERIFY(!id.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(id);
    QVERIFY(item);
    QVERIFY(requestNodeSettings(item));
    QCoreApplication::processEvents();
    QVERIFY(editor.palette()->isVisibleTo(&editor));
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
    const FunctionFlowEditorState controllerAfterInitialLayout =
        editor.controller()->editorState();
    QCOMPARE(
        editor.canvasView()->viewportCenter(),
        controllerAfterInitialLayout.viewportCenter
    );
    QCOMPARE(
        editor.canvasView()->zoomLevel(),
        controllerAfterInitialLayout.zoom
    );
    QVERIFY(editor.flushPendingEditorState());

    QSignalSpy editorStateChanged(
        editor.controller(),
        &FunctionFlowEditorController::editorStateChanged
    );

    const EditorInvariantSnapshot beforeCompact =
        editorInvariantSnapshot(editor);
    const FunctionFlowEditorState controllerBeforeCompact =
        editor.controller()->editorState();
    const int signalsBeforeCompact = editorStateChanged.count();
    const int savesBeforeCompact = fake.editorSaves;

    editor.resize(760, 640);
    QCoreApplication::processEvents();
    QVERIFY(editor.flushPendingEditorState());
    const EditorInvariantSnapshot afterCompact =
        editorInvariantSnapshot(editor);
    const FunctionFlowEditorState controllerAfterCompact =
        editor.controller()->editorState();
    QCOMPARE(afterCompact.graphHash, beforeCompact.graphHash);
    QCOMPARE(afterCompact.zoom, beforeCompact.zoom);
    QCOMPARE(
        afterCompact.viewportCenter,
        beforeCompact.viewportCenter
    );
    QCOMPARE(afterCompact.undoCount, beforeCompact.undoCount);
    QCOMPARE(
        controllerAfterCompact.viewportCenter,
        controllerBeforeCompact.viewportCenter
    );
    QCOMPARE(
        controllerAfterCompact.zoom,
        controllerBeforeCompact.zoom
    );
    QCOMPARE(
        afterCompact.viewportCenter,
        controllerAfterCompact.viewportCenter
    );
    QCOMPARE(afterCompact.zoom, controllerAfterCompact.zoom);
    QCOMPARE(editorStateChanged.count(), signalsBeforeCompact);
    QCOMPARE(fake.editorSaves, savesBeforeCompact);
    QVERIFY(!editor.palette()->isVisibleTo(&editor));
    QVERIFY(inspectorScroll->isVisibleTo(&editor));

    const EditorInvariantSnapshot beforeWide =
        editorInvariantSnapshot(editor);
    const FunctionFlowEditorState controllerBeforeWide =
        editor.controller()->editorState();
    const int signalsBeforeWide = editorStateChanged.count();
    const int savesBeforeWide = fake.editorSaves;

    editor.resize(1320, 760);
    QCoreApplication::processEvents();
    QVERIFY(editor.flushPendingEditorState());
    const EditorInvariantSnapshot afterWide =
        editorInvariantSnapshot(editor);
    const FunctionFlowEditorState controllerAfterWide =
        editor.controller()->editorState();
    QCOMPARE(afterWide.graphHash, beforeWide.graphHash);
    QCOMPARE(afterWide.zoom, beforeWide.zoom);
    QCOMPARE(afterWide.viewportCenter, beforeWide.viewportCenter);
    QCOMPARE(afterWide.undoCount, beforeWide.undoCount);
    QCOMPARE(
        controllerAfterWide.viewportCenter,
        controllerBeforeWide.viewportCenter
    );
    QCOMPARE(controllerAfterWide.zoom, controllerBeforeWide.zoom);
    QCOMPARE(
        afterWide.viewportCenter,
        controllerAfterWide.viewportCenter
    );
    QCOMPARE(afterWide.zoom, controllerAfterWide.zoom);
    QCOMPARE(editorStateChanged.count(), signalsBeforeWide);
    QCOMPARE(fake.editorSaves, savesBeforeWide);
    QVERIFY(editor.palette()->isVisibleTo(&editor));
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
}

void FunctionCanvasEditorTests::
zoomLabelTracksRestoredAndToolbarZoom()
{
    FakeEditorSettings fake;
    fake.state.editor.zoom = 1.5;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);

    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    QLabel *zoomLabel = editor.findChild<QLabel *>(
        QStringLiteral("flowZoomLabel")
    );
    QToolButton *zoomIn = editor.findChild<QToolButton *>(
        QStringLiteral("flowZoomInButton")
    );
    QVERIFY(zoomLabel);
    QVERIFY(zoomIn);
    QCOMPARE(zoomLabel->text(), QStringLiteral("150%"));

    zoomIn->click();
    QCOMPARE(zoomLabel->text(), QStringLiteral("177%"));
}

void FunctionCanvasEditorTests::
nodeCardsUseChinesePortLabels()
{
    const QStringList inputTexts =
        paintedNodeTexts(FunctionFlowNodeType::Input);
    QVERIFY(inputTexts.contains(QString::fromUtf8("文字输入")));
    QVERIFY(inputTexts.contains(QString::fromUtf8("文字输出")));
    QVERIFY(!inputTexts.contains(QStringLiteral("text in")));
    QVERIFY(!inputTexts.contains(QStringLiteral("text out")));

    const QStringList outputTexts =
        paintedNodeTexts(FunctionFlowNodeType::Output);
    QVERIFY(outputTexts.contains(QString::fromUtf8("动作输出")));
    QVERIFY(!outputTexts.contains(QStringLiteral("action out")));

    const QStringList actionTexts =
        paintedNodeTexts(FunctionFlowNodeType::ResultPopup);
    QVERIFY(actionTexts.contains(QString::fromUtf8("动作输入")));
    QVERIFY(!actionTexts.contains(QStringLiteral("action in")));
}

void FunctionCanvasEditorTests::
inspectorUsesStableSectionsForEveryNodeType()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };
    const QString expectedTypedSections[] = {
        QStringLiteral("flowInspectorSectionInputSource"),
        QStringLiteral("flowInspectorSectionInputSource"),
        QStringLiteral("flowInspectorSectionInputSource"),
        QStringLiteral("flowInspectorSectionInputSource"),
        QStringLiteral("flowInspectorSectionProcessing"),
        QStringLiteral("flowInspectorSectionDisplayOutput"),
        QStringLiteral("flowInspectorSectionDisplayOutput"),
        QStringLiteral("flowInspectorSectionDisplayOutput"),
        QStringLiteral("flowInspectorSectionDisplayOutput")
    };
    const QStringList allTypedSections = QStringList()
        << QStringLiteral("flowInspectorSectionInputSource")
        << QStringLiteral("flowInspectorSectionProcessing")
        << QStringLiteral("flowInspectorSectionDisplayOutput");

    for (int index = 0; index < 9; ++index) {
        FunctionFlowGraph graph;
        FunctionFlowNode node;
        node.id = QStringLiteral("section_node_%1").arg(index);
        node.type = types[index];
        graph.nodes.append(node);
        inspector.setGraphAndSelection(graph, node.id);

        QLabel *basic = inspector.findChild<QLabel *>(
            QStringLiteral("flowInspectorSectionBasic")
        );
        QLabel *typed = inspector.findChild<QLabel *>(
            expectedTypedSections[index]
        );
        QFrame *basicDivider = inspector.findChild<QFrame *>(
            QStringLiteral("flowInspectorSectionBasicDivider")
        );
        QFrame *typedDivider = inspector.findChild<QFrame *>(
            expectedTypedSections[index] + QStringLiteral("Divider")
        );
        QVERIFY2(basic, "每种节点都必须显示基础设置分组");
        QVERIFY2(typed, "节点必须显示唯一适用的类型设置分组");
        QVERIFY(basicDivider);
        QVERIFY(typedDivider);
        QCOMPARE(basic->text(), QString::fromUtf8("基础设置"));

        for (const QString &sectionName : allTypedSections) {
            QLabel *section =
                inspector.findChild<QLabel *>(sectionName);
            QCOMPARE(
                section != nullptr,
                sectionName == expectedTypedSections[index]
            );
        }

        QLineEdit *name = inspector.findChild<QLineEdit *>(
            QStringLiteral("flowNodeNameEdit")
        );
        QCheckBox *enabled = inspector.findChild<QCheckBox *>(
            QStringLiteral("flowNodeEnabled")
        );
        QVERIFY(name);
        QVERIFY(enabled);
        QLayout *layout = inspector.layout();
        QVERIFY(layout);
        const int basicIndex = layout->indexOf(basic);
        const int nameIndex = layout->indexOf(name->parentWidget());
        const int enabledIndex = layout->indexOf(enabled);
        const int typedIndex = layout->indexOf(typed);
        QVERIFY(basicIndex >= 0);
        QCOMPARE(
            layout->indexOf(basicDivider),
            basicIndex + 1
        );
        QVERIFY(nameIndex > basicIndex);
        QVERIFY(enabledIndex > nameIndex);
        QVERIFY(typedIndex > enabledIndex);
        QCOMPARE(
            layout->indexOf(typedDivider),
            typedIndex + 1
        );

        for (int itemIndex = enabledIndex + 1;
             itemIndex < typedIndex;
             ++itemIndex) {
            QWidget *widget = layout->itemAt(itemIndex)->widget();
            QVERIFY(widget);
            QVERIFY2(
                widget->objectName()
                    == QStringLiteral(
                        "flowInspectorSectionBasicDivider"
                    ),
                "基础设置分组只能包含节点名称和启用状态"
            );
        }
    }
}

void FunctionCanvasEditorTests::
inspectorScalesChineseTextAndUsesTheAppFont()
{
    QScrollArea scroll;
    scroll.setWidgetResizable(true);
    auto *inspector = new FunctionCanvasInspector(
        inspectorOptions()
    );
    scroll.setWidget(inspector);

    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("screenshot_scaled");
    node.type = FunctionFlowNodeType::ScreenshotSource;
    node.title = QString::fromUtf8(
        "需要在高缩放环境完整显示的截图文字识别节点"
    );
    graph.nodes.append(node);
    inspector->setGraphAndSelection(graph, node.id);

    const QString expectedFamily =
        QStringLiteral("Microsoft YaHei UI");
    const QList<QLineEdit *> edits =
        inspector->findChildren<QLineEdit *>();
    const QList<QComboBox *> combos =
        inspector->findChildren<QComboBox *>();
    const QList<QAbstractButton *> buttons =
        inspector->findChildren<QAbstractButton *>();
    const QList<QSpinBox *> spins =
        inspector->findChildren<QSpinBox *>();
    QVERIFY(!edits.isEmpty());
    QVERIFY(!combos.isEmpty());
    QVERIFY(!buttons.isEmpty());
    QVERIFY(!spins.isEmpty());
    for (QLineEdit *edit : edits) {
        QCOMPARE(edit->font().family(), expectedFamily);
    }
    for (QComboBox *combo : combos) {
        QCOMPARE(combo->font().family(), expectedFamily);
    }
    for (QAbstractButton *button : buttons) {
        QCOMPARE(button->font().family(), expectedFamily);
    }
    for (QSpinBox *spin : spins) {
        QCOMPARE(spin->font().family(), expectedFamily);
    }

    for (QLabel *label : inspector->findChildren<QLabel *>()) {
        QVERIFY2(
            label->wordWrap(),
            qPrintable(QStringLiteral("标签未启用换行：")
                + label->text())
        );
        QFont scaled = label->font();
        scaled.setPointSizeF(
            qMax(1.0, scaled.pointSizeF()) * 1.5
        );
        label->setFont(scaled);
    }

    scroll.resize(330, 260);
    scroll.show();
    QCoreApplication::processEvents();
    inspector->layout()->activate();
    QCoreApplication::processEvents();

    QLabel *languages = inspector->findChild<QLabel *>(
        QStringLiteral("flowScreenshotLanguages")
    );
    QLabel *section = inspector->findChild<QLabel *>(
        QStringLiteral("flowInspectorSectionInputSource")
    );
    QVERIFY(languages);
    QVERIFY(section);
    QCOMPARE(languages->maximumHeight(), QWIDGETSIZE_MAX);
    QCOMPARE(section->maximumHeight(), QWIDGETSIZE_MAX);
    QVERIFY(languages->height() >= languages->sizeHint().height());
    QVERIFY(section->height() >= section->sizeHint().height());
    QVERIFY(scroll.verticalScrollBar()->maximum() > 0);
}

void FunctionCanvasEditorTests::
inspectorShowsOnlyTheSpecifiedTypedSettings()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };
    QString allText;
    for (int index = 0; index < 9; ++index) {
        FunctionFlowGraph graph;
        FunctionFlowNode node;
        node.id = QStringLiteral("node_%1").arg(index);
        node.type = types[index];
        graph.nodes.append(node);
        inspector.setGraphAndSelection(graph, node.id);
        QCoreApplication::processEvents();
        const QString text = visibleText(&inspector);
        allText += text;
        QVERIFY(text.contains(QString::fromUtf8("节点名称")));
        QVERIFY(text.contains(QString::fromUtf8("启用此节点")));
        if (node.type == FunctionFlowNodeType::VoiceSource) {
            QLineEdit *device = inspector.findChild<QLineEdit *>(
                QStringLiteral("flowVoiceDevice")
            );
            QVERIFY(device);
            QVERIFY(device->isReadOnly());
            QCOMPARE(device->text(), QString::fromUtf8("系统默认"));
        }
        if (node.type == FunctionFlowNodeType::ScreenshotSource) {
            QVERIFY(text.contains(QString::fromUtf8("文字识别引擎")));
            QVERIFY(text.contains(
                QString::fromUtf8("识别语言：简体中文 / 英文（只读）")
            ));
            QVERIFY(!text.contains(QStringLiteral("zh-Hans")));
            QVERIFY(!text.contains(QStringLiteral(" ms")));
        }
        if (node.type == FunctionFlowNodeType::Model) {
            QVERIFY(text.contains(QString::fromUtf8("等待全部输入")));
        }
    }
    QVERIFY(!allText.contains(QString::fromUtf8("保留格式")));
    QVERIFY(!allText.contains(QStringLiteral("eachInput")));
    QVERIFY(!allText.contains(QString::fromUtf8("目标节点")));
    QVERIFY(!allText.contains(QString::fromUtf8("目标动作")));
}

void FunctionCanvasEditorTests::
inputRoleShowsChineseButEmitsTheStableRoleId()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("input_1");
    node.type = FunctionFlowNodeType::Input;
    node.config.input.role = QStringLiteral("source");
    graph.nodes.append(node);

    FunctionFlowNode changed;
    QObject::connect(
        &inspector,
        &FunctionCanvasInspector::nodeChanged,
        &inspector,
        [&](const FunctionFlowNode &value) {
            changed = value;
        }
    );
    inspector.setGraphAndSelection(graph, node.id);

    QComboBox *role = inspector.findChild<QComboBox *>(
        QStringLiteral("flowInputRole")
    );
    QVERIFY(role);
    QVERIFY(role->isEditable());
    QCOMPARE(role->currentText(), QString::fromUtf8("原文"));
    QCOMPARE(role->currentData().toString(), QStringLiteral("source"));
    QVERIFY(role->findData(QStringLiteral("instruction")) >= 0);
    QCOMPARE(
        role->itemText(role->findData(QStringLiteral("instruction"))),
        QString::fromUtf8("指令")
    );

    role->setCurrentIndex(
        role->findData(QStringLiteral("instruction"))
    );
    QCOMPARE(
        changed.config.input.role,
        QStringLiteral("instruction")
    );

    role->setEditText(QString::fromUtf8("自定义角色"));
    QMetaObject::invokeMethod(role->lineEdit(), "editingFinished");
    QCOMPARE(
        changed.config.input.role,
        QString::fromUtf8("自定义角色")
    );
}

void FunctionCanvasEditorTests::
popupActionsShowChineseButKeepTheStableActionIds()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("popup_1");
    node.type = FunctionFlowNodeType::ResultPopup;
    node.config.popup.resultActions = QStringList()
        << QStringLiteral("regenerate")
        << QStringLiteral("retryModel")
        << QStringLiteral("followUp")
        << QStringLiteral("expand")
        << QStringLiteral("vocabulary")
        << QStringLiteral("copy")
        << QStringLiteral("write")
        << QStringLiteral("replace");
    graph.nodes.append(node);
    inspector.setGraphAndSelection(graph, node.id);

    QListWidget *actions = inspector.findChild<QListWidget *>(
        QStringLiteral("flowPopupActions")
    );
    QVERIFY(actions);
    const QStringList expectedLabels = QStringList()
        << QString::fromUtf8("重新生成")
        << QString::fromUtf8("重试模型")
        << QString::fromUtf8("继续追问")
        << QString::fromUtf8("展开")
        << QString::fromUtf8("加入词库")
        << QString::fromUtf8("复制")
        << QString::fromUtf8("写入")
        << QString::fromUtf8("替换");
    QCOMPARE(actions->count(), expectedLabels.size());
    for (int row = 0; row < actions->count(); ++row) {
        QCOMPARE(actions->item(row)->text(), expectedLabels.at(row));
        QCOMPARE(
            actions->item(row)->data(Qt::UserRole).toString(),
            node.config.popup.resultActions.at(row)
        );
    }
}

void FunctionCanvasEditorTests::
inspectorEmitsAWholeTypedNodeWithoutDroppingRetainedValues()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    FunctionFlowGraph graph;
    FunctionFlowNode node;
    node.id = QStringLiteral("input_1");
    node.type = FunctionFlowNodeType::Input;
    node.title = QString::fromUtf8("输入");
    node.retainedValues.insert(QStringLiteral("future"), 42);
    graph.nodes.append(node);

    FunctionFlowNode changed;
    int changes = 0;
    QObject::connect(
        &inspector,
        &FunctionCanvasInspector::nodeChanged,
        &inspector,
        [&](const FunctionFlowNode &value) {
            changed = value;
            ++changes;
        }
    );
    inspector.setGraphAndSelection(graph, node.id);
    QLineEdit *name = inspector.findChild<QLineEdit *>(
        QStringLiteral("flowNodeNameEdit")
    );
    QVERIFY(name);
    name->setText(QString::fromUtf8("主要输入"));
    QMetaObject::invokeMethod(name, "editingFinished");
    QCOMPARE(changes, 1);
    QCOMPARE(changed.title, QString::fromUtf8("主要输入"));
    QCOMPARE(
        changed.retainedValues.value(QStringLiteral("future")).toInt(),
        42
    );
}

void FunctionCanvasEditorTests::
inspectorOpacityControlsCannotProduceInvalidValues()
{
    FunctionCanvasInspector inspector(inspectorOptions());
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel
    };
    const QString names[] = {
        QStringLiteral("flowPopupOpacity"),
        QStringLiteral("flowPanelOpacity")
    };

    for (int index = 0; index < 2; ++index) {
        FunctionFlowGraph graph;
        FunctionFlowNode node;
        node.id = QStringLiteral("opacity_%1").arg(index);
        node.type = types[index];
        graph.nodes.append(node);
        inspector.setGraphAndSelection(graph, node.id);

        QComboBox *opacity = inspector.findChild<QComboBox *>(names[index]);
        QVERIFY(opacity);
        QVERIFY(opacity->findData(-1) >= 0);
        QVERIFY(opacity->findData(20) >= 0);
        QVERIFY(opacity->findData(100) >= 0);
        QCOMPARE(opacity->findData(0), -1);
        QCOMPARE(opacity->findData(19), -1);
    }
}

void FunctionCanvasEditorTests::
editorWiresSceneIntentsIntoTheUndoableWorkingGraph()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    QVERIFY(editor.canvasScene());
    QVERIFY(editor.canvasView());
    QVERIFY(editor.palette());
    QVERIFY(editor.inspector());
    QCOMPARE(editor.canvasScene()->nodeCount(), 0);

    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF(100.0, 120.0)
    );
    QCOMPARE(editor.controller()->graph().nodes.size(), 1);
    QCOMPARE(editor.canvasScene()->nodeCount(), 1);
    QCOMPARE(editor.controller()->undoStack()->count(), 1);
    QVERIFY(editor.flushPendingSave());
    QCOMPARE(fake.draftSaves, 1);
}

void FunctionCanvasEditorTests::
manyInputKeepsVoiceAndSelectionConnectionsInFinalGraph()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));

    const QString voiceId = editor.controller()->placeNode(
        FunctionFlowNodeType::VoiceSource,
        QPointF(100.0, 100.0)
    );
    const QString selectionId = editor.controller()->placeNode(
        FunctionFlowNodeType::SelectionSource,
        QPointF(100.0, 320.0)
    );
    const QString inputId = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(520.0, 210.0)
    );
    QVERIFY(!voiceId.isEmpty());
    QVERIFY(!selectionId.isEmpty());
    QVERIFY(!inputId.isEmpty());

    FunctionFlowEndpoint voiceOut;
    voiceOut.nodeId = voiceId;
    voiceOut.portId = QStringLiteral("text_out");
    FunctionFlowEndpoint selectionOut;
    selectionOut.nodeId = selectionId;
    selectionOut.portId = QStringLiteral("text_out");
    FunctionFlowEndpoint inputIn;
    inputIn.nodeId = inputId;
    inputIn.portId = QStringLiteral("text_in");

    QVERIFY(editor.canvasScene()->requestConnection(
        voiceOut,
        inputIn
    ));
    QVERIFY(editor.canvasScene()->requestConnection(
        selectionOut,
        inputIn
    ));

    const FunctionFlowGraph &graph = editor.controller()->graph();
    QCOMPARE(graph.edges.size(), 2);
    QCOMPARE(editor.canvasScene()->edgeCount(), 2);
    QSet<QString> sourceIds;
    for (const FunctionFlowEdge &connection : graph.edges) {
        QCOMPARE(connection.toNodeId, inputId);
        QCOMPARE(
            connection.toPortId,
            QStringLiteral("text_in")
        );
        QCOMPARE(
            connection.fromPortId,
            QStringLiteral("text_out")
        );
        sourceIds.insert(connection.fromNodeId);
        QVERIFY(editor.canvasScene()->edgeItem(
            connection.id
        ));
    }
    QCOMPARE(sourceIds.size(), 2);
    QVERIFY(sourceIds.contains(voiceId));
    QVERIFY(sourceIds.contains(selectionId));
}

void FunctionCanvasEditorTests::
nodeInspectorRequiresDoubleClick()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();

    const QPointF nodePosition =
        editor.canvasView()->viewportCenter()
        - FunctionCanvasNodeItem::boundsForType(
            FunctionFlowNodeType::Input
        ).center();
    const QString nodeId = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        nodePosition
    );
    QVERIFY(!nodeId.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(nodeId);
    QVERIFY(item);
    QCoreApplication::processEvents();

    QScrollArea *inspectorScroll = editor.findChild<QScrollArea *>(
        QStringLiteral("flowInspectorScroll")
    );
    QVERIFY(inspectorScroll);
    QVERIFY(!inspectorScroll->isVisibleTo(&editor));

    QWidget *viewport = editor.canvasView()->viewport();
    const QPoint nodeCenter = editor.canvasView()->mapFromScene(
        item->mapToScene(QPointF(100.0, 70.0))
    );
    QVERIFY(viewport->rect().contains(nodeCenter));

    QTest::mouseClick(
        viewport,
        Qt::LeftButton,
        Qt::NoModifier,
        nodeCenter
    );
    QCoreApplication::processEvents();
    QVERIFY(item->isSelected());
    QVERIFY(!inspectorScroll->isVisibleTo(&editor));
    QVERIFY(editor.inspector()->selectedNodeId().isEmpty());

    QTest::mouseDClick(
        viewport,
        Qt::LeftButton,
        Qt::NoModifier,
        nodeCenter
    );
    QCoreApplication::processEvents();
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
    QCOMPARE(editor.inspector()->selectedNodeId(), nodeId);
}

void FunctionCanvasEditorTests::
inspectorTextEditingOwnsDeleteAndBackspace()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();

    const QString inputId = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(520.0, 240.0)
    );
    QVERIFY(!inputId.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(inputId);
    QVERIFY(item);
    QVERIFY(requestNodeSettings(item));
    QCoreApplication::processEvents();

    QLineEdit *name = editor.inspector()->findChild<QLineEdit *>(
        QStringLiteral("flowNodeNameEdit")
    );
    QVERIFY(name);
    int nodeRemovalRequests = 0;
    int edgeRemovalRequests = 0;
    connect(
        editor.canvasScene(),
        &FunctionCanvasScene::nodeRemovalRequested,
        &editor,
        [&nodeRemovalRequests](const QString &) {
            ++nodeRemovalRequests;
        }
    );
    connect(
        editor.canvasScene(),
        &FunctionCanvasScene::edgeRemovalRequested,
        &editor,
        [&edgeRemovalRequests](const QString &) {
            ++edgeRemovalRequests;
        }
    );

    name->setText(QString::fromUtf8("甲乙"));
    name->setCursorPosition(1);
    name->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(name->hasFocus());
    QTest::keyClick(name, Qt::Key_Delete);
    QCOMPARE(name->text(), QString::fromUtf8("甲"));
    QCOMPARE(nodeRemovalRequests, 0);
    QCOMPARE(edgeRemovalRequests, 0);

    name->setText(QString::fromUtf8("甲乙"));
    name->setCursorPosition(1);
    QTest::keyClick(name, Qt::Key_Backspace);
    QCOMPARE(name->text(), QString::fromUtf8("乙"));
    QCOMPARE(nodeRemovalRequests, 0);
    QCOMPARE(edgeRemovalRequests, 0);
    QCOMPARE(editor.controller()->graph().nodes.size(), 1);
    QCOMPARE(editor.canvasScene()->nodeCount(), 1);
}

void FunctionCanvasEditorTests::
hidingInspectorPreservesGraphAndEditorState()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();

    const QString nodeId = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        QPointF(520.0, 240.0)
    );
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(nodeId);
    QVERIFY(item);
    QVERIFY(requestNodeSettings(item));
    QCoreApplication::processEvents();

    QScrollArea *inspectorScroll = editor.findChild<QScrollArea *>(
        QStringLiteral("flowInspectorScroll")
    );
    QVERIFY(inspectorScroll);
    QVERIFY(inspectorScroll->isVisibleTo(&editor));
    QCOMPARE(editor.inspector()->selectedNodeId(), nodeId);

    const EditorInvariantSnapshot before =
        editorInvariantSnapshot(editor);
    editor.canvasScene()->clearSelection();
    QCoreApplication::processEvents();
    const EditorInvariantSnapshot after =
        editorInvariantSnapshot(editor);

    QVERIFY(!inspectorScroll->isVisibleTo(&editor));
    QVERIFY(editor.inspector()->selectedNodeId().isEmpty());
    QCOMPARE(after.graphHash, before.graphHash);
    QCOMPARE(after.zoom, before.zoom);
    QCOMPARE(after.viewportCenter, before.viewportCenter);
    QCOMPARE(after.undoCount, before.undoCount);
}

void FunctionCanvasEditorTests::
nodeClickAndDragDoesNotPanTheCanvas()
{
    FakeEditorSettings fake;
    fake.state.editor.viewportCenter = QPointF(900.0, 650.0);
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.resize(1320, 760);
    editor.show();
    QCoreApplication::processEvents();

    const QPointF nodePosition =
        editor.canvasView()->viewportCenter()
        - FunctionCanvasNodeItem::boundsForType(
            FunctionFlowNodeType::Input
        ).center();
    const QString inputId = editor.controller()->placeNode(
        FunctionFlowNodeType::Input,
        nodePosition
    );
    QVERIFY(!inputId.isEmpty());
    FunctionCanvasNodeItem *item =
        editor.canvasScene()->nodeItem(inputId);
    QVERIFY(item);
    QCoreApplication::processEvents();

    const QPointF centerBefore =
        editor.canvasView()->viewportCenter();
    const QPointF positionBefore = item->pos();
    const int undoBefore =
        editor.controller()->undoStack()->count();
    const QPoint pressPosition = editor.canvasView()->mapFromScene(
        item->mapToScene(QPointF(100.0, 70.0))
    );
    QWidget *viewport = editor.canvasView()->viewport();
    QVERIFY(viewport->rect().contains(pressPosition));
    QCOMPARE(
        dynamic_cast<FunctionCanvasNodeItem *>(
            editor.canvasView()->itemAt(pressPosition)
        ),
        item
    );

    QTest::mousePress(
        viewport,
        Qt::LeftButton,
        Qt::NoModifier,
        pressPosition
    );
    QCoreApplication::processEvents();
    QVERIFY(item->isSelected());

    const QPoint releasePosition =
        pressPosition + QPoint(58, 34);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(releasePosition),
        QPointF(viewport->mapToGlobal(releasePosition)),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    move.setAccepted(false);
    QApplication::sendEvent(viewport, &move);
    QTest::mouseRelease(
        viewport,
        Qt::LeftButton,
        Qt::NoModifier,
        releasePosition,
        10
    );
    QCoreApplication::processEvents();

    QCOMPARE(
        editor.canvasView()->viewportCenter(),
        centerBefore
    );
    QVERIFY(item->pos() != positionBefore);
    QCOMPARE(
        editor.controller()->undoStack()->count(),
        undoBefore + 1
    );
    for (const FunctionFlowNode &node :
         editor.controller()->graph().nodes) {
        if (node.id == inputId) {
            QCOMPARE(node.position, item->pos());
            return;
        }
    }
    QFAIL("dragged node missing from the final controller graph");
}

void FunctionCanvasEditorTests::
viewportChangesStayOutsideTheGraphUndoStack()
{
    FakeEditorSettings fake;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    const QString before =
        functionFlowGraphHash(editor.controller()->graph());
    FunctionFlowEditorState viewport;
    viewport.viewportCenter = QPointF(500.0, 400.0);
    viewport.zoom = 1.4;
    editor.controller()->updateEditorState(viewport);
    QCOMPARE(editor.controller()->undoStack()->count(), 0);
    QCOMPARE(
        functionFlowGraphHash(editor.controller()->graph()),
        before
    );
    QVERIFY(editor.flushPendingEditorState());
    QCOMPARE(fake.editorSaves, 1);
    QCOMPARE(fake.draftSaves, 0);
}

void FunctionCanvasEditorTests::
unsupportedDraftIsReadOnlyAndNeverOverwritesRawData()
{
    FakeEditorSettings fake;
    fake.state.draft.supported = false;
    fake.state.draft.unavailableCode =
        QStringLiteral("flow_schema_newer");
    fake.state.draft.retainedRaw.insert(
        QStringLiteral("futureSchema"),
        3
    );
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    QVERIFY(!editor.controller()->editable());
    QLabel *unavailable = editor.findChild<QLabel *>(
        QStringLiteral("flowUnavailableLabel")
    );
    QVERIFY(unavailable);
    QVERIFY(!unavailable->isHidden());
    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    QVERIFY(editor.controller()->graph().nodes.isEmpty());
    QVERIFY(editor.flushPendingSave());
    QCOMPARE(fake.draftSaves, 0);
    QCOMPARE(
        fake.state.draft.retainedRaw
            .value(QStringLiteral("futureSchema")).toInt(),
        3
    );
}

void FunctionCanvasEditorTests::
publishRepairRequiresExplicitConfirmation()
{
    FakeEditorSettings fake;
    fake.requireRepair = true;
    int confirmations = 0;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    access.confirmRepair = [&](const QString &, const QString &) {
        ++confirmations;
        return true;
    };
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    QPushButton *publish = editor.findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    );
    QVERIFY(publish);
    publish->click();
    QCOMPARE(confirmations, 1);
    QCOMPARE(fake.publishCalls, 2);
    QVERIFY(fake.lastRepair);
}

void FunctionCanvasEditorTests::
classicPublishShowsActivationInformationWithoutChangingMode()
{
    FakeEditorSettings fake;
    fake.executionMode = FunctionExecutionMode::Classic;
    fake.state.enabled = false;
    int informationCalls = 0;
    QString informationTitle;
    QString informationMessage;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    access.executionModeProvider = [&fake](const QString &) {
        return fake.executionMode;
    };
    access.showInformation = [&](
        const QString &title,
        const QString &message
    ) {
        ++informationCalls;
        informationTitle = title;
        informationMessage = message;
    };
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );

    editor.findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    )->click();

    QCOMPARE(fake.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!fake.state.enabled);
    QVERIFY(!editor.controller()->flowState().enabled);
    QCOMPARE(informationCalls, 1);
    QCOMPARE(informationTitle, QString::fromUtf8("流程已发布"));
    QCOMPARE(
        informationMessage,
        QString::fromUtf8(
            "流程已发布；切换到画布模式后生效。"
        )
    );
}

void FunctionCanvasEditorTests::
canvasPublishDoesNotShowClassicActivationInformation()
{
    FakeEditorSettings fake;
    fake.executionMode = FunctionExecutionMode::Canvas;
    fake.state.enabled = true;
    int informationCalls = 0;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.inspectorOptions = inspectorOptions();
    access.executionModeProvider = [&fake](const QString &) {
        return fake.executionMode;
    };
    access.showInformation = [&](
        const QString &,
        const QString &
    ) {
        ++informationCalls;
    };
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );

    editor.findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    )->click();

    QCOMPARE(fake.executionMode, FunctionExecutionMode::Canvas);
    QVERIFY(fake.state.enabled);
    QVERIFY(editor.controller()->flowState().enabled);
    QCOMPARE(informationCalls, 0);
}

void FunctionCanvasEditorTests::publishFailureUsesThePublishFlowTitle()
{
    FakeEditorSettings fake;
    int warningCalls = 0;
    QString warningTitle;
    FunctionCanvasEditorAccess access;
    access.flows = fake.flowAccess();
    access.flows.publish = [](
        const QString &,
        int,
        bool
    ) {
        FunctionFlowPublishResult result;
        result.error.code = QStringLiteral("test_publish_failed");
        result.error.message = QStringLiteral("publish failed");
        return result;
    };
    access.inspectorOptions = inspectorOptions();
    access.showWarning = [&](
        const QString &title,
        const QString &
    ) {
        ++warningCalls;
        warningTitle = title;
    };
    FunctionCanvasEditor editor(access);
    QVERIFY(editor.setFunctionId(
        QStringLiteral("custom_1"),
        defaults()
    ));
    editor.canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );

    editor.findChild<QPushButton *>(
        QStringLiteral("flowPublishButton")
    )->click();

    QCOMPARE(warningCalls, 1);
    QCOMPARE(warningTitle, QString::fromUtf8("发布流程失败"));
}

QTEST_MAIN(FunctionCanvasEditorTests)

#include "function_canvas_editor_tests.moc"
