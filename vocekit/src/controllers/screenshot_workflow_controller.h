#ifndef VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H
#define VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H

#include "../domain/function_flow_compiler.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <functional>

class FloatingBar;
class QWidget;
struct AppSettingsData;
struct SecretConfig;
struct VoiceRunContext;

using ScreenshotWorkflowCapturedCallback =
    std::function<void(const QImage &, const QRect &)>;
using ScreenshotWorkflowCancelledCallback = std::function<void()>;
using ScreenshotWorkflowOcrFinishedCallback =
    std::function<void(const OcrResult &)>;

// 冻结一次流程截图 OCR 所需的运行参数，供真实 Provider 或测试替身消费。
struct ScreenshotWorkflowFlowOcrRequest
{
    OcrRequest request;
    int timeoutMs = 45000;
    QString effectiveNetworkPolicy;
    CancellationToken cancellation;
};

// 截图工作流只通过这些回调接入外层任务，不依赖 VoiceController 的实现细节。
struct ScreenshotWorkflowAccess
{
    std::function<QWidget *()> hostWidget;
    std::function<qint64()> elapsedMs;
    std::function<void(const QString &, bool)> prepareRun;
    std::function<void(bool)> processingChanged;
    std::function<void(const QString &)> showFailure;
    std::function<void(const QString &, const QString &)> processText;
    std::function<void(const QString &)> cancelled;

    // 流程入口的可替换边界。未提供时继续使用真实截图层和 OcrManager。
    std::function<bool(
        const ScreenshotWorkflowCapturedCallback &,
        const ScreenshotWorkflowCancelledCallback &,
        QString *
    )> beginFlowCapture;
    std::function<void()> cancelFlowCapture;
    std::function<void(
        const ScreenshotWorkflowFlowOcrRequest &,
        const ScreenshotWorkflowOcrFinishedCallback &
    )> recognizeForFlow;
    std::function<void()> cancelFlowOcr;
    std::function<bool(OcrEngine)> authorizeFlowOcrUpload;
    std::function<QString()> flowTemporaryDirectory;
    std::function<void(const QString &, const QString &)> flowLog;
};

struct ScreenshotWorkflowStartRequest
{
    QString functionId;
    bool externalBusy = false;
    bool targetAlreadyRemembered = false;
};

// 完整管理截图选区、OCR、截图工具栏模型动作、临时文件和执行上下文。
class ScreenshotWorkflowController : public QObject
{
public:
    ScreenshotWorkflowController(
        const ScreenshotWorkflowAccess &access,
        FloatingBar *bar,
        QObject *parent = nullptr
    );
    ~ScreenshotWorkflowController() override;

    void updateConfiguration(
        const AppSettingsData &settings,
        const SecretConfig &secrets
    );
    bool start(const ScreenshotWorkflowStartRequest &request);
    bool beginForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    );
    void reset(bool keepPendingContext = false);

    bool isActive() const;
    bool isBusy() const;
    bool applyPendingContext(VoiceRunContext *context);

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H
