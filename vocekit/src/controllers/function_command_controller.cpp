#include "function_command_controller.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"

namespace {

QString commandText(const char *text)
{
    return QString::fromUtf8(text);
}

bool callbackValue(
    const std::function<bool()> &callback,
    bool fallback = false
)
{
    return callback ? callback() : fallback;
}

FunctionCommandOutcome commandOutcomeForFlow(
    FunctionFlowStartOutcome outcome)
{
    if (outcome == FunctionFlowStartOutcome::Started) {
        return FunctionCommandOutcome::FlowStarted;
    }
    if (outcome
        == FunctionFlowStartOutcome::CancelledExisting) {
        return FunctionCommandOutcome::FlowCancelled;
    }
    if (outcome == FunctionFlowStartOutcome::Busy) {
        return FunctionCommandOutcome::FlowBusy;
    }
    if (outcome
        == FunctionFlowStartOutcome::TargetUnavailable) {
        return FunctionCommandOutcome::FlowTargetUnavailable;
    }
    return FunctionCommandOutcome::FlowConfigurationFailed;
}

} // namespace

FunctionCommandController::FunctionCommandController(
    const FunctionCommandAccess &access,
    QObject *parent
)
    : QObject(parent),
      m_access(access)
{
}

void FunctionCommandController::updateConfiguration(
    const AppSettingsData &settings
)
{
    m_settings = settings;
}

FunctionCommandOutcome FunctionCommandController::handleHotkey(
    const QString &commandId
)
{
    const QString id = commandId.trimmed();
    log(commandText("触发"), QStringLiteral("功能=") + id);

    if (id.isEmpty()) {
        return FunctionCommandOutcome::NoAction;
    }
    if (id == QStringLiteral("hub")) {
        if (m_access.showHub) {
            m_access.showHub();
        }
        log(commandText("打开主界面"));
        return FunctionCommandOutcome::HubOpened;
    }

    QString screenshotFunctionId;
    const bool screenshotHotkey =
        parseScreenshotHotkeyLogicalId(
            id,
            &screenshotFunctionId
        );
    if (screenshotHotkey) {
        if (m_settings.functionIndex(screenshotFunctionId) < 0) {
            return FunctionCommandOutcome::NoAction;
        }
        if (m_access.recordingConsumesPress
            && m_access.recordingConsumesPress(id)) {
            return FunctionCommandOutcome::RecordingHandled;
        }
        clearInputSequence();
        rememberTargetWindow();
        const FunctionCommandOutcome flow =
            tryStartPublishedFlow(
                screenshotFunctionId,
                FunctionFlowTrigger::ScreenshotHotkey
            );
        if (flow != FunctionCommandOutcome::NoAction) {
            return flow;
        }
    } else if (id != QStringLiteral("vocabulary_add")) {
        // 已启动的录音拥有本次按键；先让旧运行停止，再按当前模式分流。
        if (m_access.recordingOwnsPress
            && m_access.recordingOwnsPress(id)
            && m_access.recordingConsumesPress
            && m_access.recordingConsumesPress(id)) {
            return FunctionCommandOutcome::RecordingHandled;
        }
        if (m_settings.functionIndex(id) < 0) {
            return FunctionCommandOutcome::NoAction;
        }
        rememberTargetWindow();
        const FunctionCommandOutcome flow =
            tryStartPublishedFlow(
                id,
                FunctionFlowTrigger::MainHotkey
            );
        if (flow != FunctionCommandOutcome::NoAction) {
            return flow;
        }
    }

    if (callbackValue(m_access.screenshotActive)) {
        if (m_access.setStatus) {
            m_access.setStatus(
                commandText("截图任务尚未结束"),
                commandText("请先在截图工具栏执行功能或关闭截图。")
            );
        }
        log(
            commandText("忽略"),
            QStringLiteral("原因=截图工作流仍打开，功能=") + id
        );
        return FunctionCommandOutcome::ScreenshotBusy;
    }

    if (callbackValue(m_access.processing)) {
        if (m_access.setStatus) {
            m_access.setStatus(
                commandText("正在处理"),
                commandText("请等待当前任务完成后再按快捷键。")
            );
        }
        log(
            commandText("忽略"),
            QStringLiteral("原因=正在处理，功能=") + id
        );
        return FunctionCommandOutcome::ProcessingBusy;
    }

    if (id == QStringLiteral("vocabulary_add")) {
        if (m_access.restartTimer) {
            m_access.restartTimer();
        }
        rememberTargetWindow();
        if (m_access.addVocabulary) {
            m_access.addVocabulary(
                m_targetWindow,
                callbackValue(m_access.recordingBusy)
            );
        }
        return FunctionCommandOutcome::VocabularyHandled;
    }

    // 已知但非当前 owner 的按键保持原有忙碌保护，不允许启动第二个录音。
    if (!screenshotHotkey
        && m_access.recordingConsumesPress
        && m_access.recordingConsumesPress(id)) {
        return FunctionCommandOutcome::RecordingHandled;
    }

    if (screenshotHotkey) {
        clearInputSequence();
        prepareFloatingBarForFunction(
            m_settings.function(screenshotFunctionId)
        );
        return startScreenshot(screenshotFunctionId, true)
            ? FunctionCommandOutcome::ScreenshotStarted
            : FunctionCommandOutcome::InputMissing;
    }

    if (m_access.beginAction) {
        m_access.beginAction();
    }
    // 目标窗口已在流程分流和任何状态 UI 之前冻结。

    const FunctionSettings &function = m_settings.function(id);
    m_selectedText.clear();
    clearInputSequence();

    const bool useSelection = function.input.useSelection;
    const bool useVoice = function.input.useVoice;
    const bool usePrimaryScreenshot =
        function.input.useScreenshot
        && screenshotTriggerUsesPrimary(
            function.input.screenshotTriggerMode
        );
    const QStringList inputOrder =
        normalizeFunctionInputOrder(function.input.order);
    log(
        commandText("触发"),
        QStringLiteral("功能=") + id
            + commandText("，选中文字=")
            + (useSelection
                ? commandText("开启")
                : commandText("关闭"))
            + commandText("，语音=")
            + (useVoice
                ? commandText("开启")
                : commandText("关闭"))
            + commandText("，截图=")
            + (usePrimaryScreenshot
                ? commandText("开启")
                : commandText("关闭"))
            + commandText("，输入顺序=")
            + inputOrder.join(QStringLiteral(">")),
        m_access.elapsedMs ? m_access.elapsedMs() : -1
    );

    for (const QString &inputId : inputOrder) {
        if (inputId == functionInputVoiceId() && useVoice) {
            m_pendingInputIds.append(inputId);
        } else if (inputId == functionInputSelectionId()
                   && useSelection) {
            m_pendingInputIds.append(inputId);
        } else if (inputId == functionInputScreenshotId()
                   && usePrimaryScreenshot) {
            m_pendingInputIds.append(inputId);
        }
    }

    if (m_pendingInputIds.isEmpty()) {
        if (function.input.useScreenshot) {
            if (m_access.showInformation) {
                m_access.showInformation(
                    commandText("请使用截图入口"),
                    commandText(
                        "这个功能只启用了截图输入。请使用独立截图快捷键或桌面截图悬浮入口。"
                    )
                );
            }
        } else if (m_access.showError) {
            m_access.showError(commandText(
                "这个功能没有启用任何输入方式，请在左侧“功能自定义”中启用选中文字或语音输入。"
            ));
        }
        return FunctionCommandOutcome::InputMissing;
    }

    prepareFloatingBarForFunction(function);
    m_activeSequenceFunctionId = id;
    return continueInputSequence();
}

void FunctionCommandController::handleHotkeyPressed(
    const QString &id)
{
    const QString functionId = id.trimmed();
    if (functionId.isEmpty()
        || m_holdRunOwners.contains(functionId)
        || m_settings.functionIndex(functionId) < 0) {
        return;
    }
    m_holdRunOwners.insert(
        functionId,
        m_settings.function(functionId).executionMode
    );
}

FunctionCommandOutcome FunctionCommandController::
handleHotkeyReleased(const QString &id)
{
    const QString functionId = id.trimmed();
    if (functionId.isEmpty()
        || !m_holdRunOwners.contains(functionId)) {
        return FunctionCommandOutcome::NoAction;
    }
    const FunctionExecutionMode owner =
        m_holdRunOwners.take(functionId);
    if (owner == FunctionExecutionMode::Canvas) {
        if (m_access.releasePublishedFlowHold
            && m_access.releasePublishedFlowHold(functionId)) {
            return FunctionCommandOutcome::RecordingHandled;
        }
        return FunctionCommandOutcome::NoAction;
    }
    if (m_access.recordingConsumesRelease
        && m_access.recordingConsumesRelease(functionId)) {
        return FunctionCommandOutcome::RecordingHandled;
    }
    return FunctionCommandOutcome::NoAction;
}

FunctionCommandOutcome FunctionCommandController::
handleScreenshotTrigger(const QString &functionId)
{
    const QString id = functionId.trimmed();
    if (id.isEmpty() || m_settings.functionIndex(id) < 0) {
        return FunctionCommandOutcome::NoAction;
    }
    return handleHotkey(screenshotHotkeyLogicalId(id));
}

FunctionCommandOutcome FunctionCommandController::
handleScreenshotLauncherTrigger(
    const QString &functionId,
    FunctionCommandWindowHandle rememberedTargetWindow)
{
    const QString id = functionId.trimmed();
    if (id.isEmpty() || m_settings.functionIndex(id) < 0) {
        return FunctionCommandOutcome::NoAction;
    }
    clearInputSequence();
    m_targetWindow = rememberedTargetWindow;
    const FunctionCommandOutcome flow =
        tryStartPublishedFlow(
            id,
            FunctionFlowTrigger::ScreenshotLauncher
        );
    if (flow != FunctionCommandOutcome::NoAction) {
        return flow;
    }

    if (callbackValue(m_access.screenshotActive)) {
        return FunctionCommandOutcome::ScreenshotBusy;
    }
    if (callbackValue(m_access.processing)
        || callbackValue(m_access.recordingBusy)) {
        return FunctionCommandOutcome::ProcessingBusy;
    }
    clearInputSequence();
    prepareFloatingBarForFunction(m_settings.function(id));
    return startScreenshot(id, true)
        ? FunctionCommandOutcome::ScreenshotStarted
        : FunctionCommandOutcome::InputMissing;
}

void FunctionCommandController::prepareScreenshotRun(
    bool targetAlreadyRemembered
)
{
    if (!m_activeSequenceFunctionId.isEmpty()) {
        return;
    }
    if (m_access.beginAction) {
        m_access.beginAction();
    }
    if (!targetAlreadyRemembered) {
        rememberTargetWindow();
    }
    m_selectedText.clear();
}

void FunctionCommandController::processScreenshotText(
    const QString &functionId,
    const QString &text
)
{
    const QString id = functionId.trimmed();
    if (m_activeSequenceFunctionId == id) {
        m_sequenceScreenshotText = text;
        continueInputSequence();
        return;
    }

    m_selectedText = text;
    if (m_access.processText) {
        m_access.processText(functionId, text);
    }
}

void FunctionCommandController::processRecognizedVoice(
    const QString &functionId,
    const QString &text
)
{
    const QString id = functionId.trimmed();
    if (m_activeSequenceFunctionId == id) {
        m_sequenceVoiceText = text;
        m_sequenceVoiceCompleted = true;
        continueInputSequence();
        return;
    }

    if (m_access.processVoice) {
        m_access.processVoice(functionId, text);
    }
}

void FunctionCommandController::cancelInputSequence(
    const QString &functionId
)
{
    const QString id = functionId.trimmed();
    if (id.isEmpty() || m_activeSequenceFunctionId == id) {
        clearInputSequence();
    }
}

const QString &FunctionCommandController::selectedText() const
{
    return m_selectedText;
}

FunctionCommandWindowHandle
FunctionCommandController::targetWindow() const
{
    return m_targetWindow;
}

void FunctionCommandController::rememberTargetWindow()
{
    m_targetWindow = m_access.captureTargetWindow
        ? m_access.captureTargetWindow()
        : nullptr;
}

void FunctionCommandController::log(
    const QString &action,
    const QString &detail,
    qint64 elapsedMs
) const
{
    if (m_access.log) {
        m_access.log(action, detail, elapsedMs);
    }
}

FunctionCommandOutcome
FunctionCommandController::continueInputSequence()
{
    const QString functionId = m_activeSequenceFunctionId;
    if (functionId.isEmpty()) {
        return FunctionCommandOutcome::NoAction;
    }

    while (!m_pendingInputIds.isEmpty()) {
        const QString inputId = m_pendingInputIds.takeFirst();
        if (inputId == functionInputSelectionId()) {
            if (!m_access.readSelectedText) {
                if (m_access.showError) {
                    m_access.showError(
                        commandText(
                            "选中文字流程尚未初始化。"
                        )
                    );
                }
                clearInputSequence();
                return FunctionCommandOutcome::SelectionBlocked;
            }

            SelectedTextWorkflowRequest request;
            request.modeId = functionId;
            request.strongSelectionEnabled =
                m_settings.strongSelectionEnabled;
            request.useVoice =
                m_settings.function(functionId).input.useVoice;
            request.targetWindow = m_targetWindow;
            const SelectedTextWorkflowResult selectedResult =
                m_access.readSelectedText(request);
            m_selectedText = selectedResult.text;
            if (selectedResult.blocked) {
                clearInputSequence();
                return FunctionCommandOutcome::SelectionBlocked;
            }
            continue;
        }

        if (inputId == functionInputVoiceId()) {
            const QString configurationError =
                m_access.speechConfigurationError
                    ? m_access.speechConfigurationError(
                        m_settings.speechProvider
                    )
                    : QString();
            if (!configurationError.isEmpty()) {
                if (m_access.showError) {
                    m_access.showError(configurationError);
                }
                clearInputSequence();
                return FunctionCommandOutcome::ConfigurationFailed;
            }
            if (m_access.beginRecording) {
                m_access.beginRecording(functionId);
            }
            return FunctionCommandOutcome::RecordingStarted;
        }

        if (inputId == functionInputScreenshotId()) {
            if (!startScreenshot(functionId, true)) {
                clearInputSequence();
                return FunctionCommandOutcome::InputMissing;
            }
            return FunctionCommandOutcome::ScreenshotStarted;
        }
    }

    return completeInputSequence();
}

FunctionCommandOutcome
FunctionCommandController::completeInputSequence()
{
    const QString functionId = m_activeSequenceFunctionId;
    const QString voiceText = m_sequenceVoiceText;
    const QString screenshotText = m_sequenceScreenshotText;
    const bool voiceCompleted = m_sequenceVoiceCompleted;
    clearInputSequence();

    if (functionId.isEmpty()) {
        return FunctionCommandOutcome::NoAction;
    }
    if (voiceCompleted) {
        if (m_access.processVoice) {
            m_access.processVoice(functionId, voiceText);
        }
        return FunctionCommandOutcome::TextSubmitted;
    }

    const QString inputText = screenshotText.trimmed().isEmpty()
        ? m_selectedText
        : screenshotText;
    setTimedStatus(
        commandText("模型处理中"),
        commandText("正在按输入顺序处理内容")
    );
    if (m_access.processText) {
        m_access.processText(functionId, inputText);
    }
    return FunctionCommandOutcome::TextSubmitted;
}

FunctionCommandOutcome
FunctionCommandController::tryStartPublishedFlow(
    const QString &functionId,
    FunctionFlowTrigger trigger) const
{
    const QString id = functionId.trimmed();
    const FunctionSettings &function = m_settings.function(id);
    if (function.executionMode != FunctionExecutionMode::Canvas) {
        return FunctionCommandOutcome::NoAction;
    }
    if (!m_access.startPublishedFlow) {
        if (m_access.showError) {
            m_access.showError(commandText(
                "画布运行服务尚未初始化。"
            ));
        }
        return FunctionCommandOutcome::FlowConfigurationFailed;
    }
    FunctionFlowTriggerRequest request;
    request.functionId = id;
    request.trigger = trigger;
    request.targetWindow = m_targetWindow;
    request.classicWorkflowBusy = classicWorkflowBusy();
    const FunctionFlowStartOutcome outcome =
        m_access.startPublishedFlow(request);
    if (outcome == FunctionFlowStartOutcome::Started) {
        prepareFloatingBarForFunction(function);
    }
    if (outcome == FunctionFlowStartOutcome::NotAvailable) {
        if (m_access.showError) {
            m_access.showError(commandText(
                "当前画布未配置此入口。"
            ));
        }
        return FunctionCommandOutcome::FlowConfigurationFailed;
    }
    return commandOutcomeForFlow(outcome);
}

bool FunctionCommandController::classicWorkflowBusy() const
{
    const bool processing = m_access.classicProcessing
        ? m_access.classicProcessing()
        : callbackValue(m_access.processing);
    return processing
        || callbackValue(m_access.screenshotActive)
        || callbackValue(m_access.recordingBusy);
}

void FunctionCommandController::clearInputSequence()
{
    m_activeSequenceFunctionId.clear();
    m_pendingInputIds.clear();
    m_sequenceVoiceText.clear();
    m_sequenceScreenshotText.clear();
    m_sequenceVoiceCompleted = false;
}

bool FunctionCommandController::startScreenshot(
    const QString &functionId,
    bool targetAlreadyRemembered
)
{
    if (m_access.beginScreenshot) {
        const bool sequenceActive =
            m_activeSequenceFunctionId == functionId.trimmed();
        return m_access.beginScreenshot(
            functionId,
            targetAlreadyRemembered,
            (!sequenceActive
                && callbackValue(m_access.processing))
                || callbackValue(m_access.recordingBusy)
        );
    }
    return false;
}

void FunctionCommandController::prepareFloatingBarForFunction(
    const FunctionSettings &function) const
{
    if (!m_access.prepareFloatingBar) {
        return;
    }
    const QString functionStyle = function.builtIn
        ? floatingBarStyleInherit()
        : function.output.floatingBarStyleOverride;
    m_access.prepareFloatingBar(
        m_settings.floatingBarEnabled,
        qMax(0, function.output.floatingBarSeconds) * 1000,
        resolveFloatingBarStyle(
            functionStyle,
            m_settings.floatingBarStyle
        )
    );
}

void FunctionCommandController::setTimedStatus(
    const QString &title,
    const QString &detail
) const
{
    if (m_access.setTimedStatus) {
        m_access.setTimedStatus(title, detail);
    } else if (m_access.setStatus) {
        m_access.setStatus(title, detail);
    }
}
