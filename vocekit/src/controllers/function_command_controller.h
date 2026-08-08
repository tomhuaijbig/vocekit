#ifndef VOCEKIT_FUNCTION_COMMAND_CONTROLLER_H
#define VOCEKIT_FUNCTION_COMMAND_CONTROLLER_H

#include "selected_text_workflow_controller.h"
#include "../config/app_settings_data.h"
#include "../domain/function_flow_runtime_types.h"

#include <QMap>
#include <QObject>

#include <functional>

using FunctionCommandWindowHandle = void *;

enum class FunctionCommandOutcome
{
    NoAction,
    HubOpened,
    ScreenshotBusy,
    ProcessingBusy,
    VocabularyHandled,
    RecordingHandled,
    ScreenshotStarted,
    InputMissing,
    SelectionBlocked,
    TextSubmitted,
    ConfigurationFailed,
    RecordingStarted,
    FlowStarted,
    FlowCancelled,
    FlowBusy,
    FlowTargetUnavailable,
    FlowConfigurationFailed
};

// 命令控制器通过访问接口调用窗口、录音、截图和文本处理能力。
struct FunctionCommandAccess
{
    std::function<void(
        const QString &action,
        const QString &detail,
        qint64 elapsedMs
    )> log;
    std::function<void()> showHub;
    std::function<bool()> screenshotActive;
    std::function<bool()> processing;
    std::function<bool()> classicProcessing;
    std::function<void(const QString &, const QString &)> setStatus;
    std::function<void(const QString &, const QString &)> setTimedStatus;
    std::function<void(const QString &, const QString &)> showInformation;
    std::function<void(const QString &)> showError;
    std::function<void()> beginAction;
    std::function<void()> restartTimer;
    std::function<qint64()> elapsedMs;
    std::function<FunctionCommandWindowHandle()> captureTargetWindow;
    std::function<FunctionFlowStartOutcome(
        const FunctionFlowTriggerRequest &
    )> startPublishedFlow;
    std::function<bool(const QString &functionId)>
        releasePublishedFlowHold;
    std::function<bool(const QString &)> recordingOwnsPress;
    std::function<bool(const QString &)> recordingConsumesPress;
    std::function<bool(const QString &)> recordingConsumesRelease;
    std::function<bool()> recordingBusy;
    std::function<void(
        FunctionCommandWindowHandle,
        bool recordingBusy
    )> addVocabulary;
    std::function<bool(
        const QString &functionId,
        bool targetAlreadyRemembered,
        bool externalBusy
    )> beginScreenshot;
    std::function<SelectedTextWorkflowResult(
        const SelectedTextWorkflowRequest &
    )> readSelectedText;
    std::function<void(const QString &, const QString &)> processText;
    std::function<void(const QString &, const QString &)> processVoice;
    std::function<QString(const QString &speechProvider)>
        speechConfigurationError;
    std::function<void(const QString &functionId)> beginRecording;
    std::function<void(bool enabled, int autoHideMsec)>
        prepareFloatingBar;
};

// 统一处理全局快捷键的命令分类、输入收集和工作流启动。
class FunctionCommandController : public QObject
{
public:
    explicit FunctionCommandController(
        const FunctionCommandAccess &access,
        QObject *parent = nullptr
    );

    void updateConfiguration(const AppSettingsData &settings);

    FunctionCommandOutcome handleHotkey(const QString &id);
    void handleHotkeyPressed(const QString &id);
    FunctionCommandOutcome handleHotkeyReleased(const QString &id);
    FunctionCommandOutcome handleScreenshotTrigger(
        const QString &functionId
    );
    FunctionCommandOutcome handleScreenshotLauncherTrigger(
        const QString &functionId,
        FunctionCommandWindowHandle rememberedTargetWindow
    );

    void prepareScreenshotRun(bool targetAlreadyRemembered);
    void processScreenshotText(
        const QString &functionId,
        const QString &text
    );
    void processRecognizedVoice(
        const QString &functionId,
        const QString &text
    );
    void cancelInputSequence(const QString &functionId = QString());

    const QString &selectedText() const;
    FunctionCommandWindowHandle targetWindow() const;

private:
    void rememberTargetWindow();
    void log(
        const QString &action,
        const QString &detail = QString(),
        qint64 elapsedMs = -1
    ) const;
    FunctionCommandOutcome continueInputSequence();
    FunctionCommandOutcome completeInputSequence();
    FunctionCommandOutcome tryStartPublishedFlow(
        const QString &functionId,
        FunctionFlowTrigger trigger
    ) const;
    bool classicWorkflowBusy() const;
    void clearInputSequence();
    bool startScreenshot(
        const QString &functionId,
        bool targetAlreadyRemembered
    );
    void setTimedStatus(
        const QString &title,
        const QString &detail
    ) const;

    FunctionCommandAccess m_access;
    AppSettingsData m_settings;
    QString m_selectedText;
    QString m_activeSequenceFunctionId;
    QStringList m_pendingInputIds;
    QString m_sequenceVoiceText;
    QString m_sequenceScreenshotText;
    QMap<QString, FunctionExecutionMode> m_holdRunOwners;
    bool m_sequenceVoiceCompleted = false;
    FunctionCommandWindowHandle m_targetWindow = nullptr;
};

// 平台适配器只负责取得当前前台窗口。
FunctionCommandWindowHandle captureForegroundFunctionCommandWindow();

#endif // VOCEKIT_FUNCTION_COMMAND_CONTROLLER_H
