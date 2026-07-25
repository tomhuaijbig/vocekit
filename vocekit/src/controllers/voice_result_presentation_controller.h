#ifndef VOCEKIT_VOICE_RESULT_PRESENTATION_CONTROLLER_H
#define VOCEKIT_VOICE_RESULT_PRESENTATION_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/voice_result_completion_executor.h"
#include "../domain/voice_run_context.h"
#include "../output/clipboard_writer.h"

#include <QObject>
#include <QString>

#include <functional>

class FloatingBar;
class VoiceRunSession;

// 结果展示控制器保存历史时使用的统一请求，外层无需感知具体窗口类型。
struct VoiceResultPresentationHistoryRequest
{
    VoiceRunContext context;
    QString output;
    QString error;
    bool draft = false;
    QString modelOverride;
};

// 结果展示层通过回调使用模型、历史、词库和系统写入能力。
struct VoiceResultPresentationAccess
{
    std::function<bool(const AppSettingsData &)> applyAndSave;
    std::function<QString(
        const VoiceRunContext &,
        const QString &,
        const QString &,
        QString *,
        const std::function<void(const QString &)> &
    )> runModel;
    std::function<QString(
        const VoiceRunContext &,
        const QString &
    )> finalizeOutput;
    std::function<void(
        const VoiceResultPresentationHistoryRequest &
    )> saveHistory;
    std::function<void(
        const QString &,
        const QString &,
        const QString &
    )> addVocabulary;
    std::function<void()> notifySettingsChanged;
    std::function<void(const QString &, const QString &)> showInformation;
    std::function<void(const QString &)> showError;
    std::function<void(const QString &, const QString &)> setTimedStatus;
    std::function<void(const QString &, bool, bool)> writeText;
    std::function<ClipboardWindowHandle()> targetWindow;
    std::function<bool()> processing;
    std::function<void(bool)> processingChanged;
    std::function<void()> cancelActiveModel;
    std::function<bool()> lastModelRunCancelled;
};

// 统一管理结果小框、截图结果窗、流式展示、重试、草稿和最终写入。
class VoiceResultPresentationController : public QObject
{
public:
    VoiceResultPresentationController(
        const VoiceResultPresentationAccess &access,
        FloatingBar *bar,
        VoiceRunSession *runSession,
        QObject *parent = nullptr
    );
    ~VoiceResultPresentationController() override;

    void updateConfiguration(const AppSettingsData &settings);
    bool shouldStream(const VoiceRunContext &context) const;
    VoiceResultCompletionHandlers completionHandlers() const;
    void stream(const VoiceRunContext &context);
    void fail(
        const VoiceRunContext &context,
        const VoiceResultCompletionResult &completion
    );
    void present(
        const VoiceRunContext &context,
        const QString &finalOutput
    );

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_VOICE_RESULT_PRESENTATION_CONTROLLER_H
