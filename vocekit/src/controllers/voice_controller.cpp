#include "voice_controller.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

#include "../config/app_settings_defaults.h"
#include "../config/secret_config.h"
#include "function_command_controller.h"
#include "selected_text_workflow_controller.h"
#include "screenshot_workflow_controller.h"
#include "vocabulary_quick_add_controller.h"
#include "voice_controller_host.h"
#include "voice_run_lifecycle_controller.h"
#include "voice_result_presentation_controller.h"
#include "voice_recording_workflow_controller.h"
#include "../domain/app_legacy_types.h"
#include "../domain/function_catalog.h"
#include "../domain/prompt_runtime_library.h"
#include "../domain/vocabulary_runtime.h"
#include "../domain/voice_function_execution_pipeline.h"
#include "../domain/voice_run_context.h"
#include "../domain/voice_run_session.h"
#include "../output/clipboard_writer.h"
#include "../providers/network_error_messages.h"
#include "../providers/streaming_speech_session_factory.h"
#include "../providers/windows_speech_helper_protocol.h"
#include "../tasks/diagnostic_helpers.h"
#include "../result_flow_config.h"
#include "../runtime_log.h"
#include "../tasks/processing_guard.h"
#include "../tasks/speech_recognition_task.h"
#include "../ui/attention_message.h"
#include "../ui/app_dialogs.h"
#include "../ui/floating_bar.h"
#include "../ui/vocabulary_quick_add_dialog.h"
// VoiceController implementation: hotkeys, screenshot input, recording, recognition, model processing and output.
class VoiceController::Impl : public QObject
{
public:
    Impl(
        const VoiceControllerAccess &access,
        FloatingBar *bar,
        VoiceControllerHost *host,
        QObject *parent = nullptr
    )
        : QObject(parent),
          m_access(access),
          m_bar(bar),
          m_host(host)
    {
        ScreenshotWorkflowAccess screenshotAccess;
        screenshotAccess.hostWidget = [this]() {
            return hostWidget();
        };
        screenshotAccess.elapsedMs = [this]() {
            return currentActionElapsedMs();
        };
        screenshotAccess.prepareRun = [this](
            const QString &,
            bool targetAlreadyRemembered
        ) {
            if (m_functionCommands) {
                m_functionCommands->prepareScreenshotRun(
                    targetAlreadyRemembered
                );
            }
        };
        screenshotAccess.processingChanged = [this](bool processing) {
            m_processing = processing;
        };
        screenshotAccess.showFailure = [this](const QString &message) {
            showError(message);
        };
        screenshotAccess.processText = [this](
            const QString &functionId,
            const QString &text
        ) {
            if (m_functionCommands) {
                m_functionCommands->processScreenshotText(
                    functionId,
                    text
                );
            }
        };
        screenshotAccess.cancelled = [this](
            const QString &functionId
        ) {
            if (m_functionCommands) {
                m_functionCommands->cancelInputSequence(functionId);
            }
        };
        m_screenshotWorkflow = new ScreenshotWorkflowController(
            screenshotAccess,
            m_bar,
            this
        );
        VoiceRecordingWorkflowAccess recordingAccess;
        recordingAccess.elapsedMs = [this]() {
            return currentActionElapsedMs();
        };
        recordingAccess.externalProcessing = [this]() {
            return m_processing;
        };
        recordingAccess.processingChanged = [this](bool processing) {
            m_processing = processing;
        };
        recordingAccess.showFailure = [this](const QString &message) {
            showError(message);
        };
        recordingAccess.showWindowsSpeechFailure = [this](
            const QString &errorCode,
            const QString &message
        ) {
            showWindowsSpeechFailureAttention(
                hostWidget(),
                errorCode,
                message
            );
        };
        recordingAccess.saveFailureHistory = [this](
            const QString &modeId,
            const QString &error
        ) {
            if (!m_runLifecycle) {
                return;
            }
            VoiceRunLifecycleHistoryRequest request;
            request.modeId = modeId;
            request.error = error;
            m_runLifecycle->saveHistory(request);
        };
        recordingAccess.processRecognizedSpeech = [this](
            const QString &modeId,
            const QString &text
        ) {
            if (m_functionCommands) {
                m_functionCommands->processRecognizedVoice(
                    modeId,
                    text
                );
            } else {
                processRecognizedSpeech(modeId, text);
            }
        };
        recordingAccess.createStreamingSpeechSession = [](
            const StreamingSpeechSessionRequest &request,
            const StreamingSpeechCallbacks &callbacks
        ) {
            return createDefaultStreamingSpeechSession(
                request,
                callbacks
            );
        };
        m_recordingWorkflow =
            new VoiceRecordingWorkflowController(
                recordingAccess,
                m_bar,
                &m_runSession,
                this
            );
        VoiceRunLifecycleAccess lifecycleAccess =
            defaultVoiceRunLifecycleAccess();
        lifecycleAccess.promptSnapshot = [this]() {
            return promptRuntimeSnapshot();
        };
        lifecycleAccess.fallbackAudioPath = [this]() {
            return m_recordingWorkflow
                ? m_recordingWorkflow->lastWavPath()
                : QString();
        };
        lifecycleAccess.elapsedMs = [this]() {
            return currentActionElapsedMs();
        };
        lifecycleAccess.historySaved = [this](
            const QString &modeDetailPath
        ) {
            if (m_host) {
                m_host->notifyHistoryRecordSavedForVoiceController(
                    modeDetailPath
                );
            }
        };
        lifecycleAccess.historyLogged = [](
            const QString &action,
            const QString &detail,
            qint64 elapsedMs
        ) {
            logRuntimeEvent(
                tr8("历史记录"),
                action,
                detail,
                elapsedMs
            );
        };
        m_runLifecycle = new VoiceRunLifecycleController(
            lifecycleAccess,
            &m_runSession,
            this
        );
        SelectedTextWorkflowAccess selectedTextAccess =
            defaultSelectedTextWorkflowAccess();
        selectedTextAccess.preCorrect = [this](
            const QString &text,
            const QString &modeId,
            const QString &sourceLabel,
            bool hasVoiceInput
        ) {
            return applyVocabularyPreCorrection(
                text,
                modeId,
                sourceLabel,
                hasVoiceInput
            );
        };
        selectedTextAccess.setStatus = [this](
            const QString &title,
            const QString &detail
        ) {
            m_bar->setStatus(title, detail);
        };
        selectedTextAccess.hideStatusLater = [this]() {
            m_bar->hideLater();
        };
        selectedTextAccess.showInformation = [this](
            const QString &title,
            const QString &message
        ) {
            showAttentionInformation(hostWidget(), title, message);
        };
        selectedTextAccess.recordReadResult =
            [this](const SelectedTextReadResult &result) {
                logRuntimeEvent(
                    tr8("选中文字"),
                    result.text.isEmpty()
                        ? tr8("未识别到")
                        : tr8("已识别"),
                    QStringLiteral("字数=")
                        + QString::number(result.text.size())
                );
            };
        m_selectedTextWorkflow =
            new SelectedTextWorkflowController(
                selectedTextAccess,
                this
            );
        VocabularyQuickAddAccess vocabularyAccess =
            defaultVocabularyQuickAddAccess();
        vocabularyAccess.askChoice = [this]() {
            return askVocabularyQuickAddChoice(hostWidget());
        };
        vocabularyAccess.vocabularyPrompt = [this]() {
            return promptRuntimeForVocabulary(
                promptRuntimeSnapshot()
            );
        };
        vocabularyAccess.openEditor =
            [this](const VocabularyEntry &entry) {
                if (m_host) {
                    m_host->openVocabularyEntryEditorForVoiceController(
                        entry
                    );
                }
            };
        vocabularyAccess.notifyVocabularyChanged = [this]() {
            if (m_host) {
                m_host->notifyVocabularyChangedForVoiceController();
            }
        };
        vocabularyAccess.prepareStatus = [this](
            bool enabled,
            int autoHideMsec
        ) {
            m_bar->setEnabledVisible(enabled);
            m_bar->setSuppressed(false);
            m_bar->setAutoHideMsec(autoHideMsec);
        };
        vocabularyAccess.setStatus = [this](
            const QString &title,
            const QString &detail
        ) {
            m_bar->setStatus(title, detail);
        };
        vocabularyAccess.hideStatusLater = [this]() {
            m_bar->hideLater();
        };
        vocabularyAccess.showInformation = [this](
            const QString &title,
            const QString &message
        ) {
            showAttentionInformation(hostWidget(), title, message);
        };
        vocabularyAccess.showWarning = [this](
            const QString &title,
            const QString &message
        ) {
            showAttentionWarning(hostWidget(), title, message);
        };
        vocabularyAccess.flushUi = []() {
            QApplication::processEvents();
        };
        m_vocabularyQuickAdd =
            new VocabularyQuickAddController(
                vocabularyAccess,
                this
            );
        VoiceResultPresentationAccess resultAccess;
        resultAccess.applyAndSave =
            [this](const AppSettingsData &settings) {
                m_settings = settings;
                return m_access.applyAndSave
                    ? m_access.applyAndSave(settings)
                    : false;
            };
        resultAccess.runModel = [this](
            const VoiceRunContext &context,
            const QString &modelOverride,
            const QString &extraInstruction,
            QString *error,
            const std::function<void(const QString &)> &onDelta
        ) {
            if (!m_runLifecycle) {
                if (error) {
                    *error = tr8(
                        "运行生命周期控制器尚未初始化。"
                    );
                }
                return QString();
            }
            return m_runLifecycle->runModel(
                context,
                modelOverride,
                extraInstruction,
                error,
                onDelta
            );
        };
        resultAccess.finalizeOutput = [this](
            const VoiceRunContext &context,
            const QString &output
        ) {
            return m_runLifecycle
                ? m_runLifecycle->finalizeOutput(context, output)
                : output;
        };
        resultAccess.saveHistory = [this](
            const VoiceResultPresentationHistoryRequest &request
        ) {
            if (!m_runLifecycle) {
                return;
            }
            m_runLifecycle->saveHistory(
                request.context,
                request.output,
                request.error,
                request.draft,
                request.modelOverride
            );
        };
        resultAccess.addVocabulary = [this](
            const QString &source,
            const QString &modeId,
            const QString &edited
        ) {
            if (m_vocabularyQuickAdd) {
                m_vocabularyQuickAdd->addText(
                    source,
                    modeId,
                    edited
                );
            }
        };
        resultAccess.notifySettingsChanged = [this]() {
            if (m_host) {
                m_host->notifySettingsChangedForVoiceController();
            }
        };
        resultAccess.showInformation = [this](
            const QString &title,
            const QString &message
        ) {
            showAttentionInformation(hostWidget(), title, message);
        };
        resultAccess.showError = [this](const QString &message) {
            showError(message);
        };
        resultAccess.setTimedStatus = [this](
            const QString &title,
            const QString &detail
        ) {
            setTimedStatus(title, detail);
        };
        resultAccess.writeText = [this](
            const QString &text,
            bool replaceSelection,
            bool hasSelection
        ) {
            return ClipboardWriter::pasteTextToWindowChecked(
                text,
                m_functionCommands
                    ? m_functionCommands->targetWindow()
                    : nullptr,
                replaceSelection,
                hasSelection
            );
        };
        resultAccess.targetWindow = [this]() {
            return m_functionCommands
                ? m_functionCommands->targetWindow()
                : nullptr;
        };
        resultAccess.processing = [this]() {
            return m_processing;
        };
        resultAccess.processingChanged = [this](bool processing) {
            m_processing = processing;
        };
        resultAccess.cancelActiveModel = [this]() {
            if (m_runLifecycle) {
                m_runLifecycle->cancelActiveModel();
            }
        };
        resultAccess.lastModelRunCancelled = [this]() {
            return m_runLifecycle
                && m_runLifecycle->lastModelRunCancelled();
        };
        m_resultPresentation =
            new VoiceResultPresentationController(
                resultAccess,
                m_bar,
                &m_runSession,
                this
            );
        m_functionCommands =
            new FunctionCommandController(
                functionCommandAccess(),
                this
            );
        m_functionExecutionPipeline.setAccess(
            functionExecutionAccess()
        );
        reload();
    }

    void reload()
    {
        m_settings = m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
        if (m_screenshotWorkflow) {
            m_screenshotWorkflow->updateConfiguration(
                m_settings,
                loadSecrets()
            );
        }
        if (m_recordingWorkflow) {
            m_recordingWorkflow->updateConfiguration(m_settings);
        }
        if (m_resultPresentation) {
            m_resultPresentation->updateConfiguration(m_settings);
        }
        if (m_runLifecycle) {
            m_runLifecycle->updateConfiguration(m_settings);
        }
        if (m_vocabularyQuickAdd) {
            m_vocabularyQuickAdd->updateConfiguration(m_settings);
        }
        if (m_functionCommands) {
            m_functionCommands->updateConfiguration(m_settings);
        }
    }

    void setActiveHoldFunctions(const QSet<QString> &ids)
    {
        if (m_recordingWorkflow) {
            m_recordingWorkflow->setActiveHoldFunctions(ids);
        }
    }

    void handleHotkeyPressed(const QString &id)
    {
        if (m_functionCommands) {
            m_functionCommands->handleHotkeyPressed(id);
        }
    }

    void handleHotkeyReleased(const QString &id)
    {
        if (m_functionCommands) {
            m_functionCommands->handleHotkeyReleased(id);
        }
    }

    void handleScreenshotTrigger(const QString &id)
    {
        if (m_functionCommands) {
            m_functionCommands->handleScreenshotTrigger(id);
        }
    }

    void handleScreenshotLauncherTrigger(
        const QString &id,
        FunctionFlowTargetWindowHandle targetWindow)
    {
        if (m_functionCommands) {
            m_functionCommands->handleScreenshotLauncherTrigger(
                id,
                static_cast<FunctionCommandWindowHandle>(
                    targetWindow
                )
            );
        }
    }

    bool beginVoiceForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion)
    {
        if (m_recordingWorkflow
            && m_recordingWorkflow->beginForFlow(
                run,
                node,
                completion
            )) {
            return true;
        }
        if (completion) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error.code = QStringLiteral("flow_voice_failed");
            completion(result);
        }
        return false;
    }

    bool beginScreenshotForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion)
    {
        if (m_screenshotWorkflow) {
            return m_screenshotWorkflow->beginForFlow(
                run,
                node,
                completion
            );
        }
        if (completion) {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error.code =
                QStringLiteral("flow_screenshot_failed");
            completion(result);
        }
        return false;
    }

    SelectedTextWorkflowResult readSelectedTextForFlow(
        const SelectedTextWorkflowRequest &request) const
    {
        return m_selectedTextWorkflow
            ? m_selectedTextWorkflow->execute(request)
            : SelectedTextWorkflowResult();
    }

    void addVocabularyForFlow(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText)
    {
        if (m_vocabularyQuickAdd) {
            m_vocabularyQuickAdd->addText(
                sourceText,
                scopeId,
                editedText
            );
        }
    }

    VocabularySuggestion suggestVocabularyEntry(
        const QString &sourceText,
        const QString &scopeId,
        QString *error,
        const QString &editedText = QString(),
        const QString &extraContext = QString()
    )
    {
        if (m_vocabularyQuickAdd) {
            return m_vocabularyQuickAdd->suggest(
                sourceText,
                scopeId,
                error,
                editedText,
                extraContext
            );
        }
        if (error) {
            *error = tr8("词库快捷加入控制器尚未初始化。");
        }
        return VocabularySuggestion();
    }

    void handleHotkey(const QString &id)
    {
        if (m_functionCommands) {
            m_functionCommands->handleHotkey(id);
        }
    }

private:
    QWidget *hostWidget() const
    {
        return m_host ? m_host->voiceControllerHostWidget() : nullptr;
    }

    FunctionCommandAccess functionCommandAccess()
    {
        FunctionCommandAccess access;
        access.log = [](
            const QString &action,
            const QString &detail,
            qint64 elapsedMs
        ) {
            logRuntimeEvent(
                tr8("快捷键"),
                action,
                detail,
                elapsedMs
            );
        };
        access.showHub = [this]() {
            if (m_host) {
                m_host->showVoiceAssistantHub();
            }
        };
        access.screenshotActive = [this]() {
            return m_screenshotWorkflow
                && m_screenshotWorkflow->isActive();
        };
        access.processing = [this]() {
            return m_processing
                || (m_access.flowProcessing
                    && m_access.flowProcessing());
        };
        access.classicProcessing = [this]() {
            return m_processing;
        };
        access.setStatus = [this](
            const QString &title,
            const QString &detail
        ) {
            m_bar->setStatus(title, detail);
        };
        access.setTimedStatus = [this](
            const QString &title,
            const QString &detail
        ) {
            setTimedStatus(title, detail);
        };
        access.showInformation = [this](
            const QString &title,
            const QString &message
        ) {
            showAttentionInformation(hostWidget(), title, message);
        };
        access.showError = [this](const QString &message) {
            showError(message);
        };
        access.beginAction = [this]() {
            m_runSession.beginAction();
        };
        access.restartTimer = [this]() {
            m_runSession.restartTimer();
        };
        access.elapsedMs = [this]() {
            return currentActionElapsedMs();
        };
        access.captureTargetWindow = []() {
            return captureForegroundFunctionCommandWindow();
        };
        access.startPublishedFlow =
            m_access.startPublishedFlow;
        access.releasePublishedFlowHold =
            [this](const QString &id) {
                return m_recordingWorkflow
                    && m_recordingWorkflow
                        ->handleFlowHotkeyReleased(id);
            };
        access.recordingOwnsPress =
            [this](const QString &id) {
                return m_recordingWorkflow
                    && m_recordingWorkflow->ownsPress(id);
            };
        access.recordingConsumesPress =
            [this](const QString &id) {
                return m_recordingWorkflow
                    && m_recordingWorkflow->handleHotkey(id);
            };
        access.recordingConsumesRelease =
            [this](const QString &id) {
                return m_recordingWorkflow
                    && m_recordingWorkflow->handleHotkeyReleased(id);
            };
        access.recordingBusy = [this]() {
            return m_recordingWorkflow
                && m_recordingWorkflow->isBusy();
        };
        access.addVocabulary = [this](
            FunctionCommandWindowHandle targetWindow,
            bool recordingBusy
        ) {
            if (m_vocabularyQuickAdd) {
                m_vocabularyQuickAdd->handleHotkey(
                    static_cast<SelectedTextNativeWindowHandle>(
                        targetWindow
                    ),
                    recordingBusy
                );
            }
        };
        access.beginScreenshot = [this](
            const QString &functionId,
            bool targetAlreadyRemembered,
            bool externalBusy
        ) {
            ScreenshotWorkflowStartRequest request;
            request.functionId = functionId;
            request.targetAlreadyRemembered =
                targetAlreadyRemembered;
            request.externalBusy = externalBusy;
            if (m_screenshotWorkflow) {
                return m_screenshotWorkflow->start(request);
            }
            return false;
        };
        access.readSelectedText =
            [this](const SelectedTextWorkflowRequest &request) {
                return m_selectedTextWorkflow
                    ? m_selectedTextWorkflow->execute(request)
                    : SelectedTextWorkflowResult();
            };
        access.processText = [this](
            const QString &functionId,
            const QString &text
        ) {
            processTextOnly(functionId, text);
        };
        access.processVoice = [this](
            const QString &functionId,
            const QString &text
        ) {
            processRecognizedSpeech(functionId, text);
        };
        access.speechConfigurationError =
            [](const QString &provider) {
                return speechRecognitionProviderConfigurationError(
                    provider
                );
            };
        access.beginRecording = [this](
            const QString &functionId
        ) {
            if (m_recordingWorkflow) {
                m_recordingWorkflow->begin(functionId);
            }
        };
        access.prepareFloatingBar = [this](
            bool enabled,
            int autoHideMsec,
            const QString &style
        ) {
            m_bar->setStyle(style);
            m_bar->setEnabledVisible(enabled);
            m_bar->setSuppressed(autoHideMsec <= 0);
            m_bar->setAutoHideMsec(autoHideMsec);
        };
        return access;
    }

    FunctionNetworkPolicies networkPoliciesFor(
        const QString &id
    ) const
    {
        return m_settings.function(id).network;
    }

    QString functionTitle(const QString &id) const
    {
        return functionDisplayTitle(
            m_settings,
            id,
            tr8("自定义功能")
        );
    }

    PromptRuntimeSnapshot promptRuntimeSnapshot() const
    {
        PromptRuntimeSnapshot snapshot;
        if (m_access.promptSnapshotProvider) {
            snapshot = m_access.promptSnapshotProvider();
        } else {
            snapshot.settings = m_settings;
        }
        return snapshot;
    }

    QString elapsedStatusText() const
    {
        const qint64 elapsed = qMax<qint64>(0, currentActionElapsedMs());
        if (elapsed < 1000) {
            return tr8("已用时 ") + QString::number(elapsed) + tr8(" ms");
        }
        return tr8("已用时 ") + QString::number(elapsed / 1000.0, 'f', 1) + tr8(" 秒");
    }

    void setTimedStatus(const QString &title, const QString &detail)
    {
        m_bar->setStatus(title, detail.trimmed().isEmpty()
            ? elapsedStatusText()
            : detail + tr8(" · ") + elapsedStatusText());
    }

    QString applyVocabularyPreCorrection(const QString &text, const QString &modeId, const QString &sourceName)
    {
        return applyVocabularyPreCorrection(text, modeId, sourceName, false);
    }

    QString applyVocabularyPreCorrection(const QString &text, const QString &modeId, const QString &sourceName, bool hasVoiceInput)
    {
        const QString corrected =
            applyVocabularyPreCorrectionForRun(
                m_settings,
                text,
                modeId,
                hasVoiceInput
            );
        if (corrected != text) {
            logRuntimeEvent(
                tr8("词库"),
                tr8("预修正"),
                tr8("来源=") + sourceName
                    + QStringLiteral("，功能=") + modeId
                    + QStringLiteral("，原字数=") + QString::number(text.size())
                    + QStringLiteral("，修正后字数=") + QString::number(corrected.size()),
                currentActionElapsedMs()
            );
        }
        return corrected;
    }

    VoiceFunctionExecutionAccess functionExecutionAccess()
    {
        VoiceFunctionExecutionAccess access;
        access.selectedText = [this]() {
            return m_functionCommands
                ? m_functionCommands->selectedText()
                : QString();
        };
        access.networkPoliciesFor = [this](const QString &modeId) {
            return networkPoliciesFor(modeId);
        };
        access.preCorrect = [this](
            const QString &text,
            const QString &modeId,
            const QString &sourceLabel,
            bool hasVoiceInput
        ) {
            const QString correctedText = applyVocabularyPreCorrection(
                text,
                modeId,
                sourceLabel,
                hasVoiceInput
            );
            if (!hasVoiceInput) {
                logRuntimeEvent(
                    tr8("功能"),
                    tr8("文本处理开始"),
                    QStringLiteral("功能=") + modeId
                        + QStringLiteral("，输入字数=")
                        + QString::number(correctedText.size()),
                    currentActionElapsedMs()
                );
            }
            return correctedText;
        };
        access.enrichContext = [this](VoiceRunContext *context) {
            if (m_screenshotWorkflow) {
                m_screenshotWorkflow->applyPendingContext(context);
            }
            if (context) {
                m_runSession.setRunContext(*context);
            }
        };
        access.shouldStream = [this](const VoiceRunContext &context) {
            return m_resultPresentation
                && m_resultPresentation->shouldStream(context);
        };
        access.stream = [this](const VoiceRunContext &context) {
            if (m_resultPresentation) {
                m_resultPresentation->stream(context);
            }
        };
        access.processingStarted = [this](
            const QString &status,
            const VoiceRunContext &context
        ) {
            setTimedStatus(status, functionTitle(context.modeId));
        };
        access.completionHandlers = [this]() {
            return m_resultPresentation
                ? m_resultPresentation->completionHandlers()
                : VoiceResultCompletionHandlers();
        };
        access.contextUpdated = [this](const VoiceRunContext &context) {
            m_runSession.setRunContext(context);
        };
        access.failed = [this](
            const VoiceRunContext &context,
            const VoiceResultCompletionResult &completion
        ) {
            if (m_resultPresentation) {
                m_resultPresentation->fail(context, completion);
            }
        };
        access.completed = [this](
            const VoiceRunContext &context,
            const QString &finalOutput
        ) {
            if (m_resultPresentation) {
                m_resultPresentation->present(
                    context,
                    finalOutput
                );
            }
        };
        return access;
    }

    void processRecognizedSpeech(
        const QString &modeId,
        const QString &rawAsr
    )
    {
        const bool willCallModel = modeId != QStringLiteral("dictate")
            || m_settings.dictatePolishEnabled;
        VoiceFunctionExecutionRequest request;
        request.modeId = modeId;
        request.inputText = rawAsr;
        request.kind = VoiceInputProcessingKind::Voice;
        request.sourceLabel = tr8("语音识别");
        request.failureStage = tr8("大模型");
        request.processingStatus =
            willCallModel ? tr8("模型处理中") : tr8("整理中");
        m_functionExecutionPipeline.execute(request);
    }

    void processTextOnly(const QString &modeId, const QString &text)
    {
        if (m_processing) {
            setTimedStatus(tr8("正在处理"), tr8("请等待当前任务完成。"));
            return;
        }
        ProcessingGuard processingGuard(m_processing);

        m_runSession.beginTextInput();
        VoiceFunctionExecutionRequest request;
        request.modeId = modeId;
        request.inputText = text;
        request.kind = VoiceInputProcessingKind::TextOnly;
        request.sourceLabel = tr8("文本输入");
        request.failureStage = tr8("文本处理");
        request.processingStatus = tr8("模型处理中");
        m_functionExecutionPipeline.execute(request);
    }

    void showError(const QString &message)
    {
        const QString text = message.trimmed().isEmpty() ? tr8("发生未知错误。") : message;
        logRuntimeEvent(tr8("错误弹窗"), tr8("显示"), text, currentActionElapsedMs());
        m_bar->setStatus(tr8("处理失败"), text);
        m_bar->hideLater();
        showAttentionWarning(hostWidget(), tr8("处理失败"), text);
    }

    qint64 currentActionElapsedMs() const
    {
        return m_runSession.elapsedMs();
    }

    VoiceControllerAccess m_access;
    AppSettingsData m_settings;
    FloatingBar *m_bar;
    VoiceControllerHost *m_host;
    SelectedTextWorkflowController *m_selectedTextWorkflow = nullptr;
    ScreenshotWorkflowController *m_screenshotWorkflow = nullptr;
    VocabularyQuickAddController *m_vocabularyQuickAdd = nullptr;
    VoiceRecordingWorkflowController *m_recordingWorkflow = nullptr;
    VoiceRunLifecycleController *m_runLifecycle = nullptr;
    VoiceResultPresentationController *m_resultPresentation = nullptr;
    FunctionCommandController *m_functionCommands = nullptr;
    VoiceFunctionExecutionPipeline m_functionExecutionPipeline;
    bool m_processing = false;
    VoiceRunSession m_runSession;
};


VoiceController::VoiceController(
    const VoiceControllerAccess &access,
    FloatingBar *bar,
    VoiceControllerHost *host,
    QObject *parent
)
    : QObject(parent), d(new Impl(access, bar, host, this))
{
}

VoiceController::~VoiceController()
{
    delete d;
    d = nullptr;
}

void VoiceController::reload()
{
    d->reload();
}

void VoiceController::setActiveHoldFunctions(const QSet<QString> &ids)
{
    d->setActiveHoldFunctions(ids);
}

void VoiceController::handleHotkeyPressed(const QString &id)
{
    d->handleHotkeyPressed(id);
}

void VoiceController::handleHotkeyReleased(const QString &id)
{
    d->handleHotkeyReleased(id);
}

void VoiceController::handleScreenshotTrigger(const QString &id)
{
    d->handleScreenshotTrigger(id);
}

void VoiceController::handleScreenshotLauncherTrigger(
    const QString &id,
    FunctionFlowTargetWindowHandle targetWindow)
{
    d->handleScreenshotLauncherTrigger(id, targetWindow);
}

bool VoiceController::beginVoiceForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion)
{
    return d->beginVoiceForFlow(run, node, completion);
}

bool VoiceController::beginScreenshotForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion)
{
    return d->beginScreenshotForFlow(run, node, completion);
}

SelectedTextWorkflowResult
VoiceController::readSelectedTextForFlow(
    const SelectedTextWorkflowRequest &request) const
{
    return d->readSelectedTextForFlow(request);
}

void VoiceController::addVocabularyForFlow(
    const QString &sourceText,
    const QString &scopeId,
    const QString &editedText)
{
    d->addVocabularyForFlow(
        sourceText,
        scopeId,
        editedText
    );
}

VocabularySuggestion VoiceController::suggestVocabularyEntry(
    const QString &sourceText,
    const QString &scopeId,
    QString *error,
    const QString &editedText,
    const QString &extraContext
)
{
    return d->suggestVocabularyEntry(sourceText, scopeId, error, editedText, extraContext);
}

void VoiceController::handleHotkey(const QString &id)
{
    d->handleHotkey(id);
}
