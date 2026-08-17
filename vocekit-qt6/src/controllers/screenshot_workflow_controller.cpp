#include "screenshot_workflow_controller.h"

#include "../capture/screen_capture_overlay.h"
#include "../config/app_settings_data.h"
#include "../config/secret_config.h"
#include "../domain/function_catalog.h"
#include "../domain/history_types.h"
#include "../domain/history_record_builder.h"
#include "../domain/vocabulary_runtime.h"
#include "../domain/voice_screenshot_session.h"
#include "../ocr/ocr_manager.h"
#include "../ocr/screenshot_ocr_config.h"
#include "../runtime_log.h"
#include "../tasks/screenshot_text_action_plan.h"
#include "../tasks/screenshot_text_action_task.h"
#include "../ui/attention_message.h"
#include "../ui/floating_bar.h"

#include <QtConcurrent>
#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

OperationError flowScreenshotError(const QString &message)
{
    OperationError error;
    error.code = QStringLiteral("flow_screenshot_failed");
    error.message = message;
    return error;
}

bool flowOcrEngineFromId(const QString &engineId, OcrEngine *engine)
{
    if (!engine) {
        return false;
    }
    const QString id = engineId.trimmed();
    if (id == QStringLiteral("automatic")) {
        *engine = OcrEngine::Automatic;
        return true;
    }
    if (id == QStringLiteral("rapid")) {
        *engine = OcrEngine::RapidOcr;
        return true;
    }
    if (id == QStringLiteral("windows")) {
        *engine = OcrEngine::WindowsOcr;
        return true;
    }
    if (id == QStringLiteral("customCloud")) {
        *engine = OcrEngine::CustomCloud;
        return true;
    }
    if (id == QStringLiteral("vision")) {
        *engine = OcrEngine::VisionModel;
        return true;
    }
    return false;
}

bool flowOcrUploadsImage(OcrEngine engine)
{
    return engine == OcrEngine::CustomCloud
        || engine == OcrEngine::VisionModel;
}

void removeFlowTemporaryFile(const QString &path)
{
    if (path.isEmpty()
        || !QFileInfo::exists(path)
        || QFile::remove(path)) {
        return;
    }

    QCoreApplication *application =
        QCoreApplication::instance();
    if (!application) {
        return;
    }
    QTimer *retryTimer = new QTimer(application);
    retryTimer->setInterval(25);
    QSharedPointer<int> attempts(new int(0));
    QObject::connect(
        retryTimer,
        &QTimer::timeout,
        application,
        [retryTimer, attempts, path]() {
            ++(*attempts);
            if (!QFileInfo::exists(path)
                || QFile::remove(path)
                || *attempts >= 200) {
                retryTimer->stop();
                retryTimer->deleteLater();
            }
        }
    );
    retryTimer->start();
}

struct ScreenshotFlowState
{
    bool active = false;
    bool capturePending = false;
    bool recognitionStarted = false;
    bool injectedCapture = false;
    bool injectedOcr = false;
    int generation = 0;
    FunctionFlowRunContext run;
    FunctionFlowCompiledNode node;
    FunctionFlowNodeCompletion completion;
    AppSettingsData settingsSnapshot;
    OcrEngine engine = OcrEngine::Automatic;
    int timeoutMs = 45000;
    QString effectiveNetworkPolicy;
    QImage capturedImage;
    QRect capturedRect;
    QString temporaryPath;
};

} // namespace

class ScreenshotWorkflowController::Impl : public QObject
{
public:
    Impl(
        const ScreenshotWorkflowAccess &access,
        FloatingBar *bar,
        QObject *parent
    )
        : QObject(parent),
          m_access(access),
          m_bar(bar),
          m_ocrManager(new OcrManager(this)),
          m_flowCancellationTimer(new QTimer(this)),
          m_flowTimeoutTimer(new QTimer(this))
    {
        m_ocrManager->statusCallback = [this](const QString &status) {
            if (m_session.isActive()) {
                setTimedStatus(status, functionTitle(m_modeId));
            }
        };
        m_ocrManager->finishedCallback = [this](const OcrResult &result) {
            handleOcrFinished(result);
        };
        m_flowCancellationTimer->setInterval(10);
        connect(
            m_flowCancellationTimer,
            &QTimer::timeout,
            this,
            [this]() {
                if (m_flow.active
                    && m_flow.run.cancellation
                        .isCancellationRequested()) {
                    completeFlowCancelled(true);
                }
            }
        );
        m_flowTimeoutTimer->setSingleShot(true);
        connect(
            m_flowTimeoutTimer,
            &QTimer::timeout,
            this,
            [this]() {
                if (!m_flow.active
                    || !m_flow.recognitionStarted) {
                    return;
                }
                FunctionFlowNodeResult result;
                result.state = FunctionFlowNodeState::Failed;
                result.error = flowScreenshotError(
                    tr8("截图识别超时。")
                );
                appendFlowHistoryObservation(&result, nullptr);
                completeFlow(
                    result,
                    true,
                    tr8("识别超时"),
                    QStringLiteral("错误码=TIMEOUT")
                );
            }
        );
    }

    ~Impl() override
    {
        if (m_flow.active) {
            completeFlowCancelled(true);
        }
        m_aiCancellation.cancel();
        if (m_ocrManager && m_ocrManager->isBusy()) {
            m_ocrManager->cancel();
        }
        const QString temporaryPath = m_session.takeTemporaryPath();
        if (!temporaryPath.isEmpty()) {
            QFile::remove(temporaryPath);
        }
        closeOverlay();
    }

    void updateConfiguration(
        const AppSettingsData &settings,
        const SecretConfig &secrets)
    {
        m_settings = settings;
        m_ocrManager->setConfig(
            buildScreenshotOcrManagerConfig(settings, secrets)
        );
    }

    bool start(const ScreenshotWorkflowStartRequest &request)
    {
        const QString functionId = request.functionId.trimmed();
        if (!useScreenshotFor(functionId)) {
            showAttentionInformation(
                hostWidget(),
                tr8("截图输入未启用"),
                tr8("请先在“功能自定义”中为这个功能启用截图输入。")
            );
            return false;
        }
        if (request.externalBusy || isBusy() || m_session.isActive()) {
            if (m_bar) {
                m_bar->setStatus(
                    tr8("当前任务尚未结束"),
                    tr8("请等待录音、识别或模型处理完成后再截图。")
                );
            }
            return false;
        }

        if (m_access.prepareRun) {
            m_access.prepareRun(
                functionId,
                request.targetAlreadyRemembered
            );
        }
        m_modeId = functionId;
        m_correctedText.clear();
        const QString staleTemporaryPath = m_session.takeTemporaryPath();
        if (!staleTemporaryPath.isEmpty()) {
            QFile::remove(staleTemporaryPath);
        }
        m_session.beginWorkflow();

        const FunctionSettings &function = m_settings.function(functionId);
        const int floatingBarSeconds = function.output.floatingBarSeconds;
        if (m_bar) {
            m_bar->setEnabledVisible(m_settings.floatingBarEnabled);
            m_bar->setSuppressed(floatingBarSeconds <= 0);
            m_bar->setAutoHideMsec(floatingBarSeconds * 1000);
            m_bar->setStatus(
                tr8("请选择截图区域"),
                functionTitle(functionId)
            );
        }
        setProcessing(true);

        auto *overlay = new ScreenCaptureOverlay;
        m_overlay = overlay;
        overlay->setFunctionActionTitle(functionTitle(functionId));
        overlay->capturedCallback =
            [this](const QImage &image, const QRect &globalRect) {
                handleCaptured(image, globalRect);
            };
        overlay->actionRequestedCallback = [this](const QString &action) {
            handleWorkbenchAction(action);
        };
        overlay->cancelledCallback = [this]() {
            const QString cancelledFunctionId = m_modeId;
            logRuntimeEvent(
                tr8("截图输入"),
                tr8("取消"),
                QStringLiteral("功能=") + cancelledFunctionId,
                elapsedMs()
            );
            if (m_bar) {
                m_bar->setStatus(
                    tr8("已取消截图"),
                    functionTitle(m_modeId)
                );
                m_bar->hideLater();
            }
            reset(false);
            if (m_access.cancelled) {
                m_access.cancelled(cancelledFunctionId);
            }
        };

        QString error;
        if (!overlay->beginCapture(&error)) {
            overlay->deleteLater();
            reset(false);
            showFailure(
                tr8("无法开始截图：")
                + (error.trimmed().isEmpty()
                    ? tr8("无法读取桌面画面。")
                    : error)
            );
            return false;
        }
        logRuntimeEvent(
            tr8("截图输入"),
            tr8("开始选区"),
            QStringLiteral("功能=") + functionId
        );
        return true;
    }

    bool beginForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion)
    {
        if (!completion) {
            return false;
        }

        if (isBusy() || m_session.isActive() || m_overlay) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("当前截图任务尚未结束。")
            );
            completion(result);
            return false;
        }
        if (run.cancellation.isCancellationRequested()) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Cancelled;
            completion(result);
            return false;
        }
        if (node.type != FunctionFlowNodeType::ScreenshotSource
            || node.nodeId.trimmed().isEmpty()
            || !run.dependencies
            || !run.dependencies->byNodeId.contains(node.nodeId)) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("截图流程配置不完整。")
            );
            completion(result);
            return false;
        }

        const FunctionFlowResolvedNodeSettings resolved =
            run.dependencies->byNodeId.value(node.nodeId);
        OcrEngine engine = OcrEngine::Automatic;
        if (!flowOcrEngineFromId(resolved.ocrEngineId, &engine)
            || node.config.screenshot.timeoutMs <= 0
            || (resolved.effectiveNetworkPolicy
                    != QStringLiteral("direct")
                && resolved.effectiveNetworkPolicy
                    != QStringLiteral("systemProxy"))) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("截图 OCR 配置无效。")
            );
            completion(result);
            return false;
        }

        m_flow = ScreenshotFlowState();
        m_flow.active = true;
        m_flow.capturePending = true;
        m_flow.generation = ++m_nextFlowGeneration;
        m_flow.run = run;
        m_flow.node = node;
        m_flow.completion = completion;
        m_flow.settingsSnapshot = m_settings;
        m_flow.engine = engine;
        m_flow.timeoutMs = node.config.screenshot.timeoutMs;
        m_flow.effectiveNetworkPolicy =
            resolved.effectiveNetworkPolicy;
        m_flowOcrConfig = m_ocrManager
            ? m_ocrManager->config()
            : OcrManagerConfig();
        m_flowOcrConfig.timeoutMs = m_flow.timeoutMs;
        m_flowOcrConfig.customCloud.timeoutMs =
            m_flow.timeoutMs;
        m_flowOcrConfig.customCloud.useSystemProxy =
            m_flow.effectiveNetworkPolicy
                == QStringLiteral("systemProxy");
        m_flowOcrConfig.customCloud.networkPolicy =
            m_flow.effectiveNetworkPolicy;

        m_flowCancellationTimer->start();
        setProcessing(true);
        logFlow(
            tr8("开始选区"),
            QStringLiteral("节点=") + node.nodeId
                + QStringLiteral("，引擎=")
                + resolved.ocrEngineId
        );

        const int generation = m_flow.generation;
        const QPointer<Impl> guard(this);
        const ScreenshotWorkflowCapturedCallback captured =
            [guard, generation](
                const QImage &image,
                const QRect &rect) {
                if (guard) {
                    guard->handleFlowCaptured(
                        generation,
                        image,
                        rect
                    );
                }
            };
        const ScreenshotWorkflowCancelledCallback cancelled =
            [guard, generation]() {
                if (guard) {
                    guard->handleFlowCaptureCancelled(generation);
                }
            };

        QString captureError;
        bool started = false;
        if (m_access.beginFlowCapture) {
            m_flow.injectedCapture = true;
            started = m_access.beginFlowCapture(
                captured,
                cancelled,
                &captureError
            );
        } else {
            auto *overlay = new ScreenCaptureOverlay;
            m_overlay = overlay;
            if (run.dependencies) {
                overlay->setFunctionActionTitle(
                    run.dependencies->functionTitle
                );
            }
            overlay->capturedCallback =
                [guard, generation](
                    const QImage &image,
                    const QRect &rect) {
                    if (!guard) {
                        return;
                    }
                    QTimer::singleShot(
                        0,
                        guard.data(),
                        [guard, generation, image, rect]() {
                            if (guard) {
                                guard->handleFlowCaptured(
                                    generation,
                                    image,
                                    rect
                                );
                            }
                        }
                    );
                };
            overlay->actionRequestedCallback = nullptr;
            overlay->cancelledCallback =
                [guard, generation]() {
                    if (!guard) {
                        return;
                    }
                    QTimer::singleShot(
                        0,
                        guard.data(),
                        [guard, generation]() {
                            if (guard) {
                                guard->handleFlowCaptureCancelled(
                                    generation
                                );
                            }
                        }
                    );
                };
            started = overlay->beginCapture(&captureError);
            if (!started) {
                overlay->deleteLater();
                m_overlay.clear();
            }
        }

        if (!started
            && m_flow.active
            && m_flow.generation == generation) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("无法开始截图。")
            );
            completeFlow(
                result,
                false,
                tr8("截图失败"),
                QStringLiteral("阶段=capture")
            );
        }
        return started;
    }

    void reset(bool keepPendingContext)
    {
        if (m_flow.active) {
            completeFlowCancelled(true);
        }
        if (m_aiWatcher) {
            m_aiCancellation.cancel();
        }
        if (m_ocrManager && m_ocrManager->isBusy()) {
            m_ocrManager->cancel();
        }
        if (!m_session.temporaryPath().isEmpty()
            && (!m_ocrManager || !m_ocrManager->isBusy())) {
            QFile::remove(m_session.takeTemporaryPath());
        }
        m_session.reset(keepPendingContext);
        if (!keepPendingContext) {
            m_correctedText.clear();
        }
        closeOverlay();
        setProcessing(false);
    }

    bool isActive() const
    {
        return m_session.isActive() || m_flow.active;
    }

    bool isBusy() const
    {
        return m_processing
            || m_flow.active
            || m_flowOcrManager
            || (m_ocrManager && m_ocrManager->isBusy())
            || m_aiWatcher;
    }

    bool applyPendingContext(VoiceRunContext *context)
    {
        return m_session.applyPendingContext(context);
    }

private:
    bool matchesFlow(int generation) const
    {
        return m_flow.active
            && m_flow.generation == generation;
    }

    void logFlow(
        const QString &action,
        const QString &detail = QString()) const
    {
        if (m_access.flowLog) {
            m_access.flowLog(action, detail);
            return;
        }
        logRuntimeEvent(
            tr8("流程截图"),
            action,
            detail,
            elapsedMs()
        );
    }

    QSharedPointer<const FunctionFlowScreenshotPayload>
    flowPayload(
        const OcrResult *ocrResult,
        const QString &recognizedText) const
    {
        FunctionFlowScreenshotPayload *payload =
            new FunctionFlowScreenshotPayload;
        payload->image = m_flow.capturedImage;
        payload->recognizedText = recognizedText;
        payload->engine = ocrResult
            ? ocrResult->engine
            : m_flow.engine;
        payload->elapsedMs = ocrResult
            ? ocrResult->elapsedMs
            : -1;
        payload->usedFallback = ocrResult
            && ocrResult->usedFallback;
        payload->rect = m_flow.capturedRect;
        if (ocrResult) {
            payload->blocks = ocrResult->blocks;
        }
        return QSharedPointer<
            const FunctionFlowScreenshotPayload
        >(payload);
    }

    void appendFlowHistoryObservation(
        FunctionFlowNodeResult *result,
        const OcrResult *ocrResult) const
    {
        if (!result
            || m_flow.capturedImage.isNull()
            || !m_flow.recognitionStarted) {
            return;
        }
        const QString recognizedText =
            ocrResult ? ocrResult->text : QString();
        FunctionFlowValue value;
        value.text = recognizedText;
        value.sourceNodeId = m_flow.node.nodeId;
        value.screenshot = flowPayload(
            ocrResult,
            recognizedText
        );
        result->historyObservations.append(value);
    }

    void completeFlow(
        const FunctionFlowNodeResult &result,
        bool cancelOutstanding,
        const QString &logAction,
        const QString &logDetail)
    {
        if (!m_flow.active) {
            return;
        }

        const FunctionFlowNodeCompletion completion =
            m_flow.completion;
        const bool cancelInjectedCapture =
            cancelOutstanding
            && m_flow.capturePending
            && m_flow.injectedCapture
            && bool(m_access.cancelFlowCapture);
        const bool cancelInjectedOcr =
            cancelOutstanding
            && m_flow.recognitionStarted
            && m_flow.injectedOcr
            && bool(m_access.cancelFlowOcr);
        const QString temporaryPath = m_flow.temporaryPath;
        m_flow.active = false;
        m_flow.completion = FunctionFlowNodeCompletion();
        m_flowCancellationTimer->stop();
        m_flowTimeoutTimer->stop();

        if (cancelInjectedCapture) {
            m_access.cancelFlowCapture();
        }
        if (cancelInjectedOcr) {
            m_access.cancelFlowOcr();
        }

        OcrManager *flowManager = m_flowOcrManager.data();
        m_flowOcrManager.clear();
        if (flowManager) {
            flowManager->statusCallback = nullptr;
            flowManager->finishedCallback = nullptr;
            if (cancelOutstanding && flowManager->isBusy()) {
                flowManager->cancel();
            }
            flowManager->deleteLater();
        }
        m_flowOcrConfig = OcrManagerConfig();

        closeOverlay();
        if (!temporaryPath.isEmpty()) {
            removeFlowTemporaryFile(temporaryPath);
        }
        m_flow = ScreenshotFlowState();
        setProcessing(false);
        logFlow(logAction, logDetail);
        if (completion) {
            completion(result);
        }
    }

    void completeFlowCancelled(bool cancelOutstanding)
    {
        FunctionFlowNodeResult result;
        result.state = FunctionFlowNodeState::Cancelled;
        appendFlowHistoryObservation(&result, nullptr);
        completeFlow(
            result,
            cancelOutstanding,
            tr8("取消"),
            QStringLiteral("状态=cancelled")
        );
    }

    void handleFlowCaptureCancelled(int generation)
    {
        if (!matchesFlow(generation)) {
            return;
        }
        completeFlowCancelled(false);
    }

    void handleFlowCaptured(
        int generation,
        const QImage &image,
        const QRect &globalRect)
    {
        if (!matchesFlow(generation)
            || !m_flow.capturePending) {
            return;
        }
        m_flow.capturePending = false;
        closeOverlay();

        if (m_flow.run.cancellation
                .isCancellationRequested()) {
            completeFlowCancelled(true);
            return;
        }
        if (image.isNull()) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("截图内容为空。")
            );
            completeFlow(
                result,
                false,
                tr8("截图失败"),
                QStringLiteral("阶段=image")
            );
            return;
        }

        m_flow.capturedImage = image;
        m_flow.capturedRect = globalRect;
        if (flowOcrUploadsImage(m_flow.engine)) {
            bool authorized = false;
            if (m_access.authorizeFlowOcrUpload) {
                authorized =
                    m_access.authorizeFlowOcrUpload(m_flow.engine);
            } else {
                const QMessageBox::StandardButton choice =
                    QMessageBox::question(
                        hostWidget(),
                        tr8("发送截图到云端"),
                        tr8("这次识别会把当前截图发送到云端。是否继续？"),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No
                    );
                authorized = choice == QMessageBox::Yes;
            }
            if (!authorized) {
                completeFlowCancelled(false);
                return;
            }
        }

        if (m_flow.run.cancellation
                .isCancellationRequested()) {
            completeFlowCancelled(true);
            return;
        }

        QString tempRoot;
        if (m_access.flowTemporaryDirectory) {
            tempRoot = m_access.flowTemporaryDirectory();
        }
        if (tempRoot.trimmed().isEmpty()) {
            tempRoot = QDir(
                QStandardPaths::writableLocation(
                    QStandardPaths::TempLocation
                )
            ).filePath(QStringLiteral("vocekit-screenshots"));
        }
        if (!QDir().mkpath(tempRoot)) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("无法准备截图临时文件。")
            );
            completeFlow(
                result,
                false,
                tr8("截图失败"),
                QStringLiteral("阶段=temp-directory")
            );
            return;
        }

        m_flow.temporaryPath = QDir(tempRoot).filePath(
            QUuid::createUuid().toString().mid(1, 36)
                + QStringLiteral(".png")
        );
        if (!image.save(m_flow.temporaryPath, "PNG")) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("无法准备截图临时文件。")
            );
            completeFlow(
                result,
                false,
                tr8("截图失败"),
                QStringLiteral("阶段=temp-save")
            );
            return;
        }

        if (m_flow.run.cancellation
                .isCancellationRequested()) {
            completeFlowCancelled(true);
            return;
        }

        ScreenshotWorkflowFlowOcrRequest flowRequest;
        flowRequest.request.requestId =
            QUuid::createUuid().toString();
        flowRequest.request.imagePath =
            QDir::toNativeSeparators(m_flow.temporaryPath);
        flowRequest.request.languages = QStringList()
            << QStringLiteral("zh-Hans")
            << QStringLiteral("en");
        flowRequest.request.engine = m_flow.engine;
        flowRequest.timeoutMs = m_flow.timeoutMs;
        flowRequest.effectiveNetworkPolicy =
            m_flow.effectiveNetworkPolicy;
        flowRequest.cancellation = m_flow.run.cancellation;

        m_flow.recognitionStarted = true;
        logFlow(
            tr8("开始识别"),
            QStringLiteral("节点=") + m_flow.node.nodeId
                + QStringLiteral("，引擎=")
                + QString::number(int(m_flow.engine))
        );
        m_flowTimeoutTimer->start(m_flow.timeoutMs);

        const QPointer<Impl> guard(this);
        const ScreenshotWorkflowOcrFinishedCallback finished =
            [guard, generation](const OcrResult &result) {
                if (guard) {
                    guard->handleFlowOcrFinished(
                        generation,
                        result
                    );
                }
            };
        if (m_access.recognizeForFlow) {
            m_flow.injectedOcr = true;
            m_access.recognizeForFlow(flowRequest, finished);
            return;
        }

        OcrManager *manager = new OcrManager(this);
        m_flowOcrManager = manager;
        manager->setConfig(m_flowOcrConfig);
        manager->statusCallback = nullptr;
        manager->finishedCallback =
            [guard, generation](const OcrResult &result) {
                if (!guard) {
                    return;
                }
                QTimer::singleShot(
                    0,
                    guard.data(),
                    [guard, generation, result]() {
                        if (guard) {
                            guard->handleFlowOcrFinished(
                                generation,
                                result
                            );
                        }
                    }
                );
            };
        manager->recognize(flowRequest.request);
    }

    void handleFlowOcrFinished(
        int generation,
        const OcrResult &ocrResult)
    {
        if (!matchesFlow(generation)
            || !m_flow.recognitionStarted) {
            return;
        }
        m_flowTimeoutTimer->stop();

        if (m_flow.run.cancellation
                .isCancellationRequested()
            || ocrResult.errorCode
                == QStringLiteral("CANCELLED")) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Cancelled;
            appendFlowHistoryObservation(&result, &ocrResult);
            completeFlow(
                result,
                false,
                tr8("取消"),
                QStringLiteral("状态=cancelled")
            );
            return;
        }
        if (!ocrResult.ok) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowScreenshotError(
                tr8("截图识别失败。")
            );
            appendFlowHistoryObservation(&result, &ocrResult);
            completeFlow(
                result,
                false,
                tr8("识别失败"),
                QStringLiteral("错误码=OCR_FAILED")
            );
            return;
        }

        const QString correctedText =
            applyVocabularyPreCorrectionForRun(
                m_flow.settingsSnapshot,
                ocrResult.text,
                m_flow.run.functionId,
                false
            );
        FunctionFlowValue value;
        value.text = correctedText;
        value.sourceNodeId = m_flow.node.nodeId;
        value.screenshot = flowPayload(
            &ocrResult,
            correctedText
        );

        FunctionFlowNodeResult result;
        result.state = FunctionFlowNodeState::Succeeded;
        result.values.append(value);
        completeFlow(
            result,
            false,
            tr8("识别完成"),
            QStringLiteral("节点=") + m_flow.node.nodeId
                + QStringLiteral("，字数=")
                + QString::number(correctedText.size())
        );
    }

    QWidget *hostWidget() const
    {
        return m_access.hostWidget ? m_access.hostWidget() : nullptr;
    }

    qint64 elapsedMs() const
    {
        return m_access.elapsedMs ? m_access.elapsedMs() : -1;
    }

    QString elapsedStatusText() const
    {
        const qint64 elapsed = qMax<qint64>(0, elapsedMs());
        if (elapsed < 1000) {
            return tr8("已用时 ") + QString::number(elapsed) + tr8(" ms");
        }
        return tr8("已用时 ")
            + QString::number(elapsed / 1000.0, 'f', 1)
            + tr8(" 秒");
    }

    void setTimedStatus(const QString &title, const QString &detail)
    {
        if (!m_bar) {
            return;
        }
        m_bar->setStatus(
            title,
            detail.trimmed().isEmpty()
                ? elapsedStatusText()
                : detail + tr8(" · ") + elapsedStatusText()
        );
    }

    QString functionTitle(const QString &functionId) const
    {
        return functionDisplayTitle(
            m_settings,
            functionId,
            tr8("自定义功能")
        );
    }

    bool useScreenshotFor(const QString &functionId) const
    {
        return m_settings.function(functionId).input.useScreenshot;
    }

    bool useVoiceFor(const QString &functionId) const
    {
        return m_settings.function(functionId).input.useVoice;
    }

    QString ocrEngineName(OcrEngine engine) const
    {
        return HistoryRecordBuilder::ocrEngineName(engine);
    }

    void setProcessing(bool processing)
    {
        if (m_processing == processing) {
            return;
        }
        m_processing = processing;
        if (m_access.processingChanged) {
            m_access.processingChanged(processing);
        }
    }

    void showFailure(const QString &message)
    {
        if (m_access.showFailure) {
            m_access.showFailure(message);
        }
    }

    void closeOverlay()
    {
        QPointer<ScreenCaptureOverlay> overlay = m_overlay;
        m_overlay.clear();
        if (!overlay) {
            return;
        }
        overlay->capturedCallback = nullptr;
        overlay->actionRequestedCallback = nullptr;
        overlay->cancelledCallback = nullptr;
        overlay->close();
    }

    QString applyVocabularyCorrection(const QString &text)
    {
        const QString corrected = applyVocabularyPreCorrectionForRun(
            m_settings,
            text,
            m_modeId,
            useVoiceFor(m_modeId)
        );
        if (corrected != text) {
            logRuntimeEvent(
                tr8("词库"),
                tr8("预修正"),
                tr8("来源=截图识别")
                    + QStringLiteral("，功能=") + m_modeId
                    + QStringLiteral("，原字数=")
                    + QString::number(text.size())
                    + QStringLiteral("，修正后字数=")
                    + QString::number(corrected.size()),
                elapsedMs()
            );
        }
        return corrected;
    }

    void handleCaptured(const QImage &image, const QRect &globalRect)
    {
        if (image.isNull()) {
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(
                    tr8("截图内容为空，请重新选择区域")
                );
            }
            return;
        }

        m_session.beginCaptureAttempt();
        m_correctedText.clear();
        if (m_overlay) {
            m_overlay->setRecognitionStatus(
                tr8("正在识别"),
                true,
                false
            );
        }

        if (m_ocrManager && m_ocrManager->isBusy()) {
            m_session.queuePendingCapture(image, globalRect);
            m_ocrManager->cancel();
            if (m_overlay) {
                m_overlay->setRecognitionStatus(
                    tr8("正在更新选区"),
                    true,
                    false
                );
            }
            return;
        }

        const OcrEngine engine = screenshotOcrEngineFromSettings(m_settings);
        if (engine == OcrEngine::VisionModel) {
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(
                    tr8("截图工作台暂不支持 AI 图片识别")
                );
            }
            return;
        }
        if (engine == OcrEngine::CustomCloud
            && !m_session.cloudConsent()) {
            const QMessageBox::StandardButton choice = QMessageBox::question(
                hostWidget(),
                tr8("发送截图到云端"),
                tr8("当前选择了自定义云 OCR。继续后，这张截图会发送到你填写的接口地址。是否继续？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (choice != QMessageBox::Yes) {
                setProcessing(false);
                if (m_overlay) {
                    m_overlay->setActionError(
                        tr8("已取消云端截图识别")
                    );
                }
                return;
            }
            m_session.setCloudConsent(true);
        }

        const QString tempRoot = QDir(
            QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        ).filePath(QStringLiteral("vocekit-screenshots"));
        if (!QDir().mkpath(tempRoot)) {
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(tr8("无法创建截图临时目录"));
            }
            return;
        }
        const QString temporaryPath = QDir(tempRoot).filePath(
            QUuid::createUuid().toString().mid(1, 36)
                + QStringLiteral(".png")
        );
        m_session.setTemporaryPath(temporaryPath);
        if (!image.save(temporaryPath, "PNG")) {
            m_session.takeTemporaryPath();
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(tr8("无法保存截图临时文件"));
            }
            return;
        }

        setProcessing(true);
        m_session.setCapture(image, globalRect);
        setTimedStatus(tr8("正在识别截图"), ocrEngineName(engine));

        OcrRequest request;
        request.requestId = QUuid::createUuid().toString();
        request.imagePath = QDir::toNativeSeparators(temporaryPath);
        request.languages = QStringList()
            << QStringLiteral("zh-Hans")
            << QStringLiteral("en");
        request.engine = engine;
        m_ocrManager->recognize(request);
        logRuntimeEvent(
            tr8("截图输入"),
            tr8("开始识别"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，引擎=") + ocrEngineName(engine)
        );
    }

    void handleOcrFinished(const OcrResult &result)
    {
        const QString temporaryPath = m_session.takeTemporaryPath();
        if (!temporaryPath.isEmpty()) {
            QFile::remove(temporaryPath);
        }
        if (!m_session.isActive()) {
            return;
        }

        if (m_session.hasPendingCapture()) {
            const VoiceScreenshotCapture pendingCapture =
                m_session.takePendingCapture();
            QTimer::singleShot(0, this, [this, pendingCapture]() {
                if (m_session.isActive() && m_overlay) {
                    handleCaptured(pendingCapture.image, pendingCapture.rect);
                }
            });
            return;
        }

        if (result.errorCode == QStringLiteral("CANCELLED")) {
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(tr8("截图识别已取消"));
            }
            return;
        }
        if (!result.ok || result.text.trimmed().isEmpty()) {
            const QString detail = result.errorMessage.trimmed().isEmpty()
                ? tr8("没有从截图中识别到文字。")
                : result.errorMessage.trimmed();
            logRuntimeEvent(
                tr8("截图输入"),
                tr8("识别失败"),
                QStringLiteral("功能=") + m_modeId
                    + QStringLiteral("，错误码=") + result.errorCode
                    + QStringLiteral("，原因=") + detail,
                elapsedMs()
            );
            setProcessing(false);
            if (m_overlay) {
                m_overlay->setActionError(tr8("识别失败：") + detail);
            }
            return;
        }

        m_session.setRecognitionResult(result);
        m_correctedText = applyVocabularyCorrection(
            m_session.recognizedText()
        );
        setProcessing(false);
        if (m_overlay) {
            m_overlay->setRecognizedText(m_correctedText);
        }

        logRuntimeEvent(
            tr8("截图输入"),
            tr8("识别完成"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，引擎=")
                + ocrEngineName(result.engine)
                + QStringLiteral("，字数=")
                + QString::number(m_correctedText.size()),
            result.elapsedMs
        );
        setTimedStatus(tr8("截图识别完成"), tr8("请选择处理方式"));
    }

    void handleWorkbenchAction(const QString &action)
    {
        if (!m_session.isActive()
            || !m_overlay
            || !m_session.hasRecognizedText()) {
            return;
        }
        if (m_aiWatcher) {
            m_overlay->setActionError(tr8("请等待当前模型任务完成"));
            return;
        }
        if (action == QStringLiteral("__currentFunction")) {
            processWithCurrentFunction();
            return;
        }

        const ScreenshotTextActionPlan plan =
            buildScreenshotTextActionPlan(m_settings, action);
        ScreenshotTextActionTaskRequest taskRequest;
        taskRequest.model = plan.model;
        taskRequest.systemPrompt = plan.systemPrompt;
        taskRequest.sourceText = m_session.recognizedText();
        taskRequest.useSystemProxy = m_settings.useSystemProxy;
        m_aiCancellation = CancellationSource();
        taskRequest.cancellation = m_aiCancellation.token();
        const int generation = m_session.generation();
        setProcessing(true);
        m_overlay->setRecognitionStatus(
            tr8("模型处理中"),
            true,
            false
        );
        logRuntimeEvent(
            tr8("截图工具"),
            tr8("开始处理"),
            QStringLiteral("操作=") + action
                + QStringLiteral("，模型=") + plan.model
        );

        const QString model = plan.model;
        m_aiWatcher = new QFutureWatcher<OcrAiTaskResult>(this);
        connect(
            m_aiWatcher,
            &QFutureWatcher<OcrAiTaskResult>::finished,
            this,
            [this, generation, action, model]() {
                const OcrAiTaskResult result = m_aiWatcher->result();
                m_aiWatcher->deleteLater();
                m_aiWatcher = nullptr;
                setProcessing(false);

                if (!m_session.matchesGeneration(generation)
                    || !m_session.isActive()
                    || !m_overlay) {
                    return;
                }
                if (!result.error.isEmpty()) {
                    m_overlay->setActionError(
                        tr8("处理失败：") + result.error
                    );
                    logRuntimeEvent(
                        tr8("截图工具"),
                        tr8("处理失败"),
                        QStringLiteral("操作=") + action
                            + QStringLiteral("，模型=") + model
                            + QStringLiteral("，错误=") + result.error
                    );
                    return;
                }

                m_overlay->setActionResult(result.text, tr8("处理完成"));
                logRuntimeEvent(
                    tr8("截图工具"),
                    tr8("处理完成"),
                    QStringLiteral("操作=") + action
                        + QStringLiteral("，模型=") + model
                        + QStringLiteral("，输出字数=")
                        + QString::number(result.text.size())
                );
            }
        );
        m_aiWatcher->setFuture(QtConcurrent::run(
            runScreenshotTextActionTask,
            taskRequest
        ));
    }

    void processWithCurrentFunction()
    {
        const QString screenshotText = m_correctedText.trimmed().isEmpty()
            ? m_session.recognizedText().trimmed()
            : m_correctedText.trimmed();
        if (m_modeId.trimmed().isEmpty() || screenshotText.isEmpty()) {
            if (m_overlay) {
                m_overlay->setActionError(tr8("没有可执行的截图文字"));
            }
            return;
        }

        logRuntimeEvent(
            tr8("截图输入"),
            tr8("执行对应功能"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，字数=")
                + QString::number(screenshotText.size()),
            elapsedMs()
        );

        closeOverlay();
        m_session.deactivate();
        m_session.markContextPending();
        if (m_access.processText) {
            m_access.processText(m_modeId, screenshotText);
        }
    }

    ScreenshotWorkflowAccess m_access;
    FloatingBar *m_bar = nullptr;
    AppSettingsData m_settings;
    OcrManager *m_ocrManager = nullptr;
    QTimer *m_flowCancellationTimer = nullptr;
    QTimer *m_flowTimeoutTimer = nullptr;
    QPointer<OcrManager> m_flowOcrManager;
    OcrManagerConfig m_flowOcrConfig;
    ScreenshotFlowState m_flow;
    int m_nextFlowGeneration = 0;
    QFutureWatcher<OcrAiTaskResult> *m_aiWatcher = nullptr;
    CancellationSource m_aiCancellation;
    QPointer<ScreenCaptureOverlay> m_overlay;
    VoiceScreenshotSession m_session;
    QString m_modeId;
    QString m_correctedText;
    bool m_processing = false;
};

ScreenshotWorkflowController::ScreenshotWorkflowController(
    const ScreenshotWorkflowAccess &access,
    FloatingBar *bar,
    QObject *parent)
    : QObject(parent), d(new Impl(access, bar, this))
{
}

ScreenshotWorkflowController::~ScreenshotWorkflowController()
{
    delete d;
    d = nullptr;
}

void ScreenshotWorkflowController::updateConfiguration(
    const AppSettingsData &settings,
    const SecretConfig &secrets)
{
    d->updateConfiguration(settings, secrets);
}

bool ScreenshotWorkflowController::start(
    const ScreenshotWorkflowStartRequest &request)
{
    return d->start(request);
}

bool ScreenshotWorkflowController::beginForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion)
{
    return d->beginForFlow(run, node, completion);
}

void ScreenshotWorkflowController::reset(bool keepPendingContext)
{
    d->reset(keepPendingContext);
}

bool ScreenshotWorkflowController::isActive() const
{
    return d->isActive();
}

bool ScreenshotWorkflowController::isBusy() const
{
    return d->isBusy();
}

bool ScreenshotWorkflowController::applyPendingContext(
    VoiceRunContext *context)
{
    return d->applyPendingContext(context);
}
