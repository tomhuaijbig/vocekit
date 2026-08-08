#include "function_flow_result_controller.h"

#include "../capture/screenshot_result_window.h"
#include "../config/app_settings_defaults.h"
#include "../ui/result_choice_popup.h"

#include <QSharedPointer>

namespace {

OperationError resultError(
    const QString &code,
    const QString &message = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    return error;
}

FunctionFlowNodeResult successResult()
{
    FunctionFlowNodeResult result;
    result.state = FunctionFlowNodeState::Succeeded;
    return result;
}

FunctionFlowNodeResult failedResult(const QString &code)
{
    FunctionFlowNodeResult result;
    result.state = FunctionFlowNodeState::Failed;
    result.error = resultError(code);
    return result;
}

FunctionFlowNodeResult cancelledResult()
{
    FunctionFlowNodeResult result;
    result.state = FunctionFlowNodeState::Cancelled;
    return result;
}

struct EditableSurfaceGate
{
    ExecutionId runId;
    FunctionFlowResultControllerCallbacks callbacks;
    bool closed = false;

    void close()
    {
        if (closed) {
            return;
        }
        closed = true;
        if (callbacks.editableSurfaceClosed) {
            callbacks.editableSurfaceClosed(runId);
        }
    }
};

int resolvedPopupOpacity(
    const FunctionFlowRunContext &run,
    const FunctionFlowResultPopupConfig &config)
{
    if (config.opacity >= 0) {
        return config.opacity;
    }
    return run.dependencies
        ? run.dependencies->inheritedResultPopupOpacity
        : 100;
}

int resolvedScreenshotOpacity(
    const FunctionFlowRunContext &run,
    const FunctionFlowScreenshotPanelConfig &config)
{
    if (config.opacity >= 0) {
        return config.opacity;
    }
    return run.dependencies
        ? run.dependencies->inheritedScreenshotPanelOpacity
        : 92;
}

ResultPopupWindowPreferences popupPreferences(
    const FunctionFlowRunContext &run,
    const FunctionFlowResultPopupConfig &config)
{
    ResultPopupWindowPreferences preferences;
    preferences.opacityPercent =
        resolvedPopupOpacity(run, config);
    if (run.dependencies) {
        preferences.hasGeometry =
            run.dependencies->hasResultPopupGeometry;
        preferences.geometry =
            run.dependencies->resultPopupGeometry;
    }
    return preferences;
}

FunctionFlowResultPopupConfig popupConfigFor(
    const FunctionFlowRunContext &run,
    const QString &popupNodeId)
{
    if (run.dependencies
        && run.dependencies->nodeConfigs.contains(
            popupNodeId)) {
        return run.dependencies->nodeConfigs.value(
            popupNodeId
        ).popup;
    }
    return FunctionFlowResultPopupConfig();
}

QString functionTitle(const FunctionFlowRunContext &run)
{
    if (run.dependencies
        && !run.dependencies->functionTitle.trimmed().isEmpty()) {
        return run.dependencies->functionTitle.trimmed();
    }
    return run.functionId;
}

} // namespace

QString buildFunctionFlowResultPopupText(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request)
{
    const QString output = request.output.text.trimmed();
    const QString templateId = normalizeResultTemplate(
        node.config.popup.resultTemplate
    );
    if (templateId == resultTemplateCompare()) {
        return QString::fromUtf8("原文 / 输入\n")
            + (request.canonicalInput.trimmed().isEmpty()
                ? QString::fromUtf8("无")
                : request.canonicalInput.trimmed())
            + QString::fromUtf8("\n\n输出结果\n")
            + output;
    }
    if (templateId == resultTemplateDetail()) {
        QStringList lines;
        lines << QString::fromUtf8("功能：")
                + functionTitle(run);
        lines << QString();
        lines << QString::fromUtf8("输入内容：");
        lines << (request.canonicalInput.trimmed().isEmpty()
            ? QString::fromUtf8("无")
            : request.canonicalInput.trimmed());
        lines << QString();
        lines << QString::fromUtf8("输出结果：");
        lines << output;
        return lines.join(QStringLiteral("\n"));
    }
    return output;
}

FunctionFlowResultController::FunctionFlowResultController(
    const FunctionFlowResultControllerAccess &access,
    const FunctionFlowResultControllerCallbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_access(access),
      m_callbacks(callbacks)
{
}

FunctionFlowResultController::~FunctionFlowResultController()
{
    for (const StreamingPreview &preview :
         m_streamingPreviews) {
        if (preview.popup) {
            preview.popup->setCancellationCallback(nullptr);
            preview.popup->setBusy(false);
            preview.popup->close();
        }
    }
    m_streamingPreviews.clear();
}

void FunctionFlowResultController::beginStreamingPreview(
    const FunctionFlowRunContext &run,
    const QString &modelNodeId,
    const QString &popupNodeId)
{
    if (run.cancellation.isCancellationRequested()
        || popupNodeId.trimmed().isEmpty()) {
        return;
    }
    const QString key = previewKey(run.runId, popupNodeId);
    if (m_streamingPreviews.value(key).popup) {
        return;
    }
    m_streamingPreviews.remove(key);

    const FunctionFlowResultPopupConfig config =
        popupConfigFor(run, popupNodeId);
    auto *popup = new ResultChoicePopup(
        popupPreferences(run, config),
        functionTitle(run),
        QString(),
        run.targetWindow,
        false,
        0
    );
    popup->setActionOrder(config.resultActions);
    popup->setBusy(true, QString::fromUtf8("正在生成"));
    const FunctionFlowResultControllerCallbacks callbacks =
        m_callbacks;
    const ExecutionId runId = run.runId;
    popup->setCancellationCallback(
        [callbacks, runId]() {
            if (callbacks.requestCancel) {
                callbacks.requestCancel(runId);
            }
        }
    );

    StreamingPreview preview;
    preview.runId = run.runId;
    preview.modelNodeId = modelNodeId;
    preview.popupNodeId = popupNodeId;
    preview.popup = popup;
    m_streamingPreviews.insert(key, preview);
    connect(
        popup,
        &QObject::destroyed,
        this,
        [this, key]() {
            m_streamingPreviews.remove(key);
        }
    );
    popup->showNearBottom();
}

void FunctionFlowResultController::appendStreamingDelta(
    const ExecutionId &runId,
    const QString &modelNodeId,
    const QString &popupNodeId,
    const QString &delta)
{
    const StreamingPreview preview =
        m_streamingPreviews.value(
            previewKey(runId, popupNodeId)
        );
    if (!preview.popup
        || preview.runId != runId
        || preview.modelNodeId != modelNodeId
        || preview.popupNodeId != popupNodeId) {
        return;
    }
    preview.popup->appendResultText(delta);
}

void FunctionFlowResultController::abandonStreamingPreview(
    const ExecutionId &runId,
    const QString &modelNodeId,
    const QString &popupNodeId)
{
    const QString key = previewKey(runId, popupNodeId);
    const StreamingPreview preview =
        m_streamingPreviews.take(key);
    if (!preview.popup
        || preview.runId != runId
        || preview.modelNodeId != modelNodeId
        || preview.popupNodeId != popupNodeId) {
        return;
    }
    preview.popup->setCancellationCallback(nullptr);
    preview.popup->setBusy(false, QString::fromUtf8("已取消"));
    preview.popup->close();
}

void FunctionFlowResultController::runAction(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request,
    const FunctionFlowNodeCompletion &completion)
{
    if (!completion) {
        return;
    }
    if (run.cancellation.isCancellationRequested()) {
        completion(cancelledResult());
        return;
    }
    if (node.type == FunctionFlowNodeType::ResultPopup) {
        presentResultPopup(run, node, request, completion);
        return;
    }
    if (node.type == FunctionFlowNodeType::ScreenshotPanel) {
        presentScreenshotPanel(run, node, request, completion);
        return;
    }
    if (node.type == FunctionFlowNodeType::AutoWrite) {
        runAutoWrite(run, node, request, completion);
        return;
    }
    completion(failedResult(
        QStringLiteral("flow_result_action_failed")
    ));
}

QString FunctionFlowResultController::previewKey(
    const ExecutionId &runId,
    const QString &popupNodeId) const
{
    return runId.value
        + QChar(0x1f)
        + popupNodeId;
}

ResultChoicePopup *
FunctionFlowResultController::takeStreamingPreview(
    const ExecutionId &runId,
    const QString &popupNodeId)
{
    const QString key = previewKey(runId, popupNodeId);
    return m_streamingPreviews.take(key).popup.data();
}

ClipboardWriteResult
FunctionFlowResultController::checkedPopupWrite(
    const FunctionFlowRunContext &run,
    bool collectedSelection,
    const QString &action,
    const QString &text) const
{
    ClipboardWriteResult failed;
    if (!m_access.isUsableExternalTargetWindow
        || !m_access.isUsableExternalTargetWindow(
            run.targetWindow)) {
        failed.errorCode =
            QStringLiteral("flow_target_window_unavailable");
        return failed;
    }

    const bool replace =
        action == QStringLiteral("replace");
    bool currentSelection = false;
    if (collectedSelection
        && m_access.hasCurrentSelection) {
        currentSelection =
            m_access.hasCurrentSelection(run.targetWindow);
    }
    if (replace
        && (!collectedSelection || !currentSelection)) {
        failed.errorCode = QStringLiteral(
            "flow_replace_selection_unavailable"
        );
        return failed;
    }
    if (!m_access.writeText) {
        failed.errorCode =
            QStringLiteral("flow_auto_write_failed");
        return failed;
    }
    ClipboardWriteResult result = m_access.writeText(
        text,
        run.targetWindow,
        replace,
        !replace && currentSelection
    );
    if (!result.ok && result.errorCode.trimmed().isEmpty()) {
        result.errorCode =
            QStringLiteral("flow_auto_write_failed");
    }
    return result;
}

void FunctionFlowResultController::presentResultPopup(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request,
    const FunctionFlowNodeCompletion &completion)
{
    const FunctionFlowResultPopupConfig &config =
        node.config.popup;
    ResultChoicePopup *popup =
        takeStreamingPreview(run.runId, node.nodeId);
    const bool reusedPreview = popup != nullptr;
    if (!popup) {
        popup = new ResultChoicePopup(
            popupPreferences(run, config),
            functionTitle(run),
            QString(),
            run.targetWindow,
            request.collectedSelection,
            0
        );
    }

    popup->setCancellationCallback(nullptr);
    popup->setActionCallbacks(nullptr, nullptr, nullptr);
    popup->setActionOrder(config.resultActions);
    popup->setHasSelection(request.collectedSelection);
    popup->setResultText(
        buildFunctionFlowResultPopupText(run, node, request)
    );
    popup->setBusy(false, QString::fromUtf8("已生成"));
    popup->setAutoCloseMsec(config.displaySeconds * 1000);

    const FunctionFlowResultControllerAccess access = m_access;
    const FunctionFlowResultControllerCallbacks callbacks =
        m_callbacks;
    const ExecutionId runId = run.runId;
    const QString flowFunctionId = run.functionId;
    const bool collectedSelection =
        request.collectedSelection;
    QPointer<FunctionFlowResultController> guard(this);
    popup->setCheckedWriteCallback(
        [guard,
         run,
         collectedSelection](
            const QString &action,
            const QString &text,
            ClipboardWindowHandle,
            bool) {
            if (!guard) {
                ClipboardWriteResult result;
                result.errorCode =
                    QStringLiteral("flow_auto_write_failed");
                return result;
            }
            return guard->checkedPopupWrite(
                run,
                collectedSelection,
                action,
                text
            );
        }
    );
    popup->setVocabularyCallback(
        [access, runId, flowFunctionId](
            const QString &before,
            const QString &after) {
            Q_UNUSED(runId);
            if (access.addVocabulary) {
                access.addVocabulary(
                    before,
                    flowFunctionId,
                    after
                );
            }
        }
    );
    popup->setWindowPreferenceCallback(
        [access](const QRect &geometry) {
            if (access.saveResultPopupGeometry) {
                access.saveResultPopupGeometry(geometry);
            }
        }
    );

    const QSharedPointer<EditableSurfaceGate> surface(
        new EditableSurfaceGate
    );
    surface->runId = runId;
    surface->callbacks = callbacks;
    if (callbacks.editableSurfaceOpened) {
        callbacks.editableSurfaceOpened(runId);
    }
    popup->setDraftCallback(
        [callbacks, runId](const QString &editedText) {
            if (callbacks.editedTextCommitted) {
                callbacks.editedTextCommitted(
                    runId,
                    editedText
                );
            }
        }
    );
    popup->setResolvedCallback([surface](const QString &) {
        surface->close();
    });
    QObject::connect(
        popup,
        &QObject::destroyed,
        [surface]() {
            surface->close();
        }
    );

    if (!reusedPreview) {
        popup->showNearBottom();
    }
    completion(successResult());
}

void FunctionFlowResultController::presentScreenshotPanel(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request,
    const FunctionFlowNodeCompletion &completion)
{
    const QSharedPointer<const FunctionFlowScreenshotPayload>
        screenshot = request.output.screenshot;
    if (screenshot.isNull()) {
        completion(failedResult(
            QStringLiteral("flow_screenshot_context_missing")
        ));
        return;
    }

    const FunctionFlowScreenshotPanelConfig &config =
        node.config.screenshotPanel;
    const QRect geometry =
        run.dependencies
        && run.dependencies->hasScreenshotPanelGeometry
            ? run.dependencies->screenshotPanelGeometry
            : QRect();
    auto *window = new ScreenshotResultWindow(
        functionTitle(run),
        screenshot->image,
        screenshot->blocks,
        screenshot->recognizedText,
        request.output.text,
        geometry,
        resolvedScreenshotOpacity(run, config),
        qMax(0, config.displaySeconds) * 1000
    );
    window->setActionCallbacks(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    const FunctionFlowResultControllerAccess access = m_access;
    const FunctionFlowResultControllerCallbacks callbacks =
        m_callbacks;
    const ExecutionId runId = run.runId;
    const bool collectedSelection =
        request.collectedSelection;
    QPointer<FunctionFlowResultController> guard(this);
    window->setCheckedWriteCallback(
        [guard, run, collectedSelection](
            const QString &action,
            const QString &text) {
            if (!guard) {
                ClipboardWriteResult result;
                result.errorCode =
                    QStringLiteral("flow_auto_write_failed");
                return result;
            }
            return guard->checkedPopupWrite(
                run,
                collectedSelection,
                action,
                text
            );
        }
    );
    window->setWindowPreferenceCallback(
        [access](const QRect &rect, int opacity) {
            if (access.saveScreenshotPanelPreference) {
                access.saveScreenshotPanelPreference(
                    rect,
                    opacity
                );
            }
        }
    );

    const QSharedPointer<EditableSurfaceGate> surface(
        new EditableSurfaceGate
    );
    surface->runId = runId;
    surface->callbacks = callbacks;
    if (callbacks.editableSurfaceOpened) {
        callbacks.editableSurfaceOpened(runId);
    }
    window->setDraftCallback(
        [callbacks, runId](const QString &editedText) {
            if (callbacks.editedTextCommitted) {
                callbacks.editedTextCommitted(
                    runId,
                    editedText
                );
            }
        }
    );
    window->setLiveDraftCallback(nullptr);
    window->setResolvedCallback([surface]() {
        surface->close();
    });
    QObject::connect(
        window,
        &QObject::destroyed,
        [surface]() {
            surface->close();
        }
    );

    window->showNearBottom();
    completion(successResult());
}

void FunctionFlowResultController::runAutoWrite(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowResultActionRequest &request,
    const FunctionFlowNodeCompletion &completion)
{
    const bool replace =
        node.config.autoWrite.writeMode
            == QStringLiteral("replace");
    ClipboardWriteResult write = checkedPopupWrite(
        run,
        request.collectedSelection,
        replace
            ? QStringLiteral("replace")
            : QStringLiteral("write"),
        request.output.text
    );
    if (write.ok) {
        completion(successResult());
        return;
    }

    if (node.config.autoWrite.fallbackToPopup
        && !node.autoWriteFallbackCoveredByExplicitPopup) {
        FunctionFlowCompiledNode fallbackNode;
        fallbackNode.nodeId =
            node.nodeId + QStringLiteral(":fallback");
        fallbackNode.type = FunctionFlowNodeType::ResultPopup;
        fallbackNode.config.popup.resultTemplate =
            resultTemplateOutputOnly();
        fallbackNode.config.popup.resultActions =
            defaultFunctionFlowPopupActionIds();

        FunctionFlowResultActionRequest fallbackRequest;
        fallbackRequest.output.text = request.output.text;
        fallbackRequest.canonicalInput = request.canonicalInput;
        fallbackRequest.collectedSelection =
            request.collectedSelection;
        presentResultPopup(
            run,
            fallbackNode,
            fallbackRequest,
            [](const FunctionFlowNodeResult &) {}
        );
    }

    FunctionFlowNodeResult failed = failedResult(
        write.errorCode.trimmed().isEmpty()
            ? QStringLiteral("flow_auto_write_failed")
            : write.errorCode
    );
    completion(failed);
}
