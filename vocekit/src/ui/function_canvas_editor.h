#ifndef VOCEKIT_FUNCTION_CANVAS_EDITOR_H
#define VOCEKIT_FUNCTION_CANVAS_EDITOR_H

#include "function_canvas_inspector.h"
#include "function_flow_settings_access.h"

#include "../controllers/function_flow_editor_controller.h"
#include "../domain/function_flow_runtime_types.h"

#include <QWidget>

#include <functional>

class FunctionCanvasPalette;
class FunctionCanvasScene;
class FunctionCanvasView;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QToolButton;

struct FunctionCanvasEditorAccess
{
    FunctionFlowSettingsAccess flows;
    FunctionCanvasInspectorOptions inspectorOptions;
    std::function<bool(const QString &, const QString &)> confirmRepair;
    std::function<void(const QString &, const QString &)> showWarning;
    std::function<FunctionExecutionMode(const QString &)>
        executionModeProvider;
    std::function<void(const QString &, const QString &)>
        showInformation;
};

// 完整流程编辑器：组合工具栏、节点库、画布、Inspector、状态和发布区。
class FunctionCanvasEditor : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionCanvasEditor(
        const FunctionCanvasEditorAccess &access,
        QWidget *parent = nullptr
    );
    ~FunctionCanvasEditor() override;

    bool setFunctionId(
        const QString &functionId,
        const FunctionFlowPlacementDefaults &defaults
    );
    QString functionId() const;
    bool refreshFromAccess();
    void observeRemoteState(const FunctionFlowState &state);

    bool flushPendingSave();
    bool flushPendingEditorState();
    bool flushAllPendingSaves();
    void discardPendingSaves();

    FunctionFlowEditorController *controller() const;
    FunctionCanvasScene *canvasScene() const;
    FunctionCanvasView *canvasView() const;
    FunctionCanvasPalette *palette() const;
    FunctionCanvasInspector *inspector() const;

    bool applyRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    bool applyRunEvent(
        const FunctionFlowRunExecutionEvent &event
    );

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class SidePanel
    {
        Palette,
        Inspector
    };

    void placeNodeAtViewportCenter(FunctionFlowNodeType type);
    void updateSidePanelPolicy();
    void setPaletteVisible(bool visible);
    void setInspectorVisible(bool visible);
    void restoreViewportAfterLayout(
        const FunctionFlowEditorState &viewport
    );
    void activateEditorLayouts();
    void restorePendingViewport();
    void updateInspectorSelection();
    void openInspectorForNode(const QString &nodeId);
    void refreshInspectorGraph();
    void restoreControllerViewport();
    void updateZoomLabel();
    void refreshStatus();
    void publishRequested();
    void showFailure(
        const QString &title,
        const OperationError &error
    );

    FunctionCanvasEditorAccess m_access;
    FunctionFlowEditorController *m_controller = nullptr;
    FunctionCanvasScene *m_scene = nullptr;
    FunctionCanvasView *m_view = nullptr;
    FunctionCanvasPalette *m_palette = nullptr;
    FunctionCanvasInspector *m_inspector = nullptr;
    QScrollArea *m_inspectorScroll = nullptr;
    QHBoxLayout *m_bodyLayout = nullptr;
    QToolButton *m_placeButton = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_unavailable = nullptr;
    QLabel *m_counts = nullptr;
    QLabel *m_draftStatus = nullptr;
    QLabel *m_publishStatus = nullptr;
    QLabel *m_runStatus = nullptr;
    QPushButton *m_publishButton = nullptr;
    bool m_paletteRequestedVisible = false;
    bool m_inspectorRequestedVisible = false;
    SidePanel m_lastActiveSidePanel = SidePanel::Palette;
    FunctionFlowEditorState m_pendingLayoutViewport;
    bool m_hasPendingLayoutViewport = false;
    bool m_pendingViewportRestoreScheduled = false;
    bool m_forwardingBeforePendingLayout = false;
    bool m_forwardingViewport = false;
    bool m_runActive = false;
};

#endif // VOCEKIT_FUNCTION_CANVAS_EDITOR_H
