#include "function_canvas_editor.h"

#include "function_canvas_node_item.h"
#include "function_canvas_palette.h"
#include "function_canvas_scene.h"
#include "function_canvas_view.h"
#include "ui_style.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {

QString unavailableMessage(const QString &code)
{
    if (code == QStringLiteral("flow_schema_newer")
        || code == QStringLiteral("flow_node_type_unsupported")) {
        return QString::fromUtf8(
            "较新版本的流程无法在当前版本编辑。"
        );
    }
    return QString::fromUtf8(
        "草稿数据损坏，已切换为只读模式。"
    );
}

QString safeErrorMessage(const OperationError &error)
{
    if (!error.message.trimmed().isEmpty()) {
        return error.message;
    }
    if (!error.code.trimmed().isEmpty()) {
        return QString::fromUtf8("操作失败（%1）").arg(error.code);
    }
    return QString::fromUtf8("操作失败，请重试。");
}

QToolButton *toolButton(
    const QString &text,
    const QString &objectName,
    QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setText(text);
    button->setObjectName(objectName);
    button->setMinimumHeight(34);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

FunctionCanvasEditor::FunctionCanvasEditor(
    const FunctionCanvasEditorAccess &access,
    QWidget *parent)
    : QWidget(parent),
      m_access(access),
      m_controller(new FunctionFlowEditorController(
          access.flows,
          this
      )),
      m_scene(new FunctionCanvasScene(this)),
      m_view(new FunctionCanvasView(this)),
      m_palette(new FunctionCanvasPalette(this)),
      m_inspector(new FunctionCanvasInspector(
          access.inspectorOptions,
          this
      ))
{
    setObjectName(QStringLiteral("functionCanvasEditor"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("flowCanvasToolbar"));
    auto *tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(8, 6, 8, 6);
    tools->setSpacing(6);
    m_placeButton = toolButton(
        QString::fromUtf8("放置"),
        QStringLiteral("flowPlaceButton"),
        toolbar
    );
    m_placeButton->setCheckable(true);
    QToolButton *undo = toolButton(
        QString::fromUtf8("撤销"),
        QStringLiteral("flowUndoButton"),
        toolbar
    );
    QToolButton *redo = toolButton(
        QString::fromUtf8("重做"),
        QStringLiteral("flowRedoButton"),
        toolbar
    );
    QToolButton *zoomOut = toolButton(
        QStringLiteral("−"),
        QStringLiteral("flowZoomOutButton"),
        toolbar
    );
    QToolButton *zoomIn = toolButton(
        QStringLiteral("+"),
        QStringLiteral("flowZoomInButton"),
        toolbar
    );
    QToolButton *fit = toolButton(
        QString::fromUtf8("适应窗口"),
        QStringLiteral("flowFitButton"),
        toolbar
    );
    m_zoomLabel = new QLabel(QStringLiteral("100%"), toolbar);
    m_zoomLabel->setObjectName(QStringLiteral("flowZoomLabel"));
    tools->addWidget(m_placeButton);
    tools->addSpacing(8);
    tools->addWidget(undo);
    tools->addWidget(redo);
    tools->addStretch();
    tools->addWidget(zoomOut);
    tools->addWidget(m_zoomLabel);
    tools->addWidget(zoomIn);
    tools->addWidget(fit);
    root->addWidget(toolbar);

    m_unavailable = new QLabel(this);
    m_unavailable->setObjectName(
        QStringLiteral("flowUnavailableLabel")
    );
    m_unavailable->setWordWrap(true);
    m_unavailable->setStyleSheet(QStringLiteral(
        "QLabel { background:#fff4e5; color:#92400e;"
        " border:1px solid #fbbf24; border-radius:6px;"
        " padding:8px 10px; }"
    ));
    m_unavailable->hide();
    root->addWidget(m_unavailable);

    auto *body = new QWidget(this);
    m_bodyLayout = new QHBoxLayout(body);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(8);
    m_palette->hide();
    m_bodyLayout->addWidget(m_palette);
    m_view->setCanvasScene(m_scene);
    m_view->setAcceptDrops(true);
    m_view->viewport()->setAcceptDrops(true);
    m_view->viewport()->installEventFilter(this);
    m_bodyLayout->addWidget(m_view, 1);

    m_inspectorScroll = new QScrollArea(this);
    m_inspectorScroll->setObjectName(
        QStringLiteral("flowInspectorScroll")
    );
    m_inspectorScroll->setWidgetResizable(true);
    m_inspectorScroll->setFrameShape(QFrame::NoFrame);
    m_inspectorScroll->setWidget(m_inspector);
    m_inspectorScroll->setMinimumWidth(290);
    m_inspectorScroll->setMaximumWidth(370);
    m_inspectorScroll->hide();
    m_bodyLayout->addWidget(m_inspectorScroll);
    root->addWidget(body, 1);

    auto *footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("flowCanvasFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(8, 6, 8, 6);
    footerLayout->setSpacing(10);
    m_counts = new QLabel(footer);
    m_counts->setObjectName(QStringLiteral("flowCountsLabel"));
    m_draftStatus = new QLabel(footer);
    m_draftStatus->setObjectName(
        QStringLiteral("flowDraftStatusLabel")
    );
    m_publishStatus = new QLabel(footer);
    m_publishStatus->setObjectName(
        QStringLiteral("flowPublishedStatusLabel")
    );
    m_runStatus = new QLabel(footer);
    m_runStatus->setObjectName(
        QStringLiteral("flowRunStatusLabel")
    );
    m_publishButton = new QPushButton(
        QString::fromUtf8("发布流程"),
        footer
    );
    m_publishButton->setObjectName(
        QStringLiteral("flowPublishButton")
    );
    m_publishButton->setProperty("buttonRole", "primary");
    m_publishButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#2563eb; color:white;"
        " border:1px solid #2563eb; border-radius:6px;"
        " padding:6px 14px; }"
        "QPushButton:disabled { background:#cbd5e1;"
        " border-color:#cbd5e1; color:#f8fafc; }"
    ));
    const QList<QLabel *> footerLabels = QList<QLabel *>()
        << m_counts
        << m_draftStatus
        << m_publishStatus
        << m_runStatus;
    for (QLabel *label : footerLabels) {
        label->setSizePolicy(
            QSizePolicy::Minimum,
            QSizePolicy::Preferred
        );
    }
    footerLayout->addWidget(m_counts);
    footerLayout->addWidget(m_draftStatus);
    footerLayout->addWidget(m_publishStatus);
    footerLayout->addWidget(m_runStatus);
    footerLayout->addStretch();
    footerLayout->addWidget(m_publishButton);
    root->addWidget(footer);

    connect(
        m_placeButton,
        &QToolButton::toggled,
        this,
        &FunctionCanvasEditor::setPaletteVisible
    );
    connect(
        undo,
        &QToolButton::clicked,
        m_controller->undoStack(),
        &QUndoStack::undo
    );
    connect(
        redo,
        &QToolButton::clicked,
        m_controller->undoStack(),
        &QUndoStack::redo
    );
    connect(
        m_controller->undoStack(),
        &QUndoStack::canUndoChanged,
        undo,
        &QWidget::setEnabled
    );
    connect(
        m_controller->undoStack(),
        &QUndoStack::canRedoChanged,
        redo,
        &QWidget::setEnabled
    );
    undo->setEnabled(false);
    redo->setEnabled(false);

    connect(
        zoomOut,
        &QToolButton::clicked,
        this,
        [this]() {
            FunctionFlowEditorState state;
            state.viewportCenter = m_view->viewportCenter();
            state.zoom = m_view->zoomLevel() / 1.18;
            m_view->restoreViewport(state);
            updateZoomLabel();
            m_controller->updateEditorState(state);
        }
    );
    connect(
        zoomIn,
        &QToolButton::clicked,
        this,
        [this]() {
            FunctionFlowEditorState state;
            state.viewportCenter = m_view->viewportCenter();
            state.zoom = m_view->zoomLevel() * 1.18;
            m_view->restoreViewport(state);
            updateZoomLabel();
            m_controller->updateEditorState(state);
        }
    );
    connect(
        fit,
        &QToolButton::clicked,
        this,
        [this]() {
            const QRectF bounds = m_scene->itemsBoundingRect();
            FunctionFlowEditorState state;
            state.viewportCenter = bounds.isEmpty()
                ? QPointF()
                : bounds.center();
            if (bounds.isEmpty()) {
                state.zoom = 1.0;
            } else {
                const qreal horizontal =
                    qreal(m_view->viewport()->width())
                    / qMax(qreal(1.0), bounds.width() + 120.0);
                const qreal vertical =
                    qreal(m_view->viewport()->height())
                    / qMax(qreal(1.0), bounds.height() + 120.0);
                state.zoom = qMin(horizontal, vertical);
            }
            m_view->restoreViewport(state);
            updateZoomLabel();
            m_controller->updateEditorState(state);
        }
    );
    connect(
        m_view,
        &FunctionCanvasView::viewportChanged,
        this,
        [this](const QPointF &center, qreal zoom) {
            updateZoomLabel();
            if (m_forwardingViewport) {
                return;
            }
            FunctionFlowEditorState state;
            state.viewportCenter = center;
            state.zoom = zoom;
            m_forwardingViewport = true;
            m_controller->updateEditorState(state);
            m_forwardingViewport = false;
        }
    );
    connect(
        m_palette,
        &FunctionCanvasPalette::nodeTypeChosen,
        this,
        &FunctionCanvasEditor::placeNodeAtViewportCenter
    );
    connect(
        m_scene,
        &FunctionCanvasScene::nodePlacementRequested,
        m_controller,
        [this](FunctionFlowNodeType type, const QPointF &position) {
            m_controller->placeNode(type, position);
        }
    );
    connect(
        m_scene,
        &FunctionCanvasScene::connectionRequested,
        m_controller,
        &FunctionFlowEditorController::addConnection
    );
    connect(
        m_scene,
        &FunctionCanvasScene::nodeRemovalRequested,
        m_controller,
        &FunctionFlowEditorController::removeNode
    );
    connect(
        m_scene,
        &FunctionCanvasScene::edgeRemovalRequested,
        m_controller,
        &FunctionFlowEditorController::removeEdge
    );
    connect(
        m_scene,
        &FunctionCanvasScene::positionCommitted,
        m_controller,
        &FunctionFlowEditorController::commitNodePosition
    );
    connect(
        m_scene,
        &QGraphicsScene::selectionChanged,
        this,
        &FunctionCanvasEditor::updateInspectorSelection
    );
    connect(
        m_scene,
        &FunctionCanvasScene::nodeSettingsRequested,
        this,
        &FunctionCanvasEditor::openInspectorForNode
    );
    connect(
        m_inspector,
        &FunctionCanvasInspector::nodeChanged,
        m_controller,
        &FunctionFlowEditorController::updateNode
    );
    connect(
        m_inspector,
        &FunctionCanvasInspector::outputActionOrderChanged,
        m_controller,
        &FunctionFlowEditorController::reorderOutputActions
    );
    connect(
        m_controller,
        &FunctionFlowEditorController::graphChanged,
        this,
        [this]() {
            m_scene->setGraph(m_controller->graph());
            QTimer::singleShot(
                0,
                this,
                [this]() { refreshInspectorGraph(); }
            );
            refreshStatus();
        }
    );
    connect(
        m_controller,
        &FunctionFlowEditorController::editorStateChanged,
        this,
        [this]() {
            if (!m_forwardingViewport) {
                restoreControllerViewport();
            }
        }
    );
    connect(
        m_controller,
        &FunctionFlowEditorController::analysisChanged,
        this,
        &FunctionCanvasEditor::refreshStatus
    );
    connect(
        m_controller,
        &FunctionFlowEditorController::stateChanged,
        this,
        &FunctionCanvasEditor::refreshStatus
    );
    connect(
        m_controller,
        &FunctionFlowEditorController::publicationBusyChanged,
        this,
        [this](bool) { refreshStatus(); }
    );
    connect(
        m_publishButton,
        &QPushButton::clicked,
        this,
        &FunctionCanvasEditor::publishRequested
    );
    refreshStatus();
    updateSidePanelPolicy();
}

FunctionCanvasEditor::~FunctionCanvasEditor()
{
    if (m_scene) {
        QObject::disconnect(m_scene, nullptr, this, nullptr);
    }
    if (m_view) {
        QObject::disconnect(m_view, nullptr, this, nullptr);
        m_view->setCanvasScene(nullptr);
    }
}

bool FunctionCanvasEditor::setFunctionId(
    const QString &functionId,
    const FunctionFlowPlacementDefaults &defaults)
{
    if (!m_controller->openFunction(functionId, defaults)) {
        showFailure(
            QString::fromUtf8("无法打开流程"),
            m_controller->lastError()
        );
        return false;
    }
    m_scene->setGraph(m_controller->graph());
    m_inspector->clearSelection();
    setInspectorVisible(false);
    restoreControllerViewport();
    refreshStatus();
    return true;
}

QString FunctionCanvasEditor::functionId() const
{
    return m_controller->functionId();
}

bool FunctionCanvasEditor::refreshFromAccess()
{
    const bool refreshed = m_controller->refreshFromAccess();
    refreshStatus();
    return refreshed;
}

void FunctionCanvasEditor::observeRemoteState(
    const FunctionFlowState &state)
{
    m_controller->observeRemoteState(state);
    refreshStatus();
}

bool FunctionCanvasEditor::flushPendingSave()
{
    return m_controller->flushPendingSave();
}

bool FunctionCanvasEditor::flushPendingEditorState()
{
    return m_controller->flushPendingEditorState();
}

bool FunctionCanvasEditor::flushAllPendingSaves()
{
    return m_controller->flushAllPendingSaves();
}

void FunctionCanvasEditor::discardPendingSaves()
{
    m_controller->discardPendingSaves();
}

FunctionFlowEditorController *
FunctionCanvasEditor::controller() const
{
    return m_controller;
}

FunctionCanvasScene *FunctionCanvasEditor::canvasScene() const
{
    return m_scene;
}

FunctionCanvasView *FunctionCanvasEditor::canvasView() const
{
    return m_view;
}

FunctionCanvasPalette *FunctionCanvasEditor::palette() const
{
    return m_palette;
}

FunctionCanvasInspector *FunctionCanvasEditor::inspector() const
{
    return m_inspector;
}

bool FunctionCanvasEditor::applyRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    if (event.functionId != functionId()
        || event.publishedHash
            != m_controller->flowState().published.graphHash) {
        return false;
    }
    return m_scene->applyRuntimeEvent(event);
}

bool FunctionCanvasEditor::applyRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    if (event.functionId != functionId()
        || event.publishedHash
            != m_controller->flowState().published.graphHash) {
        return false;
    }
    m_runActive = event.running;
    refreshStatus();
    return true;
}

bool FunctionCanvasEditor::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (watched == m_view->viewport() && event) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *drag =
                static_cast<QDragEnterEvent *>(event);
            if (drag->mimeData()->hasFormat(
                    functionCanvasNodeMimeType())) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *drag =
                static_cast<QDragMoveEvent *>(event);
            if (drag->mimeData()->hasFormat(
                    functionCanvasNodeMimeType())) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            QDropEvent *drop =
                static_cast<QDropEvent *>(event);
            const QString typeId = QString::fromUtf8(
                drop->mimeData()->data(
                    functionCanvasNodeMimeType()
                )
            );
            bool ok = false;
            const FunctionFlowNodeType type =
                functionFlowNodeTypeFromId(typeId, &ok);
            if (ok) {
                const QPointF center =
                    m_view->mapToScene(drop->pos());
                m_controller->placeNode(
                    type,
                    center
                        - FunctionCanvasNodeItem::boundsForType(
                            type
                        ).center()
                );
                drop->acceptProposedAction();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FunctionCanvasEditor::resizeEvent(QResizeEvent *event)
{
    if (!m_view) {
        QWidget::resizeEvent(event);
        return;
    }
    FunctionFlowEditorState authoritativeViewport;
    if (m_controller
        && !m_controller->functionId().isEmpty()) {
        authoritativeViewport = m_controller->editorState();
    } else {
        authoritativeViewport.viewportCenter =
            m_view->viewportCenter();
        authoritativeViewport.zoom = m_view->zoomLevel();
    }
    if (!m_hasPendingLayoutViewport) {
        m_forwardingBeforePendingLayout =
            m_forwardingViewport;
    }
    m_pendingLayoutViewport = authoritativeViewport;
    m_hasPendingLayoutViewport = true;
    m_forwardingViewport = true;
    QWidget::resizeEvent(event);
    updateSidePanelPolicy();
    activateEditorLayouts();
    restoreViewportAfterLayout(m_pendingLayoutViewport);
    if (!m_pendingViewportRestoreScheduled) {
        m_pendingViewportRestoreScheduled = true;
        QTimer::singleShot(
            0,
            this,
            [this]() { restorePendingViewport(); }
        );
    }
}

void FunctionCanvasEditor::updateSidePanelPolicy()
{
    if (!m_palette || !m_inspectorScroll || !m_view) {
        return;
    }
    bool showPalette = m_paletteRequestedVisible;
    bool showInspector = m_inspectorRequestedVisible;
    const int sidePanelSpacing = 16;
    const int remainingCanvasWidth = width()
        - m_palette->minimumWidth()
        - m_inspectorScroll->minimumWidth()
        - sidePanelSpacing;
    const bool compact = width() < 1000
        || remainingCanvasWidth < 460;
    if (compact && showPalette && showInspector) {
        if (m_lastActiveSidePanel == SidePanel::Palette) {
            showInspector = false;
        } else {
            showPalette = false;
        }
    }
    const bool paletteVisibilityChanged =
        m_palette->isHidden() == showPalette;
    const bool inspectorVisibilityChanged =
        m_inspectorScroll->isHidden() == showInspector;
    const bool visibilityChanged =
        paletteVisibilityChanged || inspectorVisibilityChanged;
    if (visibilityChanged) {
        FunctionFlowEditorState preservedViewport;
        preservedViewport.viewportCenter =
            m_view->viewportCenter();
        preservedViewport.zoom = m_view->zoomLevel();
        const bool wasForwardingViewport =
            m_forwardingViewport;
        m_forwardingViewport = true;
        m_palette->setVisible(showPalette);
        m_inspectorScroll->setVisible(showInspector);
        if (m_bodyLayout) {
            m_bodyLayout->invalidate();
            m_bodyLayout->activate();
        }
        restoreViewportAfterLayout(preservedViewport);
        m_forwardingViewport = wasForwardingViewport;
    }
    if (m_placeButton) {
        const QSignalBlocker blocker(m_placeButton);
        m_placeButton->setChecked(showPalette);
    }
}

void FunctionCanvasEditor::restoreViewportAfterLayout(
    const FunctionFlowEditorState &viewport)
{
    m_view->restoreViewport(viewport);
    for (int attempt = 0; attempt < 2; ++attempt) {
        const QPointF restoredCenter =
            m_view->viewportCenter();
        if (restoredCenter == viewport.viewportCenter) {
            break;
        }
        FunctionFlowEditorState correctedViewport = viewport;
        correctedViewport.viewportCenter +=
            viewport.viewportCenter - restoredCenter;
        m_view->restoreViewport(correctedViewport);
    }
}

void FunctionCanvasEditor::activateEditorLayouts()
{
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
    if (m_bodyLayout) {
        m_bodyLayout->invalidate();
        m_bodyLayout->activate();
    }
}

void FunctionCanvasEditor::restorePendingViewport()
{
    m_pendingViewportRestoreScheduled = false;
    if (!m_hasPendingLayoutViewport || !m_view) {
        return;
    }
    if (m_controller
        && !m_controller->functionId().isEmpty()) {
        m_pendingLayoutViewport =
            m_controller->editorState();
    }
    m_forwardingViewport = true;
    activateEditorLayouts();
    restoreViewportAfterLayout(m_pendingLayoutViewport);
    updateZoomLabel();
    m_hasPendingLayoutViewport = false;
    m_forwardingViewport =
        m_forwardingBeforePendingLayout;
}

void FunctionCanvasEditor::setPaletteVisible(bool visible)
{
    m_paletteRequestedVisible = visible;
    if (visible) {
        m_lastActiveSidePanel = SidePanel::Palette;
    }
    updateSidePanelPolicy();
}

void FunctionCanvasEditor::setInspectorVisible(bool visible)
{
    m_inspectorRequestedVisible = visible;
    if (visible) {
        m_lastActiveSidePanel = SidePanel::Inspector;
    }
    updateSidePanelPolicy();
}

void FunctionCanvasEditor::placeNodeAtViewportCenter(
    FunctionFlowNodeType type)
{
    const int offset = m_controller->graph().nodes.size() % 6;
    const QPointF center = m_view->viewportCenter()
        + QPointF(offset * 24.0, offset * 20.0);
    const QPointF position = center
        - FunctionCanvasNodeItem::boundsForType(type).center();
    const QString id = m_controller->placeNode(type, position);
    if (!id.isEmpty()) {
        if (FunctionCanvasNodeItem *item = m_scene->nodeItem(id)) {
            m_scene->clearSelection();
            item->setSelected(true);
        }
    }
}

void FunctionCanvasEditor::updateInspectorSelection()
{
    QString selectedId;
    const QList<QGraphicsItem *> selected =
        m_scene->selectedItems();
    for (QGraphicsItem *item : selected) {
        FunctionCanvasNodeItem *node =
            dynamic_cast<FunctionCanvasNodeItem *>(item);
        if (node) {
            selectedId = node->nodeId();
            break;
        }
    }
    if (selectedId.isEmpty()
        || selectedId != m_inspector->selectedNodeId()) {
        m_inspector->clearSelection();
        setInspectorVisible(false);
    }
}

void FunctionCanvasEditor::openInspectorForNode(
    const QString &nodeId)
{
    FunctionCanvasNodeItem *node = m_scene->nodeItem(nodeId);
    if (!node) {
        return;
    }
    if (!node->isSelected()) {
        m_scene->clearSelection();
        node->setSelected(true);
    }
    m_inspector->setGraphAndSelection(
        m_controller->graph(),
        nodeId
    );
    m_inspector->setEditable(m_controller->editable());
    setInspectorVisible(true);
}

void FunctionCanvasEditor::refreshInspectorGraph()
{
    const QString selected = m_inspector->selectedNodeId();
    if (selected.isEmpty()) {
        return;
    }
    m_inspector->setGraphAndSelection(
        m_controller->graph(),
        selected
    );
    m_inspector->setEditable(m_controller->editable());
}

void FunctionCanvasEditor::restoreControllerViewport()
{
    const bool wasForwardingViewport = m_forwardingViewport;
    m_forwardingViewport = true;
    m_view->restoreViewport(m_controller->editorState());
    m_forwardingViewport = wasForwardingViewport;
    updateZoomLabel();
}

void FunctionCanvasEditor::updateZoomLabel()
{
    if (!m_zoomLabel || !m_view) {
        return;
    }
    m_zoomLabel->setText(
        QStringLiteral("%1%")
            .arg(qRound(m_view->zoomLevel() * 100.0))
    );
}

void FunctionCanvasEditor::refreshStatus()
{
    const FunctionFlowGraph &graph = m_controller->graph();
    m_counts->setText(QString::fromUtf8("节点 %1 · 连线 %2")
        .arg(graph.nodes.size())
        .arg(graph.edges.size()));
    if (!m_controller->editable()) {
        m_draftStatus->setText(QString::fromUtf8("只读"));
        m_unavailable->setText(
            unavailableMessage(m_controller->unavailableCode())
        );
        m_unavailable->show();
    } else if (m_controller->hasRemoteConflict()) {
        m_draftStatus->setText(QString::fromUtf8("远端已更新"));
        m_unavailable->setText(QString::fromUtf8(
            "草稿已在其它位置更新。请重新加载远端草稿，"
            "或保留当前页面继续检查。"
        ));
        m_unavailable->show();
    } else {
        m_draftStatus->setText(
            m_controller->isDirty()
                ? QString::fromUtf8("未保存")
                : QString::fromUtf8("已保存")
        );
        m_unavailable->hide();
    }
    m_publishStatus->setText(
        QString::fromUtf8("版本 %1")
            .arg(m_controller->flowState().published.revision)
    );
    const FunctionFlowDraftAnalysis &analysis =
        m_controller->analysis();
    m_runStatus->setText(
        m_runActive ? QString::fromUtf8("运行中") : QString()
    );
    m_runStatus->setVisible(m_runActive);
    const bool busy = m_controller->publicationBusy();
    m_publishButton->setEnabled(
        !busy
        && m_controller->editable()
        && !m_controller->hasRemoteConflict()
        && analysis.validation.ok
    );
}

void FunctionCanvasEditor::publishRequested()
{
    FunctionFlowEditorPublishResult result =
        m_controller->publishFlow(false);
    if (result.outcome
        == FunctionFlowEditorPublishOutcome::RepairConfirmationRequired) {
        const QString title = QString::fromUtf8("修复发布流程");
        const QString message = QString::fromUtf8(
            "当前发布版本已损坏或哈希不匹配。"
            "继续会用当前草稿替换该发布版本，是否继续？"
        );
        const bool confirmed = m_access.confirmRepair
            ? m_access.confirmRepair(title, message)
            : QMessageBox::question(
                this,
                title,
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            ) == QMessageBox::Yes;
        if (!confirmed) {
            return;
        }
        result = m_controller->publishFlow(true);
    }
    if (result.outcome == FunctionFlowEditorPublishOutcome::Failed
        || result.outcome == FunctionFlowEditorPublishOutcome::Blocked) {
        showFailure(
            QString::fromUtf8("发布流程失败"),
            result.error
        );
    } else if (
        result.outcome == FunctionFlowEditorPublishOutcome::Succeeded
        && m_access.executionModeProvider
        && m_access.executionModeProvider(m_controller->functionId())
            == FunctionExecutionMode::Classic
    ) {
        const QString title = QString::fromUtf8("流程已发布");
        const QString message = QString::fromUtf8(
            "流程已发布；切换到画布模式后生效。"
        );
        if (m_access.showInformation) {
            m_access.showInformation(title, message);
        } else {
            QMessageBox::information(this, title, message);
        }
    }
    refreshStatus();
}

void FunctionCanvasEditor::showFailure(
    const QString &title,
    const OperationError &error)
{
    const QString message = safeErrorMessage(error);
    if (m_access.showWarning) {
        m_access.showWarning(title, message);
        return;
    }
    QMessageBox::warning(this, title, message);
}
