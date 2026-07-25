#ifndef VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H
#define VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <functional>

class FloatingBar;
class QWidget;
struct AppSettingsData;
struct SecretConfig;
struct VoiceRunContext;

// 截图工作流只通过这些回调接入外层任务，不依赖 VoiceController 的实现细节。
struct ScreenshotWorkflowAccess
{
    std::function<QWidget *()> hostWidget;
    std::function<qint64()> elapsedMs;
    std::function<void(const QString &, bool)> prepareRun;
    std::function<void(bool)> processingChanged;
    std::function<void(const QString &)> showFailure;
    std::function<void(const QString &, const QString &)> processText;
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
    void reset(bool keepPendingContext = false);

    bool isActive() const;
    bool isBusy() const;
    bool applyPendingContext(VoiceRunContext *context);

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_SCREENSHOT_WORKFLOW_CONTROLLER_H
