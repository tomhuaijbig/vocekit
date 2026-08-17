#include <QtTest>

#include "../../src/controllers/screenshot_workflow_controller.h"

#include "../../src/capture/screen_capture_overlay.h"
#include "../../src/config/app_settings_data.h"
#include "../../src/config/secret_config.h"
#include "../../src/domain/function_catalog.h"
#include "../../src/domain/history_types.h"
#include "../../src/domain/history_record_builder.h"
#include "../../src/domain/vocabulary_runtime.h"
#include "../../src/ocr/ocr_manager.h"
#include "../../src/ocr/screenshot_ocr_config.h"
#include "../../src/result_flow_config.h"
#include "../../src/runtime_log.h"
#include "../../src/tasks/screenshot_text_action_plan.h"
#include "../../src/tasks/screenshot_text_action_task.h"
#include "../../src/ui/attention_message.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <type_traits>

// Link-time fakes keep this controller test independent from real capture,
// OCR providers, vocabulary storage, model requests, dialogs, and runtime logs.
ScreenCaptureOverlay::ScreenCaptureOverlay(QWidget *parent)
    : QWidget(parent)
{
}

bool ScreenCaptureOverlay::beginCapture(QString *error)
{
    if (error) {
        *error = QStringLiteral("real capture is disabled in this test");
    }
    return false;
}

void ScreenCaptureOverlay::setRecognitionStatus(
    const QString &,
    bool,
    bool)
{
}

void ScreenCaptureOverlay::setRecognizedText(const QString &)
{
}

void ScreenCaptureOverlay::setActionResult(
    const QString &,
    const QString &)
{
}

void ScreenCaptureOverlay::setActionError(const QString &)
{
}

void ScreenCaptureOverlay::setFunctionActionTitle(const QString &)
{
}

void ScreenCaptureOverlay::paintEvent(QPaintEvent *)
{
}

void ScreenCaptureOverlay::mousePressEvent(QMouseEvent *)
{
}

void ScreenCaptureOverlay::mouseMoveEvent(QMouseEvent *)
{
}

void ScreenCaptureOverlay::mouseReleaseEvent(QMouseEvent *)
{
}

void ScreenCaptureOverlay::mouseDoubleClickEvent(QMouseEvent *)
{
}

void ScreenCaptureOverlay::keyPressEvent(QKeyEvent *)
{
}

OcrManager::OcrManager(QObject *parent)
    : QObject(parent)
{
}

OcrManager::~OcrManager()
{
}

void OcrManager::setConfig(const OcrManagerConfig &config)
{
    m_config = config;
}

OcrManagerConfig OcrManager::config() const
{
    return m_config;
}

bool OcrManager::isBusy() const
{
    return m_busy;
}

void OcrManager::recognize(const OcrRequest &request)
{
    OcrResult result;
    result.engine = request.engine;
    result.errorCode = QStringLiteral("REAL_OCR_DISABLED");
    result.errorMessage = QStringLiteral("real OCR is disabled in this test");
    if (finishedCallback) {
        finishedCallback(result);
    }
}

void OcrManager::cancel()
{
    m_busy = false;
}

OcrEngine screenshotOcrEngineFromSettings(const AppSettingsData &)
{
    return OcrEngine::Automatic;
}

OcrManagerConfig buildScreenshotOcrManagerConfig(
    const AppSettingsData &settings,
    const SecretConfig &)
{
    OcrManagerConfig config;
    config.timeoutMs = settings.ocrTimeoutMs;
    config.customCloud.useSystemProxy = settings.useSystemProxy;
    return config;
}

QString functionDisplayTitle(
    const AppSettingsData &,
    const QString &id,
    const QString &fallback)
{
    return id.trimmed().isEmpty() ? fallback : id;
}

QString HistoryRecordBuilder::ocrEngineName(OcrEngine engine)
{
    return QString::number(int(engine));
}

QString applyVocabularyPreCorrectionForRun(
    const AppSettingsData &,
    const QString &text,
    const QString &,
    bool)
{
    return text;
}

ScreenshotTextActionPlan buildScreenshotTextActionPlan(
    const AppSettingsData &,
    const QString &)
{
    return ScreenshotTextActionPlan();
}

OcrAiTaskResult runScreenshotTextActionTask(
    const ScreenshotTextActionTaskRequest &)
{
    return OcrAiTaskResult();
}

void logRuntimeEvent(
    const QString &,
    const QString &,
    const QString &,
    qint64)
{
}

void showAttentionInformation(
    QWidget *,
    const QString &,
    const QString &)
{
}

QStringList defaultResultActionIds()
{
    return QStringList()
        << QStringLiteral("copy")
        << QStringLiteral("write");
}

namespace {

FunctionFlowCompiledNode screenshotNode(
    const QString &nodeId,
    int timeoutMs = 2000)
{
    FunctionFlowCompiledNode node;
    node.nodeId = nodeId;
    node.type = FunctionFlowNodeType::ScreenshotSource;
    node.config.screenshot.ocrEngineId = QStringLiteral("windows");
    node.config.screenshot.timeoutMs = timeoutMs;
    return node;
}

FunctionFlowRunContext screenshotRun(
    const QString &runId,
    const QString &nodeId,
    const CancellationToken &cancellation,
    const QString &engineId = QStringLiteral("windows"),
    const QString &networkPolicy = QStringLiteral("direct"),
    QSharedPointer<FunctionFlowResolvedDependencies> *mutableDependencies =
        nullptr)
{
    QSharedPointer<FunctionFlowResolvedDependencies> dependencies(
        new FunctionFlowResolvedDependencies
    );
    FunctionFlowResolvedNodeSettings resolved;
    resolved.ocrEngineId = engineId;
    resolved.effectiveNetworkPolicy = networkPolicy;
    dependencies->byNodeId.insert(nodeId, resolved);
    dependencies->functionTitle = QStringLiteral("截图流程");

    FunctionFlowRunContext run;
    run.runId.value = runId;
    run.functionId = QStringLiteral("custom:screenshot");
    run.publishedRevision = 7;
    run.publishedHash = QStringLiteral("published-hash");
    run.cancellation = cancellation;
    run.dependencies = dependencies;
    if (mutableDependencies) {
        *mutableDependencies = dependencies;
    }
    return run;
}

QImage sampleImage(int seed = 1)
{
    QImage image(12, 8, QImage::Format_ARGB32);
    image.fill(QColor(30 + seed, 60 + seed, 90 + seed));
    return image;
}

OcrResult successfulOcr(
    const QString &text = QStringLiteral("识别文字"),
    OcrEngine engine = OcrEngine::WindowsOcr)
{
    OcrResult result;
    result.ok = true;
    result.engine = engine;
    result.text = text;
    result.elapsedMs = 37;
    result.usedFallback = true;
    OcrTextBlock block;
    block.text = text;
    block.points
        << QPoint(1, 1)
        << QPoint(8, 1)
        << QPoint(8, 6)
        << QPoint(1, 6);
    result.blocks.append(block);
    return result;
}

struct FlowHarness
{
    FlowHarness()
    {
        access.beginFlowCapture =
            [this](
                const ScreenshotWorkflowCapturedCallback &capturedCallback,
                const ScreenshotWorkflowCancelledCallback &cancelledCallback,
                QString *) {
                ++captureBeginCalls;
                captured = capturedCallback;
                captureCancelled = cancelledCallback;
                return captureStarts;
            };
        access.cancelFlowCapture = [this]() {
            ++captureCancelCalls;
        };
        access.recognizeForFlow =
            [this](
                const ScreenshotWorkflowFlowOcrRequest &request,
                const ScreenshotWorkflowOcrFinishedCallback &finished) {
                ++providerCalls;
                requests.append(request);
                ocrFinished = finished;
            };
        access.cancelFlowOcr = [this]() {
            ++ocrCancelCalls;
        };
        access.authorizeFlowOcrUpload = [this](OcrEngine) {
            ++authorizationCalls;
            return authorizationGranted;
        };
        access.flowTemporaryDirectory = [this]() {
            return temporaryDirectoryOverride.isEmpty()
                ? temporaryDirectory.path()
                : temporaryDirectoryOverride;
        };
        access.flowLog =
            [this](const QString &action, const QString &detail) {
                logActions.append(action);
                logDetails.append(detail);
            };
    }

    void capture(const QImage &image, const QRect &rect)
    {
        QVERIFY(captured);
        captured(image, rect);
    }

    void cancelCaptureFromUi()
    {
        QVERIFY(captureCancelled);
        captureCancelled();
    }

    void finishOcr(const OcrResult &result)
    {
        QVERIFY(ocrFinished);
        const ScreenshotWorkflowOcrFinishedCallback callback =
            ocrFinished;
        ocrFinished = ScreenshotWorkflowOcrFinishedCallback();
        callback(result);
    }

    bool directoryIsEmpty() const
    {
        return QDir(temporaryDirectory.path()).entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries
        ).isEmpty();
    }

    ScreenshotWorkflowAccess access;
    QTemporaryDir temporaryDirectory;
    ScreenshotWorkflowCapturedCallback captured;
    ScreenshotWorkflowCancelledCallback captureCancelled;
    ScreenshotWorkflowOcrFinishedCallback ocrFinished;
    QVector<ScreenshotWorkflowFlowOcrRequest> requests;
    QStringList logActions;
    QStringList logDetails;
    QString temporaryDirectoryOverride;
    int captureBeginCalls = 0;
    int captureCancelCalls = 0;
    int providerCalls = 0;
    int ocrCancelCalls = 0;
    int authorizationCalls = 0;
    bool captureStarts = true;
    bool authorizationGranted = true;
};

} // namespace

class ScreenshotWorkflowControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentWorkflowInterface();
    void voiceControllerDoesNotOwnScreenshotImplementation();
    void flowSuccessUsesFrozenPublishedConfigurationAndCleansFile();
    void emptyOcrTextIsSuccessful();
    void cloudAuthorizationIsPerRunAndDenialSkipsProvider();
    void captureCancellationCompletesExactlyOnce();
    void externalCancellationCleansAndIgnoresLateOcr();
    void timeoutCancelsProviderAndIgnoresLateOcr();
    void ocrFailureIsNotEmptySuccessAndDoesNotLeakPath();
    void controllerDestructionCleansAndCompletesExactlyOnce();
    void temporaryPathIsUniqueAcrossRuns();
    void temporaryDirectoryFailureDoesNotInvokeProviderOrLeakPath();
};

void ScreenshotWorkflowControllerTests::exposesIndependentWorkflowInterface()
{
    QVERIFY((std::is_default_constructible<
        ScreenshotWorkflowAccess
    >::value));
    QVERIFY((std::is_default_constructible<
        ScreenshotWorkflowStartRequest
    >::value));
    QVERIFY((std::is_constructible<
        ScreenshotWorkflowController,
        const ScreenshotWorkflowAccess &,
        FloatingBar *,
        QObject *
    >::value));

    typedef bool (ScreenshotWorkflowController::*BeginForFlowMethod)(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    );
    BeginForFlowMethod method =
        &ScreenshotWorkflowController::beginForFlow;
    QVERIFY(method != nullptr);
}

void ScreenshotWorkflowControllerTests::
voiceControllerDoesNotOwnScreenshotImplementation()
{
    const QString voicePath = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    const QString screenshotPath = QFINDTESTDATA(
        "../../src/controllers/screenshot_workflow_controller.cpp"
    );
    QVERIFY2(!voicePath.isEmpty(), "找不到 VoiceController 源文件");
    QVERIFY2(!screenshotPath.isEmpty(), "找不到截图工作流控制器源文件");

    QFile voiceSource(voicePath);
    QFile screenshotSource(screenshotPath);
    QVERIFY(voiceSource.open(QIODevice::ReadOnly));
    QVERIFY(screenshotSource.open(QIODevice::ReadOnly));
    const QByteArray voiceContents = voiceSource.readAll();
    const QByteArray screenshotContents = screenshotSource.readAll();

    QVERIFY(voiceContents.contains("ScreenshotWorkflowController"));
    QVERIFY(!voiceContents.contains("m_screenshotOcrManager"));
    QVERIFY(!voiceContents.contains("m_screenshotAiWatcher"));
    QVERIFY(!voiceContents.contains("m_captureOverlay"));
    QVERIFY(!voiceContents.contains("m_screenshotSession"));
    QVERIFY(!voiceContents.contains("handleScreenshotCaptured("));
    QVERIFY(!voiceContents.contains("handleScreenshotOcrFinished("));
    QVERIFY(!voiceContents.contains("handleScreenshotWorkbenchAction("));

    QVERIFY(screenshotContents.contains("OcrManager"));
    QVERIFY(screenshotContents.contains("ScreenCaptureOverlay"));
    QVERIFY(screenshotContents.contains("VoiceScreenshotSession"));
    QVERIFY(screenshotContents.contains("runScreenshotTextActionTask"));
}

void ScreenshotWorkflowControllerTests::
flowSuccessUsesFrozenPublishedConfigurationAndCleansFile()
{
    FlowHarness harness;
    QVERIFY(harness.temporaryDirectory.isValid());
    ScreenshotWorkflowController controller(harness.access, nullptr);

    CancellationSource cancellation;
    QSharedPointer<FunctionFlowResolvedDependencies> mutableDependencies;
    FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("screenshot"), 2000);
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-success"),
        node.nodeId,
        cancellation.token(),
        QStringLiteral("windows"),
        QStringLiteral("direct"),
        &mutableDependencies
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));

    mutableDependencies->byNodeId[node.nodeId].ocrEngineId =
        QStringLiteral("customCloud");
    mutableDependencies->byNodeId[node.nodeId].effectiveNetworkPolicy =
        QStringLiteral("systemProxy");
    node.config.screenshot.timeoutMs = 17;

    const QImage image = sampleImage();
    const QRect rect(11, 22, image.width(), image.height());
    harness.capture(image, rect);

    QCOMPARE(harness.providerCalls, 1);
    QCOMPARE(harness.requests.size(), 1);
    const ScreenshotWorkflowFlowOcrRequest request =
        harness.requests.first();
    QCOMPARE(request.request.engine, OcrEngine::WindowsOcr);
    QCOMPARE(request.timeoutMs, 2000);
    QCOMPARE(
        request.effectiveNetworkPolicy,
        QStringLiteral("direct")
    );
    QCOMPARE(
        request.cancellation.executionId().value,
        cancellation.executionId().value
    );
    QVERIFY(QFileInfo::exists(request.request.imagePath));

    harness.finishOcr(successfulOcr());

    QCOMPARE(completions.size(), 1);
    const FunctionFlowNodeResult result = completions.first();
    QCOMPARE(result.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(result.values.size(), 1);
    QCOMPARE(result.historyObservations.size(), 0);
    QCOMPARE(result.values.first().text, QStringLiteral("识别文字"));
    QCOMPARE(result.values.first().sourceNodeId, node.nodeId);
    QVERIFY(!result.values.first().screenshot.isNull());
    const FunctionFlowScreenshotPayload &payload =
        *result.values.first().screenshot;
    QCOMPARE(payload.image.size(), image.size());
    QCOMPARE(payload.image.pixel(2, 2), image.pixel(2, 2));
    QCOMPARE(payload.blocks.size(), 1);
    QCOMPARE(payload.blocks.first().text, QStringLiteral("识别文字"));
    QCOMPARE(payload.recognizedText, QStringLiteral("识别文字"));
    QCOMPARE(payload.engine, OcrEngine::WindowsOcr);
    QCOMPARE(payload.elapsedMs, qint64(37));
    QCOMPARE(payload.usedFallback, true);
    QCOMPARE(payload.rect, rect);
    QVERIFY(!QFileInfo::exists(request.request.imagePath));
    QVERIFY(harness.directoryIsEmpty());
    QCOMPARE(harness.authorizationCalls, 0);

    for (const QString &detail : harness.logDetails) {
        QVERIFY2(
            !detail.contains(request.request.imagePath),
            qPrintable(detail)
        );
    }
}

void ScreenshotWorkflowControllerTests::emptyOcrTextIsSuccessful()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("empty-text"));
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-empty"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(), QRect(1, 2, 12, 8));
    harness.finishOcr(successfulOcr(QString()));

    QCOMPARE(completions.size(), 1);
    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Succeeded
    );
    QCOMPARE(completions.first().values.size(), 1);
    QCOMPARE(completions.first().values.first().text, QString());
    QVERIFY(
        !completions.first().values.first().screenshot.isNull()
    );
    QCOMPARE(
        completions.first().values.first().screenshot->recognizedText,
        QString()
    );
    QVERIFY(harness.directoryIsEmpty());
}

void ScreenshotWorkflowControllerTests::
cloudAuthorizationIsPerRunAndDenialSkipsProvider()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("cloud"));
    QVector<FunctionFlowNodeResult> completions;

    CancellationSource deniedCancellation;
    const FunctionFlowRunContext deniedRun = screenshotRun(
        QStringLiteral("run-cloud-denied"),
        node.nodeId,
        deniedCancellation.token(),
        QStringLiteral("customCloud"),
        QStringLiteral("systemProxy")
    );
    harness.authorizationGranted = false;
    QVERIFY(controller.beginForFlow(
        deniedRun,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(2), QRect(3, 4, 12, 8));

    QCOMPARE(completions.size(), 1);
    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Cancelled
    );
    QCOMPARE(harness.authorizationCalls, 1);
    QCOMPARE(harness.providerCalls, 0);
    QVERIFY(harness.directoryIsEmpty());

    CancellationSource allowedCancellation;
    const FunctionFlowRunContext allowedRun = screenshotRun(
        QStringLiteral("run-cloud-allowed"),
        node.nodeId,
        allowedCancellation.token(),
        QStringLiteral("customCloud"),
        QStringLiteral("systemProxy")
    );
    harness.authorizationGranted = true;
    QVERIFY(controller.beginForFlow(
        allowedRun,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(3), QRect(5, 6, 12, 8));

    QCOMPARE(harness.authorizationCalls, 2);
    QCOMPARE(harness.providerCalls, 1);
    QCOMPARE(
        harness.requests.last().request.engine,
        OcrEngine::CustomCloud
    );
    QCOMPARE(
        harness.requests.last().effectiveNetworkPolicy,
        QStringLiteral("systemProxy")
    );
    harness.finishOcr(successfulOcr(
        QStringLiteral("云端文字"),
        OcrEngine::CustomCloud
    ));

    QCOMPARE(completions.size(), 2);
    QCOMPARE(
        completions.last().state,
        FunctionFlowNodeState::Succeeded
    );
    QVERIFY(harness.directoryIsEmpty());
}

void ScreenshotWorkflowControllerTests::
captureCancellationCompletesExactlyOnce()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("capture-cancel"));
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-capture-cancel"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    const ScreenshotWorkflowCancelledCallback lateCancel =
        harness.captureCancelled;
    const ScreenshotWorkflowCapturedCallback lateCaptured =
        harness.captured;
    harness.cancelCaptureFromUi();
    lateCancel();
    lateCaptured(sampleImage(30), QRect(4, 4, 12, 8));

    QCOMPARE(completions.size(), 1);
    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Cancelled
    );
    QCOMPARE(harness.providerCalls, 0);
    QVERIFY(harness.directoryIsEmpty());
}

void ScreenshotWorkflowControllerTests::
externalCancellationCleansAndIgnoresLateOcr()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("external-cancel"), 2000);
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-external-cancel"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(4), QRect(7, 8, 12, 8));
    QCOMPARE(harness.providerCalls, 1);
    const QString temporaryPath =
        harness.requests.first().request.imagePath;
    QVERIFY(QFileInfo::exists(temporaryPath));
    const ScreenshotWorkflowOcrFinishedCallback lateFinished =
        harness.ocrFinished;

    cancellation.cancel();
    QTRY_COMPARE(completions.size(), 1);

    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Cancelled
    );
    QCOMPARE(completions.first().values.size(), 0);
    QCOMPARE(completions.first().historyObservations.size(), 1);
    QVERIFY(
        !completions.first().historyObservations.first()
            .screenshot.isNull()
    );
    QCOMPARE(harness.ocrCancelCalls, 1);
    QVERIFY(!QFileInfo::exists(temporaryPath));
    QVERIFY(harness.directoryIsEmpty());

    lateFinished(successfulOcr(QStringLiteral("迟到结果")));
    QCoreApplication::processEvents();
    QCOMPARE(completions.size(), 1);
}

void ScreenshotWorkflowControllerTests::
timeoutCancelsProviderAndIgnoresLateOcr()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("timeout"), 30);
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-timeout"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(5), QRect(9, 10, 12, 8));
    const QString temporaryPath =
        harness.requests.first().request.imagePath;
    const ScreenshotWorkflowOcrFinishedCallback lateFinished =
        harness.ocrFinished;

    QTRY_COMPARE(completions.size(), 1);

    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Failed
    );
    QCOMPARE(
        completions.first().error.code,
        QStringLiteral("flow_screenshot_failed")
    );
    QVERIFY(completions.first().error.detail.isEmpty());
    QCOMPARE(completions.first().values.size(), 0);
    QCOMPARE(completions.first().historyObservations.size(), 1);
    QCOMPARE(harness.ocrCancelCalls, 1);
    QVERIFY(!QFileInfo::exists(temporaryPath));
    QVERIFY(harness.directoryIsEmpty());

    lateFinished(successfulOcr(QStringLiteral("超时后的结果")));
    QCoreApplication::processEvents();
    QCOMPARE(completions.size(), 1);
}

void ScreenshotWorkflowControllerTests::
ocrFailureIsNotEmptySuccessAndDoesNotLeakPath()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("ocr-failure"));
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-ocr-failure"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(6), QRect(1, 1, 12, 8));
    const QString temporaryPath =
        harness.requests.first().request.imagePath;
    OcrResult failed;
    failed.engine = OcrEngine::WindowsOcr;
    failed.errorCode =
        QStringLiteral("PROVIDER_ERROR_") + temporaryPath;
    failed.errorMessage =
        QStringLiteral("provider exposed ") + temporaryPath;
    harness.finishOcr(failed);

    QCOMPARE(completions.size(), 1);
    const FunctionFlowNodeResult result = completions.first();
    QCOMPARE(result.state, FunctionFlowNodeState::Failed);
    QCOMPARE(
        result.error.code,
        QStringLiteral("flow_screenshot_failed")
    );
    QVERIFY(!result.error.message.contains(temporaryPath));
    QVERIFY(!result.error.detail.contains(temporaryPath));
    QCOMPARE(result.values.size(), 0);
    QCOMPARE(result.historyObservations.size(), 1);
    QVERIFY(!QFileInfo::exists(temporaryPath));
    QVERIFY(harness.directoryIsEmpty());
    for (const QString &detail : harness.logDetails) {
        QVERIFY2(
            !detail.contains(temporaryPath),
            qPrintable(detail)
        );
    }
}

void ScreenshotWorkflowControllerTests::
controllerDestructionCleansAndCompletesExactlyOnce()
{
    FlowHarness harness;
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("destruction"));
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-destruction"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;
    ScreenshotWorkflowController *controller =
        new ScreenshotWorkflowController(harness.access, nullptr);

    QVERIFY(controller->beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(7), QRect(2, 2, 12, 8));
    const QString temporaryPath =
        harness.requests.first().request.imagePath;
    const ScreenshotWorkflowOcrFinishedCallback lateFinished =
        harness.ocrFinished;

    delete controller;

    QCOMPARE(completions.size(), 1);
    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Cancelled
    );
    QCOMPARE(completions.first().historyObservations.size(), 1);
    QCOMPARE(harness.ocrCancelCalls, 1);
    QVERIFY(!QFileInfo::exists(temporaryPath));
    QVERIFY(harness.directoryIsEmpty());

    lateFinished(successfulOcr(QStringLiteral("析构后的结果")));
    QCoreApplication::processEvents();
    QCOMPARE(completions.size(), 1);
}

void ScreenshotWorkflowControllerTests::
temporaryPathIsUniqueAcrossRuns()
{
    FlowHarness harness;
    ScreenshotWorkflowController controller(harness.access, nullptr);
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("unique-path"));
    QVector<FunctionFlowNodeResult> completions;
    QStringList paths;

    for (int i = 0; i < 2; ++i) {
        CancellationSource cancellation;
        const FunctionFlowRunContext run = screenshotRun(
            QStringLiteral("run-unique-%1").arg(i),
            node.nodeId,
            cancellation.token()
        );
        QVERIFY(controller.beginForFlow(
            run,
            node,
            [&completions](const FunctionFlowNodeResult &result) {
                completions.append(result);
            }
        ));
        harness.capture(
            sampleImage(10 + i),
            QRect(i, i, 12, 8)
        );
        paths.append(harness.requests.last().request.imagePath);
        QVERIFY(QFileInfo::exists(paths.last()));
        harness.finishOcr(successfulOcr(
            QStringLiteral("第 %1 次").arg(i)
        ));
        QVERIFY(!QFileInfo::exists(paths.last()));
        QVERIFY(harness.directoryIsEmpty());
    }

    QCOMPARE(completions.size(), 2);
    QVERIFY(paths.at(0) != paths.at(1));
}

void ScreenshotWorkflowControllerTests::
temporaryDirectoryFailureDoesNotInvokeProviderOrLeakPath()
{
    FlowHarness harness;
    const QString occupiedPath =
        harness.temporaryDirectory.filePath(QStringLiteral("occupied"));
    QFile occupied(occupiedPath);
    QVERIFY(occupied.open(QIODevice::WriteOnly));
    occupied.write("not a directory");
    occupied.close();
    harness.temporaryDirectoryOverride = occupiedPath;

    ScreenshotWorkflowController controller(harness.access, nullptr);
    CancellationSource cancellation;
    const FunctionFlowCompiledNode node =
        screenshotNode(QStringLiteral("directory-failure"));
    const FunctionFlowRunContext run = screenshotRun(
        QStringLiteral("run-directory-failure"),
        node.nodeId,
        cancellation.token()
    );
    QVector<FunctionFlowNodeResult> completions;

    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&completions](const FunctionFlowNodeResult &result) {
            completions.append(result);
        }
    ));
    harness.capture(sampleImage(20), QRect(3, 3, 12, 8));

    QCOMPARE(completions.size(), 1);
    QCOMPARE(
        completions.first().state,
        FunctionFlowNodeState::Failed
    );
    QCOMPARE(harness.providerCalls, 0);
    QVERIFY(!completions.first().error.message.contains(occupiedPath));
    QVERIFY(!completions.first().error.detail.contains(occupiedPath));
    for (const QString &detail : harness.logDetails) {
        QVERIFY2(!detail.contains(occupiedPath), qPrintable(detail));
    }
    QVERIFY(QFile::remove(occupiedPath));
    QVERIFY(harness.directoryIsEmpty());
}

QTEST_GUILESS_MAIN(ScreenshotWorkflowControllerTests)

#include "screenshot_workflow_controller_tests.moc"
