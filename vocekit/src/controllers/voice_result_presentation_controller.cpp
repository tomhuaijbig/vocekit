#include "voice_result_presentation_controller.h"

#include "../capture/screenshot_result_window.h"
#include "../config/app_settings_defaults.h"
#include "../domain/function_catalog.h"
#include "../domain/voice_result_rerun_executor.h"
#include "../domain/voice_result_stream_executor.h"
#include "../domain/voice_run_session.h"
#include "../output/voice_result_output_dispatcher.h"
#include "../output/voice_result_popup_builder.h"
#include "../providers/model_catalog.h"
#include "../runtime_log.h"
#include "../tasks/model_request_task.h"
#include "../ui/floating_bar.h"
#include "../ui/result_choice_popup.h"

#include <QApplication>
#include <QPointer>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

class ProcessingCallbackGuard
{
public:
    explicit ProcessingCallbackGuard(
        const std::function<void(bool)> &callback
    )
        : m_callback(callback)
    {
        if (m_callback) {
            m_callback(true);
        }
    }

    ~ProcessingCallbackGuard()
    {
        if (m_callback) {
            m_callback(false);
        }
    }

private:
    std::function<void(bool)> m_callback;
};

} // namespace

class VoiceResultPresentationController::Impl : public QObject
{
public:
    Impl(
        const VoiceResultPresentationAccess &access,
        FloatingBar *bar,
        VoiceRunSession *runSession,
        QObject *parent
    )
        : QObject(parent),
          m_access(access),
          m_bar(bar),
          m_runSession(runSession)
    {
    }

    ~Impl() override
    {
        cancelActiveModel();
    }

    void updateConfiguration(const AppSettingsData &settings)
    {
        m_settings = settings;
    }

    bool shouldStream(const VoiceRunContext &context) const
    {
        if (functionSettings(context.modeId).output.outputMode
            != outputModePopup()) {
            return false;
        }
        if (context.modeId == QStringLiteral("dictate")) {
            return m_settings.dictatePolishEnabled
                && isModelProviderAvailableForTask(
                    modelFor(QStringLiteral("dictate"))
                );
        }
        return true;
    }

    VoiceResultCompletionHandlers completionHandlers() const
    {
        VoiceResultCompletionHandlers handlers;
        handlers.runContext = [this](
            const VoiceRunContext &context,
            QString *error
        ) {
            return runModel(
                context,
                QString(),
                QString(),
                error
            );
        };
        handlers.finalizeOutput = [this](
            const VoiceRunContext &context,
            const QString &output
        ) {
            return finalizeOutput(context, output);
        };
        handlers.wasCancelled = [this]() {
            return lastModelRunCancelled();
        };
        return handlers;
    }

    void stream(const VoiceRunContext &context)
    {
        auto *popup = new ResultChoicePopup(
            popupWindowPreferences(),
            functionTitle(context.modeId),
            QString(),
            targetWindow(),
            !context.selectedText.trimmed().isEmpty(),
            0
        );
        configurePopupActions(popup, context);
        popup->setBusy(true, text8("正在生成"));
        popup->showNearBottom();

        setTimedStatus(
            text8("模型处理中"),
            text8("正在流式生成结果")
        );
        QApplication::processEvents();

        QPointer<ResultChoicePopup> guardedPopup(popup);
        VoiceResultStreamRequest request;
        request.context = context;
        request.onDelta = [guardedPopup](const QString &delta) {
            if (guardedPopup) {
                guardedPopup->appendResultText(delta);
                QApplication::processEvents();
            }
        };

        const VoiceResultStreamResult result =
            VoiceResultStreamExecutor::run(
                request,
                streamHandlers()
            );
        if (!result.ok) {
            if (result.cancelled) {
                if (guardedPopup) {
                    guardedPopup->setBusy(false, text8("已取消"));
                }
                logRuntimeEvent(
                    text8("功能"),
                    result.logAction,
                    result.logDetail,
                    elapsedMs()
                );
                hideFloatingBar();
                return;
            }
            if (guardedPopup) {
                guardedPopup->setBusy(false, text8("生成失败"));
            }
            logRuntimeEvent(
                text8("功能"),
                result.logAction,
                result.logDetail,
                elapsedMs()
            );
            showError(result.error);
            saveHistory(context, QString(), result.error);
            return;
        }

        if (guardedPopup) {
            guardedPopup->setResultText(
                popupPresentation(context, result.finalOutput).text
            );
            guardedPopup->setBusy(false, text8("已生成"));
        }
        saveHistory(context, result.finalOutput, QString());
        if (m_bar) {
            m_bar->setResult(text8("处理完成"), result.finalOutput);
            m_bar->hideLater();
        }
        logRuntimeEvent(
            text8("功能"),
            result.logAction,
            result.logDetail,
            elapsedMs()
        );
    }

    void fail(
        const VoiceRunContext &context,
        const VoiceResultCompletionResult &completion
    )
    {
        if (completion.cancelled) {
            logRuntimeEvent(
                text8("功能"),
                completion.logAction,
                completion.logDetail,
                elapsedMs()
            );
            hideFloatingBar();
            return;
        }
        logRuntimeEvent(
            text8("功能"),
            completion.logAction,
            completion.logDetail,
            elapsedMs()
        );
        showError(completion.error);
        saveHistory(context, QString(), completion.error);
    }

    void present(
        const VoiceRunContext &context,
        const QString &finalOutput
    )
    {
        saveHistory(context, finalOutput, QString());
        if (m_bar) {
            m_bar->setResult(text8("处理完成"), finalOutput);
        }

        VoiceResultOutputRequest request;
        request.modeId = context.modeId;
        request.outputMode =
            functionSettings(context.modeId).output.outputMode;
        request.finalOutput = finalOutput;
        request.screenshotInput = context.screenshotInput;
        request.hasSelectedText =
            !context.selectedText.trimmed().isEmpty();
        const VoiceResultOutputDispatch dispatch =
            VoiceResultOutputDispatcher::plan(request);
        const ResultOutputPlan plan = dispatch.routePlan;

        logRuntimeEvent(
            text8("功能"),
            text8("完成"),
            dispatch.completionLogDetail,
            elapsedMs()
        );

        if (plan.destination == ResultOutputDestination::AutoWrite) {
            setTimedStatus(plan.progressTitle, plan.progressMessage);
            writeText(
                finalOutput,
                true,
                plan.replaceSelectedText
            );
            setTimedStatus(plan.doneTitle, plan.doneMessage);
            logRuntimeEvent(
                text8("写入"),
                plan.logAction,
                dispatch.autoWriteLogDetail,
                elapsedMs()
            );
            hideFloatingBar();
            return;
        }

        hideFloatingBar();
        if (plan.destination == ResultOutputDestination::ScreenshotPanel) {
            showScreenshotResultWindow(context, finalOutput);
            return;
        }
        showResultPopup(context, finalOutput);
    }

private:
    const FunctionSettings &functionSettings(const QString &id) const
    {
        return m_settings.function(id);
    }

    QString functionTitle(const QString &id) const
    {
        return functionDisplayTitle(
            m_settings,
            id,
            text8("自定义功能")
        );
    }

    QString modelFor(const QString &id) const
    {
        return functionSettings(id).modelId;
    }

    qint64 elapsedMs() const
    {
        return m_runSession ? m_runSession->elapsedMs() : -1;
    }

    void cancelActiveModel() const
    {
        if (m_access.cancelActiveModel) {
            m_access.cancelActiveModel();
        }
    }

    bool lastModelRunCancelled() const
    {
        return m_access.lastModelRunCancelled
            && m_access.lastModelRunCancelled();
    }

    ClipboardWindowHandle targetWindow() const
    {
        return m_access.targetWindow
            ? m_access.targetWindow()
            : nullptr;
    }

    void setTimedStatus(
        const QString &title,
        const QString &detail
    ) const
    {
        if (m_access.setTimedStatus) {
            m_access.setTimedStatus(title, detail);
        }
    }

    void showError(const QString &error) const
    {
        if (m_access.showError) {
            m_access.showError(error);
        }
    }

    void writeText(
        const QString &text,
        bool replaceSelection,
        bool hasSelection
    ) const
    {
        if (m_access.writeText) {
            m_access.writeText(
                text,
                replaceSelection,
                hasSelection
            );
        }
    }

    void hideFloatingBar() const
    {
        if (m_bar) {
            m_bar->hideLater();
        }
    }

    QString runModel(
        const VoiceRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction,
        QString *error,
        const std::function<void(const QString &)> &onDelta =
            std::function<void(const QString &)>()
    ) const
    {
        if (!m_access.runModel) {
            if (error) {
                *error = text8("结果展示控制器未配置模型执行能力。");
            }
            return QString();
        }
        return m_access.runModel(
            context,
            modelOverride,
            extraInstruction,
            error,
            onDelta
        );
    }

    QString finalizeOutput(
        const VoiceRunContext &context,
        const QString &output
    ) const
    {
        return m_access.finalizeOutput
            ? m_access.finalizeOutput(context, output)
            : output;
    }

    void saveHistory(
        const VoiceRunContext &context,
        const QString &output,
        const QString &error,
        bool draft = false,
        const QString &modelOverride = QString()
    ) const
    {
        if (!m_access.saveHistory) {
            return;
        }
        VoiceResultPresentationHistoryRequest request;
        request.context = context;
        request.output = output;
        request.error = error;
        request.draft = draft;
        request.modelOverride = modelOverride;
        m_access.saveHistory(request);
    }

    ResultPopupWindowPreferences popupWindowPreferences() const
    {
        ResultPopupWindowPreferences preferences;
        preferences.opacityPercent = m_settings.resultPopupOpacity;
        preferences.hasGeometry =
            m_settings.windows.hasResultPopupGeometry;
        preferences.geometry =
            m_settings.windows.resultPopupGeometry;
        return preferences;
    }

    void savePopupGeometry(const QRect &geometry)
    {
        m_settings.windows.hasResultPopupGeometry = geometry.isValid();
        m_settings.windows.resultPopupGeometry = geometry;
        if (m_access.applyAndSave) {
            m_access.applyAndSave(m_settings);
        }
    }

    void saveScreenshotWindow(
        const QRect &geometry,
        int opacity
    )
    {
        m_settings.windows.hasScreenshotResultGeometry =
            geometry.isValid();
        m_settings.windows.screenshotResultGeometry = geometry;
        m_settings.windows.screenshotResultOpacity = opacity;
        if (m_access.applyAndSave) {
            m_access.applyAndSave(m_settings);
        }
    }

    VoiceResultPopupPresentation popupPresentation(
        const VoiceRunContext &context,
        const QString &output,
        const QString &modelOverride = QString()
    ) const
    {
        const QString finalModel =
            modelOverride.trimmed().isEmpty()
                ? modelFor(context.modeId)
                : modelOverride.trimmed();
        VoiceResultPopupBuildRequest request;
        request.context = context;
        request.output = output;
        request.templateId =
            functionSettings(context.modeId).output.resultTemplate;
        request.functionTitle = functionTitle(context.modeId);
        request.modelId = finalModel;
        request.modelTitle = modelTitle(finalModel);
        request.elapsedMs = elapsedMs();
        request.timeoutMs =
            functionSettings(context.modeId).output.resultPopupSeconds
            * 1000;
        return VoiceResultPopupBuilder::build(request);
    }

    VoiceResultRerunHandlers rerunHandlers() const
    {
        VoiceResultRerunHandlers handlers;
        handlers.runContext = [this](
            const VoiceRunContext &context,
            const QString &modelOverride,
            const QString &extraInstruction,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            return runModel(
                context,
                modelOverride,
                extraInstruction,
                error,
                onDelta
            );
        };
        handlers.finalizeOutput = [this](
            const VoiceRunContext &context,
            const QString &output
        ) {
            return finalizeOutput(context, output);
        };
        handlers.wasCancelled = [this]() {
            return lastModelRunCancelled();
        };
        return handlers;
    }

    VoiceResultStreamHandlers streamHandlers() const
    {
        VoiceResultStreamHandlers handlers;
        handlers.runContext = [this](
            const VoiceRunContext &context,
            QString *error,
            const VoiceRunDeltaCallback &onDelta
        ) {
            return runModel(
                context,
                QString(),
                QString(),
                error,
                onDelta
            );
        };
        handlers.finalizeOutput = [this](
            const VoiceRunContext &context,
            const QString &output
        ) {
            return finalizeOutput(context, output);
        };
        handlers.wasCancelled = [this]() {
            return lastModelRunCancelled();
        };
        return handlers;
    }

    void configurePopupActions(
        ResultChoicePopup *popup,
        const VoiceRunContext &context
    )
    {
        if (!popup) {
            return;
        }

        QPointer<ResultChoicePopup> guardedPopup(popup);
        popup->setCurrentModel(modelFor(context.modeId));
        popup->setActionOrder(
            functionSettings(context.modeId).output.resultActions
        );
        popup->setActionCallbacks(
            [this, guardedPopup, context]() {
                rerunPopup(
                    guardedPopup,
                    context,
                    QString(),
                    QString()
                );
            },
            [this, guardedPopup, context](const QString &model) {
                rerunPopup(
                    guardedPopup,
                    context,
                    model,
                    QString()
                );
            },
            [this, guardedPopup, context](
                const QString &followUp
            ) {
                rerunPopup(
                    guardedPopup,
                    context,
                    QString(),
                    followUp
                );
            }
        );
        popup->setCancellationCallback([this]() {
            cancelActiveModel();
        });
        popup->setDraftCallback(
            [this, guardedPopup, context](
                const QString &draftText
            ) {
                saveHistory(
                    context,
                    draftText,
                    QString(),
                    true,
                    guardedPopup
                        ? guardedPopup->currentModel()
                        : QString()
                );
                if (m_access.notifySettingsChanged) {
                    m_access.notifySettingsChanged();
                }
            }
        );
        popup->setVocabularyCallback(
            [this, context](
                const QString &before,
                const QString &after
            ) {
                const QString sourceText =
                    before.trimmed().isEmpty()
                        ? after.trimmed()
                        : before.trimmed();
                const QString editedText =
                    after.trimmed() == sourceText
                        ? QString()
                        : after.trimmed();
                if (sourceText.isEmpty() && editedText.isEmpty()) {
                    if (m_access.showInformation) {
                        m_access.showInformation(
                            text8("缺少文字"),
                            text8("结果小框里没有可加入词库的文字。")
                        );
                    }
                    return;
                }
                if (m_access.addVocabulary) {
                    m_access.addVocabulary(
                        sourceText,
                        context.modeId,
                        editedText
                    );
                }
            }
        );
        popup->setWindowPreferenceCallback(
            [this](const QRect &geometry) {
                savePopupGeometry(geometry);
            }
        );
    }

    void rerunPopup(
        const QPointer<ResultChoicePopup> &popup,
        const VoiceRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction
    )
    {
        if (!popup) {
            return;
        }

        if (m_runSession) {
            m_runSession->beginModelAttempt();
        }
        popup->setBusy(
            true,
            extraInstruction.trimmed().isEmpty()
                ? text8("正在重新生成")
                : text8("正在继续处理")
        );
        popup->setResultText(QString());
        setTimedStatus(
            text8("模型处理中"),
            text8("正在调用模型生成结果")
        );
        QApplication::processEvents();

        VoiceResultRerunRequest request;
        request.context = context;
        request.modelOverride = modelOverride;
        request.extraInstruction = extraInstruction;
        request.defaultModel = modelFor(context.modeId);
        request.elapsedMs = elapsedMs();
        request.onDelta = [popup](const QString &delta) {
            if (popup) {
                popup->appendResultText(delta);
                QApplication::processEvents();
            }
        };

        const VoiceResultRerunResult result =
            VoiceResultRerunExecutor::run(
                request,
                rerunHandlers()
            );
        if (!result.ok) {
            if (result.cancelled) {
                if (popup) {
                    popup->setBusy(false, text8("已取消"));
                }
                logRuntimeEvent(
                    text8("功能"),
                    result.logAction,
                    result.logDetail,
                    elapsedMs()
                );
                hideFloatingBar();
                return;
            }
            if (popup) {
                popup->setBusy(false, text8("生成失败"));
            }
            logRuntimeEvent(
                text8("功能"),
                result.logAction,
                result.logDetail,
                elapsedMs()
            );
            showError(result.error);
            return;
        }

        if (popup) {
            popup->setCurrentModel(result.finalModel);
            popup->setResultText(
                popupPresentation(
                    context,
                    result.finalOutput,
                    result.finalModel
                ).text
            );
            popup->setBusy(false, text8("已生成"));
        }
        saveHistory(
            context,
            result.finalOutput,
            QString(),
            false,
            result.finalModel
        );
        if (m_bar) {
            m_bar->setResult(text8("处理完成"), result.finalOutput);
            m_bar->hideLater();
        }
        logRuntimeEvent(
            text8("功能"),
            result.logAction,
            result.logDetail,
            elapsedMs()
        );
    }

    QVector<QPair<QString, QString>> screenshotModelOptions() const
    {
        QVector<QPair<QString, QString>> options;
        for (const ModelOption &option : modelOptions()) {
            options.append(qMakePair(option.id, option.title));
        }
        return options;
    }

    void rerunScreenshotWindow(
        const QPointer<ScreenshotResultWindow> &window,
        const VoiceRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction
    )
    {
        if (!window
            || (m_access.processing && m_access.processing())) {
            return;
        }

        ProcessingCallbackGuard guard(
            m_access.processingChanged
        );
        if (m_runSession) {
            m_runSession->beginModelAttempt();
        }
        window->setBusy(
            true,
            extraInstruction.trimmed().isEmpty()
                ? text8("正在重新生成")
                : text8("正在继续处理")
        );
        window->setResultText(QString());
        setTimedStatus(
            text8("模型处理中"),
            text8("正在生成截图处理结果")
        );
        QApplication::processEvents();

        VoiceResultRerunRequest request;
        request.context = context;
        request.modelOverride = modelOverride;
        request.extraInstruction = extraInstruction;
        request.defaultModel = modelFor(context.modeId);
        request.elapsedMs = elapsedMs();
        request.onDelta = [window](const QString &delta) {
            if (window) {
                window->appendResultText(delta);
                QApplication::processEvents();
            }
        };

        const VoiceResultRerunResult result =
            VoiceResultRerunExecutor::run(
                request,
                rerunHandlers()
            );
        if (!result.ok) {
            if (result.cancelled) {
                if (window) {
                    window->setBusy(false, text8("已取消"));
                }
                logRuntimeEvent(
                    text8("截图结果"),
                    result.logAction,
                    result.logDetail,
                    elapsedMs()
                );
                hideFloatingBar();
                return;
            }
            if (window) {
                window->setBusy(false, text8("生成失败"));
            }
            logRuntimeEvent(
                text8("截图结果"),
                result.logAction,
                result.logDetail,
                elapsedMs()
            );
            showError(result.error);
            return;
        }

        if (window) {
            window->setCurrentModel(result.finalModel);
            window->setResultText(result.finalOutput);
            window->setBusy(false, text8("已生成"));
        }
        if (m_runSession) {
            m_runSession->setRunContext(context);
        }
        saveHistory(
            context,
            result.finalOutput,
            QString(),
            false,
            result.finalModel
        );
        if (m_bar) {
            m_bar->setResult(text8("处理完成"), result.finalOutput);
            m_bar->hideLater();
        }
        logRuntimeEvent(
            text8("截图结果"),
            result.logAction,
            result.logDetail,
            elapsedMs()
        );
    }

    void showScreenshotResultWindow(
        const VoiceRunContext &context,
        const QString &result
    )
    {
        const QRect savedGeometry =
            m_settings.windows.hasScreenshotResultGeometry
                ? m_settings.windows.screenshotResultGeometry
                : QRect();
        auto *window = new ScreenshotResultWindow(
            functionTitle(context.modeId),
            context.screenshotImage,
            context.screenshotBlocks,
            context.screenshotRecognizedText,
            result,
            savedGeometry,
            m_settings.windows.screenshotResultOpacity,
            functionSettings(context.modeId)
                .output.resultPopupSeconds * 1000
        );
        QPointer<ScreenshotResultWindow> guardedWindow(window);
        window->setCurrentModel(modelFor(context.modeId));
        window->setModelOptions(screenshotModelOptions());
        window->setActionCallbacks(
            [this, guardedWindow, context]() {
                rerunScreenshotWindow(
                    guardedWindow,
                    context,
                    QString(),
                    QString()
                );
            },
            [this, guardedWindow, context](
                const QString &model
            ) {
                rerunScreenshotWindow(
                    guardedWindow,
                    context,
                    model,
                    QString()
                );
            },
            [this, guardedWindow, context](
                const QString &followUp
            ) {
                rerunScreenshotWindow(
                    guardedWindow,
                    context,
                    QString(),
                    followUp
                );
            },
            [this](const QString &text) {
                writeText(text, false, false);
            },
            [this](const QString &text) {
                writeText(text, true, true);
            }
        );
        window->setDraftCallback(
            [this, context](const QString &draftText) {
                if (m_runSession) {
                    m_runSession->setRunContext(context);
                }
                saveHistory(
                    context,
                    draftText,
                    QString(),
                    true
                );
                if (m_access.notifySettingsChanged) {
                    m_access.notifySettingsChanged();
                }
            }
        );
        window->setWindowPreferenceCallback(
            [this](const QRect &geometry, int opacity) {
                saveScreenshotWindow(geometry, opacity);
            }
        );
        window->showNearBottom();
    }

    void showResultPopup(
        const VoiceRunContext &context,
        const QString &result
    )
    {
        const VoiceResultPopupPresentation presentation =
            popupPresentation(context, result);
        auto *popup = new ResultChoicePopup(
            popupWindowPreferences(),
            presentation.title,
            presentation.text,
            targetWindow(),
            presentation.hasSelectedText,
            presentation.timeoutMs
        );
        configurePopupActions(popup, context);
        popup->showNearBottom();
    }

    VoiceResultPresentationAccess m_access;
    AppSettingsData m_settings;
    FloatingBar *m_bar = nullptr;
    VoiceRunSession *m_runSession = nullptr;
};

VoiceResultPresentationController::
VoiceResultPresentationController(
    const VoiceResultPresentationAccess &access,
    FloatingBar *bar,
    VoiceRunSession *runSession,
    QObject *parent
)
    : QObject(parent),
      d(new Impl(access, bar, runSession, this))
{
}

VoiceResultPresentationController::
~VoiceResultPresentationController()
{
    delete d;
    d = nullptr;
}

void VoiceResultPresentationController::updateConfiguration(
    const AppSettingsData &settings
)
{
    d->updateConfiguration(settings);
}

bool VoiceResultPresentationController::shouldStream(
    const VoiceRunContext &context
) const
{
    return d->shouldStream(context);
}

VoiceResultCompletionHandlers
VoiceResultPresentationController::completionHandlers() const
{
    return d->completionHandlers();
}

void VoiceResultPresentationController::stream(
    const VoiceRunContext &context
)
{
    d->stream(context);
}

void VoiceResultPresentationController::fail(
    const VoiceRunContext &context,
    const VoiceResultCompletionResult &completion
)
{
    d->fail(context, completion);
}

void VoiceResultPresentationController::present(
    const VoiceRunContext &context,
    const QString &finalOutput
)
{
    d->present(context, finalOutput);
}
