#include "function_command_controller.h"

#include "../capture/screenshot_types.h"

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

    if (id == QStringLiteral("hub")) {
        if (m_access.showHub) {
            m_access.showHub();
        }
        log(commandText("打开主界面"));
        return FunctionCommandOutcome::HubOpened;
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

    if (m_access.recordingConsumesPress
        && m_access.recordingConsumesPress(id)) {
        return FunctionCommandOutcome::RecordingHandled;
    }

    QString screenshotFunctionId;
    if (parseScreenshotHotkeyLogicalId(
            id,
            &screenshotFunctionId
        )) {
        startScreenshot(screenshotFunctionId, false);
        return FunctionCommandOutcome::ScreenshotStarted;
    }

    if (m_access.beginAction) {
        m_access.beginAction();
    }
    rememberTargetWindow();

    const FunctionSettings &function = m_settings.function(id);
    const int floatingBarMsec =
        qMax(0, function.output.floatingBarSeconds) * 1000;
    if (m_access.prepareFloatingBar) {
        m_access.prepareFloatingBar(
            m_settings.floatingBarEnabled,
            floatingBarMsec
        );
    }
    m_selectedText.clear();

    if (function.input.useScreenshot
        && screenshotTriggerUsesPrimary(
            function.input.screenshotTriggerMode
        )) {
        startScreenshot(id, true);
        return FunctionCommandOutcome::ScreenshotStarted;
    }

    const bool useSelection = function.input.useSelection;
    const bool useVoice = function.input.useVoice;
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
                : commandText("关闭")),
        m_access.elapsedMs ? m_access.elapsedMs() : -1
    );

    if (!useSelection && !useVoice) {
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

    if (useSelection) {
        if (!m_access.readSelectedText) {
            if (m_access.showError) {
                m_access.showError(
                    commandText("选中文字流程尚未初始化。")
                );
            }
            return FunctionCommandOutcome::SelectionBlocked;
        }
        SelectedTextWorkflowRequest request;
        request.modeId = id;
        request.strongSelectionEnabled =
            m_settings.strongSelectionEnabled;
        request.useVoice = useVoice;
        request.targetWindow = m_targetWindow;
        const SelectedTextWorkflowResult selectedResult =
            m_access.readSelectedText(request);
        m_selectedText = selectedResult.text;
        if (selectedResult.blocked) {
            return FunctionCommandOutcome::SelectionBlocked;
        }
    }

    if (!useVoice) {
        setTimedStatus(
            commandText("模型处理中"),
            commandText("正在处理选中文字")
        );
        if (m_access.processText) {
            m_access.processText(id, m_selectedText);
        }
        return FunctionCommandOutcome::TextSubmitted;
    }

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
        return FunctionCommandOutcome::ConfigurationFailed;
    }

    if (m_access.beginRecording) {
        m_access.beginRecording(id);
    }
    return FunctionCommandOutcome::RecordingStarted;
}

FunctionCommandOutcome FunctionCommandController::
handleHotkeyReleased(const QString &id)
{
    if (m_access.recordingConsumesRelease
        && m_access.recordingConsumesRelease(id)) {
        return FunctionCommandOutcome::RecordingHandled;
    }
    return FunctionCommandOutcome::NoAction;
}

FunctionCommandOutcome FunctionCommandController::
handleScreenshotTrigger(const QString &functionId)
{
    return handleHotkey(screenshotHotkeyLogicalId(functionId));
}

void FunctionCommandController::prepareScreenshotRun(
    bool targetAlreadyRemembered
)
{
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
    m_selectedText = text;
    if (m_access.processText) {
        m_access.processText(functionId, text);
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

void FunctionCommandController::startScreenshot(
    const QString &functionId,
    bool targetAlreadyRemembered
)
{
    if (m_access.beginScreenshot) {
        m_access.beginScreenshot(
            functionId,
            targetAlreadyRemembered,
            callbackValue(m_access.processing)
                || callbackValue(m_access.recordingBusy)
        );
    }
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
