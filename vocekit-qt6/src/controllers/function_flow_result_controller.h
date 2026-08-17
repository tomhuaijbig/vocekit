#ifndef VOCEKIT_FUNCTION_FLOW_RESULT_CONTROLLER_H
#define VOCEKIT_FUNCTION_FLOW_RESULT_CONTROLLER_H

#include "../domain/function_flow_compiler.h"
#include "../output/clipboard_writer.h"

#include <QMap>
#include <QObject>
#include <QPointer>

class ResultChoicePopup;
class ScreenshotResultWindow;

struct FunctionFlowResultControllerAccess
{
    std::function<bool(FunctionFlowTargetWindowHandle)>
        isUsableExternalTargetWindow;
    std::function<bool(FunctionFlowTargetWindowHandle)>
        hasCurrentSelection;
    std::function<ClipboardWriteResult(
        const QString &,
        FunctionFlowTargetWindowHandle,
        bool,
        bool
    )> writeText;
    std::function<void(
        const QString &,
        const QString &,
        const QString &
    )> addVocabulary;
    std::function<void(const QRect &)> saveResultPopupGeometry;
    std::function<void(const QRect &, int)>
        saveScreenshotPanelPreference;
    std::function<void(const QString &, const QString &)>
        showInformation;
};

struct FunctionFlowResultControllerCallbacks
{
    std::function<void(const ExecutionId &)> requestCancel;
    std::function<void(const ExecutionId &)>
        editableSurfaceOpened;
    std::function<void(const ExecutionId &, const QString &)>
        editedTextCommitted;
    std::function<void(const ExecutionId &)>
        editableSurfaceClosed;
};

QString buildFunctionFlowResultPopupText(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request
);

class FunctionFlowResultController : public QObject
{
public:
    explicit FunctionFlowResultController(
        const FunctionFlowResultControllerAccess &access,
        const FunctionFlowResultControllerCallbacks &callbacks,
        QObject *parent = nullptr
    );
    ~FunctionFlowResultController() override;

    void beginStreamingPreview(
        const FunctionFlowRunContext &run,
        const QString &modelNodeId,
        const QString &popupNodeId
    );
    void appendStreamingDelta(
        const ExecutionId &runId,
        const QString &modelNodeId,
        const QString &popupNodeId,
        const QString &delta
    );
    void abandonStreamingPreview(
        const ExecutionId &runId,
        const QString &modelNodeId,
        const QString &popupNodeId
    );
    void runAction(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowResultActionRequest &request,
        const FunctionFlowNodeCompletion &completion
    );

private:
    struct StreamingPreview
    {
        ExecutionId runId;
        QString modelNodeId;
        QString popupNodeId;
        QPointer<ResultChoicePopup> popup;
    };

    QString previewKey(
        const ExecutionId &runId,
        const QString &popupNodeId
    ) const;
    ResultChoicePopup *takeStreamingPreview(
        const ExecutionId &runId,
        const QString &popupNodeId
    );
    ClipboardWriteResult checkedPopupWrite(
        const FunctionFlowRunContext &run,
        bool collectedSelection,
        const QString &action,
        const QString &text
    ) const;
    void presentResultPopup(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowResultActionRequest &request,
        const FunctionFlowNodeCompletion &completion
    );
    void presentScreenshotPanel(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowResultActionRequest &request,
        const FunctionFlowNodeCompletion &completion
    );
    void runAutoWrite(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowResultActionRequest &request,
        const FunctionFlowNodeCompletion &completion
    );

    FunctionFlowResultControllerAccess m_access;
    FunctionFlowResultControllerCallbacks m_callbacks;
    QMap<QString, StreamingPreview> m_streamingPreviews;
};

#endif // VOCEKIT_FUNCTION_FLOW_RESULT_CONTROLLER_H
