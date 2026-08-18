#include "vocekit_application_runtime.h"

#include "application_events.h"
#include "selection_context_feature.h"
#include "../capture/screenshot_launcher.h"
#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../config/app_settings_store.h"
#include "../controllers/function_command_controller.h"
#include "../controllers/function_flow_execution_controller.h"
#include "../controllers/function_flow_plan_cache.h"
#include "../controllers/function_flow_publication_service.h"
#include "../controllers/function_flow_result_controller.h"
#include "../controllers/function_flow_runtime_adapters.h"
#include "../controllers/tray_controller.h"
#include "../controllers/voice_controller.h"
#include "../domain/app_legacy_types.h"
#include "../domain/function_catalog.h"
#include "../domain/function_flow_errors.h"
#include "../domain/history_record_builder.h"
#include "../domain/prompt_runtime_library.h"
#include "../input/global_hotkeys.h"
#include "../input/hotkey_definitions.h"
#include "../input/hotkey_refresh_coordinator.h"
#include "../input/hotkey_settings_snapshot.h"
#include "../input/hold_to_talk.h"
#include "../input/selected_text_reader.h"
#include "../output/clipboard_writer.h"
#include "../platform/windows_autostart.h"
#include "../providers/model_catalog.h"
#include "../runtime/function_flow_runtime_log.h"
#include "../runtime_crash_handler.h"
#include "../runtime_log.h"
#include "../storage/history_paths.h"
#include "../storage/history_record_service.h"
#include "../storage/prompt_library_store.h"
#include "../tasks/speech_recognition_task.h"
#include "../ui/attention_message.h"
#include "../ui/app_dialogs.h"
#include "../ui/chinese_text_context_menu.h"
#include "../ui/floating_bar.h"
#include "../ui/hub_settings_state.h"
#include "../ui/hub_window.h"
#include "../ui/screen_position.h"
#include "../ui/toggle_switch.h"
#include "../ui/ui_style.h"

#include <QtWidgets>
#include <QPointer>
#include <functional>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

namespace
{

QString mainShortcutForFunction(
    const AppSettingsData &settings,
    const FunctionSettings &function)
{
    if (!function.builtIn) {
        return function.shortcut.trimmed();
    }
    for (const HotkeyDef &definition : hotkeyDefs()) {
        if (definition.id == function.id) {
            const QString shortcut = settings.applicationHotkeys
                .value(definition.id, definition.defaultValue)
                .trimmed();
            return shortcut.isEmpty()
                ? definition.defaultValue
                : shortcut;
        }
    }
    return function.shortcut.trimmed();
}

void addOccupiedShortcut(
    FunctionFlowValidationContext *context,
    const QString &shortcut,
    const QString &owner)
{
    if (!context || shortcut.trimmed().isEmpty()) {
        return;
    }
    const QString key = shortcut.trimmed();
    const QString existing =
        context->occupiedShortcutOwners.value(key);
    if (existing.isEmpty()
        || (existing == context->functionId
            && owner != context->functionId)) {
        context->occupiedShortcutOwners.insert(key, owner);
    }
}

FunctionFlowValidationContext functionFlowValidationContext(
    const AppSettingsData &settings,
    const QVector<PromptLibraryItem> &libraryItems,
    const QString &functionId)
{
    FunctionFlowValidationContext context;
    context.functionId = functionId;
    for (const ModelOption &option : modelOptions()) {
        if (!option.id.trimmed().isEmpty()) {
            context.references.modelIds.append(option.id);
        }
    }

    PromptRuntimeSnapshot promptSnapshot;
    promptSnapshot.settings = settings;
    promptSnapshot.libraryItems = libraryItems;
    for (const PromptTargetInfo &target :
         promptRuntimeTargets(promptSnapshot)) {
        if (!target.id.trimmed().isEmpty()
            && !context.references.promptIds.contains(target.id)) {
            context.references.promptIds.append(target.id);
        }
    }

    context.references.speechProviderIds =
        supportedSpeechProviderIds();
    context.references.ocrEngineIds = supportedOcrEngineIds();
    context.references.defaultSpeechProviderId =
        normalizeSpeechProvider(settings.speechProvider);
    context.references.defaultOcrEngineId =
        normalizeOcrEngine(settings.ocrEngine);

    const int currentIndex = settings.functionIndex(functionId);
    if (currentIndex >= 0) {
        context.mainShortcut = mainShortcutForFunction(
            settings,
            settings.functions.at(currentIndex)
        );
    }

    for (const HotkeyDef &definition : hotkeyDefs()) {
        const QString configured = settings.applicationHotkeys
            .value(definition.id, definition.defaultValue)
            .trimmed();
        addOccupiedShortcut(
            &context,
            configured.isEmpty()
                ? definition.defaultValue
                : configured,
            definition.id
        );
    }
    for (const FunctionSettings &function : settings.functions) {
        if (!function.builtIn) {
            addOccupiedShortcut(
                &context,
                function.shortcut,
                function.id
            );
        }
        if (screenshotTriggerUsesSeparate(
                function.input.screenshotTriggerMode)) {
            QString screenshotShortcut =
                function.input.screenshotShortcut.trimmed();
            if (screenshotShortcut.isEmpty()) {
                screenshotShortcut =
                    screenshotShortcutFromFunctionShortcut(
                        mainShortcutForFunction(settings, function)
                    );
            }
            addOccupiedShortcut(
                &context,
                screenshotShortcut,
                QStringLiteral("screenshot:") + function.id
            );
        }
    }
    return context;
}

OcrEngine functionFlowOcrEngine(const QString &id)
{
    const QString normalized = id.trimmed();
    if (normalized == QStringLiteral("rapid")) {
        return OcrEngine::RapidOcr;
    }
    if (normalized == QStringLiteral("windows")) {
        return OcrEngine::WindowsOcr;
    }
    if (normalized == QStringLiteral("customCloud")) {
        return OcrEngine::CustomCloud;
    }
    if (normalized == QStringLiteral("vision")) {
        return OcrEngine::VisionModel;
    }
    return OcrEngine::Automatic;
}

HistoryRecordSaveRequest historyRequestForFunctionFlow(
    const FunctionFlowHistoryRequest &request)
{
    HistoryRecordSaveRequest save;
    save.recordDirectory = request.recordDirectory;
    save.modeId = request.functionId;
    save.modeTitle = request.functionTitle.trimmed().isEmpty()
        ? request.functionId
        : request.functionTitle;
    save.sourceAudioPath = request.sourceAudioPath;

    HistoryRecordMetadataRequest metadata;
    metadata.input = request.canonicalInput;
    metadata.output = request.pendingEditedText.isNull()
        ? request.finalOutput
        : request.pendingEditedText;
    if (!request.terminalError.isEmpty()) {
        metadata.error =
            functionFlowUserMessage(request.terminalError);
    }
    metadata.actionHadRecording =
        !request.sourceAudioPath.trimmed().isEmpty()
        || !request.recordingSegments.isEmpty();
    metadata.recordingTriggerMode =
        request.recordingTriggerMode;
    metadata.longRecording = request.longRecording;
    metadata.recordingSegments = request.recordingSegments;
    metadata.speechElapsedMs = request.speechElapsedMs;

    if (!request.ocrEngineId.trimmed().isEmpty()) {
        metadata.runContext.screenshotInput = true;
        metadata.runContext.screenshotOcrEngine =
            functionFlowOcrEngine(request.ocrEngineId);
        metadata.runContext.screenshotOcrElapsedMs =
            request.ocrElapsedMs;
        metadata.runContext.screenshotOcrUsedFallback =
            request.ocrUsedFallback;
        metadata.runContext.screenshotRect =
            request.screenshotRect;
    }

    metadata.flowRunId = request.runId.value;
    metadata.flowPublishedRevision =
        request.publishedRevision;
    metadata.flowPublishedHash = request.publishedHash;
    metadata.flowTrigger = request.trigger;
    metadata.flowFailedNodeId = request.failedNodeId;
    metadata.flowFailedNodeType = request.failedNodeType;
    metadata.flowCancelled = request.cancelled;
    qint64 elapsedMs = 0;
    bool hasElapsed = false;
    for (const FunctionFlowNodeTrace &trace : request.traces) {
        HistoryFlowNodeTrace historyTrace;
        historyTrace.nodeId = trace.nodeId;
        historyTrace.nodeType = trace.nodeType;
        historyTrace.state = trace.state;
        historyTrace.elapsedMs = trace.elapsedMs;
        historyTrace.errorCode = trace.errorCode;
        historyTrace.modelId = trace.modelId;
        historyTrace.promptVersion = trace.promptVersion;
        metadata.flowNodeTraces.append(historyTrace);
        if (trace.elapsedMs >= 0) {
            elapsedMs += trace.elapsedMs;
            hasElapsed = true;
        }
        if (!trace.modelId.trimmed().isEmpty()) {
            metadata.model = trace.modelId;
            metadata.promptVersion = trace.promptVersion;
            metadata.modelElapsedMs = trace.elapsedMs;
        }
    }
    metadata.elapsedMs = hasElapsed ? elapsedMs : -1;
    save.metadata = metadata;
    return save;
}

} // namespace

// 应用运行组装：初始化 Qt、配置、托盘、快捷键和主界面。
int runVocekitApplication(int argc, char *argv[])
{
    QApplication app(argc, argv);
    installRuntimeCrashHandlers();
    const bool startedByAutoStart = isAutoStartLaunch();
    logRuntimeEvent(
        tr8("程序"),
        tr8("启动"),
        QStringLiteral("版本=vocekit，开机自启动=") + (startedByAutoStart ? QStringLiteral("是") : QStringLiteral("否"))
    );
    QStyle *baseStyle = QStyleFactory::create(app.style()->objectName());
    if (!baseStyle) {
        baseStyle = QStyleFactory::create(QStringLiteral("Fusion"));
    }
    app.setStyle(new ToggleSwitchStyle(baseStyle));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    QTranslator qtChineseTranslator;
    const QString deployedTranslations = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"));
    if (!qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), deployedTranslations)) {
        qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    }
    app.installTranslator(&qtChineseTranslator);

    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(QStringLiteral("vocekit"));
    QApplication::setOrganizationName(QStringLiteral("vocekit"));
    app.setFont(appFont());
    ChineseTextContextMenu chineseTextContextMenu(&app);
    app.installEventFilter(&chineseTextContextMenu);

    AppSettingsStore settingsStore;
    OperationError settingsLoadError;
    settingsStore.loadOrCreateDefaults(&settingsLoadError);
    PromptLibraryStore promptLibraryStore;
    promptLibraryStore.load();
    FunctionFlowPlanCache functionFlowPlanCache;
    functionFlowPlanCache.rebuildAll(settingsStore.snapshot());

    ApplicationEvents events;
    FunctionFlowPublicationAccess publicationAccess;
    publicationAccess.settingsSnapshotProvider = [&settingsStore]() {
        return settingsStore.snapshot();
    };
    publicationAccess.replaceAndSave = [&settingsStore](
        const AppSettingsData &data,
        OperationError *error
    ) {
        return settingsStore.replaceAndSave(data, error);
    };
    publicationAccess.validationContextProvider =
        [&promptLibraryStore](
            const AppSettingsData &data,
            const QString &functionId
        ) {
            return functionFlowValidationContext(
                data,
                promptLibraryStore.items(),
                functionId
            );
        };
    publicationAccess.publishSettingsChanged = [&events](
        const QString &key,
        const QString &functionId
    ) {
        SettingsChangeSet change;
        change.keys << key;
        change.functionIds << functionId;
        events.publishSettingsChanged(change);
    };
    FunctionFlowPublicationService publicationService(
        publicationAccess
    );

    FunctionFlowSettingsAccess functionFlows;
    functionFlows.readState = [&publicationService](
        const QString &functionId,
        FunctionFlowState *state,
        OperationError *error
    ) {
        return publicationService.readState(
            functionId,
            state,
            error
        );
    };
    functionFlows.analyzeDraft = [&publicationService](
        const QString &functionId,
        const FunctionFlowGraph &draft
    ) {
        return publicationService.analyzeDraft(
            functionId,
            draft
        );
    };
    functionFlows.addCustomFunction = [&publicationService](
        const FunctionSettings &function,
        OperationError *error
    ) {
        return publicationService.addCustomFunction(
            function,
            error
        );
    };
    functionFlows.updateDraft = [&publicationService](
        const QString &functionId,
        int expectedRevision,
        const FunctionFlowGraph &draft,
        int *savedRevision,
        OperationError *error
    ) {
        return publicationService.updateDraft(
            functionId,
            expectedRevision,
            draft,
            savedRevision,
            error
        );
    };
    functionFlows.updateEditorState = [&publicationService](
        const QString &functionId,
        const FunctionFlowEditorState &editor,
        OperationError *error
    ) {
        return publicationService.updateEditorState(
            functionId,
            editor,
            error
        );
    };
    functionFlows.publish = [&publicationService](
        const QString &functionId,
        int expectedRevision,
        bool replaceCorruptPublished
    ) {
        return publicationService.publish(
            functionId,
            expectedRevision,
            replaceCorruptPublished
        );
    };
    functionFlows.setExecutionMode = [&publicationService](
        const QString &functionId,
        FunctionExecutionMode mode,
        OperationError *error
    ) {
        return publicationService.setExecutionMode(
            functionId,
            mode,
            error
        );
    };
    functionFlows.renameCustomFunction = [&publicationService](
        const QString &functionId,
        const QString &name,
        OperationError *error
    ) {
        return publicationService.renameCustomFunction(
            functionId,
            name,
            error
        );
    };
    functionFlows.removeCustomFunction = [&publicationService](
        const QString &functionId,
        OperationError *error
    ) {
        return publicationService.removeCustomFunction(
            functionId,
            error
        );
    };

    std::function<void(const QStringList &)>
        refreshFunctionFlowRuntime;
    std::function<void(const QStringList &)>
        refreshFunctionFlowHotkeys;
    HubWindowAccess hubAccess;
    hubAccess.settingsSnapshotProvider = [&settingsStore]() {
        return settingsStore.snapshot();
    };
    hubAccess.promptLibraryProvider = [&promptLibraryStore]() {
        return promptLibraryStore.items();
    };
    hubAccess.applyNonFlowAndSave = [&settingsStore](
        const AppSettingsData &data,
        OperationError *error
    ) {
        return settingsStore.replaceNonFlowSettingsAndSave(
            data,
            error
        );
    };
    hubAccess.savePromptLibrary = [&promptLibraryStore](
        const QVector<PromptLibraryItem> &items) {
        OperationError error;
        return promptLibraryStore.save(items, &error);
    };
    hubAccess.functionFlows = functionFlows;
    hubAccess.refreshFunctionFlowRuntime =
        [&refreshFunctionFlowRuntime](
            const QStringList &functionIds) {
            if (refreshFunctionFlowRuntime) {
                refreshFunctionFlowRuntime(functionIds);
            }
        };
    hubAccess.refreshFunctionFlowHotkeys =
        [&refreshFunctionFlowHotkeys](
            const QStringList &functionIds) {
            if (refreshFunctionFlowHotkeys) {
                refreshFunctionFlowHotkeys(functionIds);
            }
        };
    HubSettingsState settings(hubAccess);

    FloatingBarPositionCallbacks floatingBarPositionCallbacks;
    floatingBarPositionCallbacks.hasSavedPosition = [&settings]() {
        return settings.hasFloatingBarPosition();
    };
    floatingBarPositionCallbacks.savedPosition = [&settings]() {
        return settings.floatingBarPosition();
    };
    floatingBarPositionCallbacks.savePosition = [&settings](const QPoint &position) {
        settings.load();
        settings.setFloatingBarPosition(position);
        settings.save();
    };
    FloatingBar bar(floatingBarPositionCallbacks);
    GlobalHotkeys hotkeys;
    VoiceController *controller = nullptr;

    std::function<void()> settingsChanged;
    QScopedPointer<HubWindow> hub(createHubWindow(
        hubAccess,
        &bar,
        [&]() {
            if (settingsChanged) {
                settingsChanged();
            }
        },
        [&](const QString &sourceText, const QString &scopeId, QString *error, const QString &editedText, const QString &extraContext) {
            if (controller) {
                return controller->suggestVocabularyEntry(sourceText, scopeId, error, editedText, extraContext);
            }
            if (error) {
                *error = tr8("词库 AI 入口尚未初始化。");
            }
            return VocabularySuggestion();
        }
    ));
    hub->setApplicationEvents(&events);
    FunctionFlowExecutionController *flowExecutionController =
        nullptr;
    VoiceControllerAccess voiceAccess;
    voiceAccess.settingsSnapshotProvider = [&settingsStore]() {
        return settingsStore.snapshot();
    };
    voiceAccess.promptSnapshotProvider = [&settingsStore, &promptLibraryStore]() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings = settingsStore.snapshot();
        snapshot.libraryItems = promptLibraryStore.items();
        return snapshot;
    };
    voiceAccess.applyAndSave = [&settingsStore, &settings](
        const AppSettingsData &updatedSettings) {
        OperationError error;
        if (!settingsStore.replaceNonFlowSettingsAndSave(
                updatedSettings,
                &error)) {
            return false;
        }
        settings.load();
        return true;
    };
    voiceAccess.startPublishedFlow =
        [&functionFlowPlanCache,
         &flowExecutionController,
         &hub,
         &bar](
            const FunctionFlowTriggerRequest &request) {
            const QSharedPointer<
                const FunctionFlowExecutionPlan
            > plan = functionFlowPlanCache.plan(
                request.functionId
            );
            if (plan.isNull()) {
                OperationError error =
                    functionFlowPlanCache.error(
                        request.functionId
                    );
                if (error.code.trimmed().isEmpty()) {
                    error.code = QStringLiteral(
                        "flow_published_unavailable"
                    );
                }
                showAttentionWarning(
                    hub.data(),
                    tr8("功能流程配置错误"),
                    functionFlowUserMessage(error)
                );
                return FunctionFlowStartOutcome::
                    ConfigurationError;
            }
            if (!flowExecutionController) {
                return FunctionFlowStartOutcome::
                    ConfigurationError;
            }
            const FunctionFlowStartOutcome outcome =
                flowExecutionController->start(
                request,
                plan
            );
            if (outcome
                == FunctionFlowStartOutcome::Busy) {
                bar.setStatus(
                    tr8("正在处理"),
                    tr8("请等待当前任务完成，或再次触发当前流程取消。")
                );
                bar.hideLater();
            } else if (
                outcome
                    == FunctionFlowStartOutcome::
                        TargetUnavailable) {
                OperationError error =
                    flowExecutionController
                        ->lastStartError();
                if (error.code.trimmed().isEmpty()) {
                    error.code = QStringLiteral(
                        "flow_target_window_unavailable"
                    );
                }
                showAttentionWarning(
                    hub.data(),
                    tr8("目标窗口不可用"),
                    functionFlowUserMessage(error)
                );
            } else if (
                outcome
                    == FunctionFlowStartOutcome::
                        ConfigurationError) {
                OperationError error =
                    flowExecutionController
                        ->lastStartError();
                if (error.code.trimmed().isEmpty()) {
                    error.code = QStringLiteral(
                        "flow_dependency_resolution_failed"
                    );
                }
                showAttentionWarning(
                    hub.data(),
                    tr8("功能流程配置错误"),
                    functionFlowUserMessage(error)
                );
            }
            return outcome;
        };
    voiceAccess.flowProcessing =
        [&flowExecutionController]() {
            return flowExecutionController
                && flowExecutionController->isRunning();
        };
    VoiceController voice(voiceAccess, &bar, hub.data());
    HotkeyRefreshCoordinator hotkeyRefreshCoordinator(
        [&](const GlobalHotkeySettingsSnapshot &snapshot) {
            const QStringList hotkeyFailures =
                hotkeys.registerFromSnapshot(snapshot);
            voice.setActiveHoldFunctions(
                hotkeys.activeHoldFunctions()
            );
            for (const QString &failure : hotkeyFailures) {
                logRuntimeEvent(
                    tr8("快捷键"),
                    tr8("注册失败"),
                    failure
                );
            }
            if (!hotkeyFailures.isEmpty() && hub->isVisible()) {
                QTimer::singleShot(
                    0,
                    hub.data(),
                    [&hub, hotkeyFailures]() {
                        showAttentionWarning(
                            hub.data(),
                            tr8("全局快捷键注册失败"),
                            tr8("以下快捷键没有注册成功：\n")
                                + hotkeyFailures.join(
                                    QStringLiteral("\n")
                                )
                                + tr8(
                                    "\n\n请修改冲突快捷键，或关闭占用它的其它软件。"
                                )
                        );
                    }
                );
            }
        }
    );
    QPointer<VoiceController> controllerGuard(&voice);
    controller = &voice;

    FunctionFlowResultControllerAccess flowResultAccess;
    flowResultAccess.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return ClipboardWriter::isUsableExternalWindow(
                static_cast<ClipboardWindowHandle>(target)
            );
        };
    flowResultAccess.hasCurrentSelection =
        [](FunctionFlowTargetWindowHandle target) {
            return SelectedTextReader::hasSelectionInWindow(
                static_cast<SelectedTextNativeWindowHandle>(
                    target
                )
            );
        };
    flowResultAccess.writeText =
        [](
            const QString &text,
            FunctionFlowTargetWindowHandle target,
            bool replaceSelection,
            bool hasSelection) {
            return ClipboardWriter::pasteTextToWindowChecked(
                text,
                static_cast<ClipboardWindowHandle>(target),
                replaceSelection,
                hasSelection
            );
        };
    flowResultAccess.addVocabulary =
        [&voice](
            const QString &source,
            const QString &scopeId,
            const QString &edited) {
            voice.addVocabularyForFlow(
                source,
                scopeId,
                edited
            );
        };
    flowResultAccess.saveResultPopupGeometry =
        [&settingsStore, &settings](const QRect &geometry) {
            AppSettingsData updated = settingsStore.snapshot();
            updated.windows.hasResultPopupGeometry = true;
            updated.windows.resultPopupGeometry = geometry;
            OperationError error;
            if (settingsStore.replaceNonFlowSettingsAndSave(
                    updated,
                    &error)) {
                settings.load();
            }
        };
    flowResultAccess.saveScreenshotPanelPreference =
        [&settingsStore, &settings](
            const QRect &geometry,
            int opacity) {
            AppSettingsData updated = settingsStore.snapshot();
            updated.windows.hasScreenshotResultGeometry = true;
            updated.windows.screenshotResultGeometry = geometry;
            updated.windows.screenshotResultOpacity =
                qBound(35, opacity, 100);
            OperationError error;
            if (settingsStore.replaceNonFlowSettingsAndSave(
                    updated,
                    &error)) {
                settings.load();
            }
        };
    flowResultAccess.showInformation =
        [&hub](
            const QString &title,
            const QString &message) {
            showAttentionInformation(
                hub.data(),
                title,
                message
            );
        };

    FunctionFlowResultControllerCallbacks flowResultCallbacks;
    flowResultCallbacks.requestCancel =
        [&flowExecutionController](const ExecutionId &runId) {
            if (flowExecutionController) {
                flowExecutionController->cancel(runId);
            }
        };
    flowResultCallbacks.editableSurfaceOpened =
        [&flowExecutionController](const ExecutionId &runId) {
            if (flowExecutionController) {
                flowExecutionController
                    ->editableSurfaceOpened(runId);
            }
        };
    flowResultCallbacks.editedTextCommitted =
        [&flowExecutionController](
            const ExecutionId &runId,
            const QString &text) {
            if (flowExecutionController) {
                flowExecutionController->editedTextCommitted(
                    runId,
                    text
                );
            }
        };
    flowResultCallbacks.editableSurfaceClosed =
        [&flowExecutionController](const ExecutionId &runId) {
            if (flowExecutionController) {
                flowExecutionController
                    ->editableSurfaceClosed(runId);
            }
        };
    FunctionFlowResultController flowResultController(
        flowResultAccess,
        flowResultCallbacks
    );

    QMap<QString, QString> savedFunctionFlowHistory;
    FunctionFlowRuntimeAdapterAccess flowAdapterAccess;
    flowAdapterAccess.runtimeSnapshot =
        [&settingsStore, &promptLibraryStore]() {
            PromptRuntimeSnapshot snapshot;
            snapshot.settings = settingsStore.snapshot();
            snapshot.libraryItems = promptLibraryStore.items();
            return snapshot;
        };
    flowAdapterAccess.availableModelIds = []() {
        QStringList ids;
        for (const ModelOption &option : modelOptions()) {
            if (isModelProviderAvailableForTask(option.id)) {
                ids.append(option.id);
            }
        }
        return ids;
    };
    flowAdapterAccess.speechConfigurationError =
        [](const QString &providerId) {
            return speechRecognitionProviderConfigurationError(
                providerId
            );
        };
    flowAdapterAccess.resolveRecordDirectory =
        [](const QString &recordDirectory) {
            return historyRootPath(recordDirectory);
        };
    flowAdapterAccess.isUsableExternalTargetWindow =
        flowResultAccess.isUsableExternalTargetWindow;
    flowAdapterAccess.readSelectedText =
        [&voice](const SelectedTextWorkflowRequest &request) {
            return voice.readSelectedTextForFlow(request);
        };
    flowAdapterAccess.collectVoice =
        [&voice](
            const FunctionFlowRunContext &run,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowNodeCompletion &completion) {
            voice.beginVoiceForFlow(run, node, completion);
        };
    flowAdapterAccess.collectScreenshot =
        [&voice](
            const FunctionFlowRunContext &run,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowNodeCompletion &completion) {
            voice.beginScreenshotForFlow(
                run,
                node,
                completion
            );
        };
    flowAdapterAccess.runResultAction =
        [&flowResultController](
            const FunctionFlowRunContext &run,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowResultActionRequest &request,
            const FunctionFlowNodeCompletion &completion) {
            flowResultController.runAction(
                run,
                node,
                request,
                completion
            );
        };
    flowAdapterAccess.beginStreamingPreview =
        [&flowResultController](
            const FunctionFlowRunContext &run,
            const QString &modelNodeId,
            const QString &popupNodeId) {
            flowResultController.beginStreamingPreview(
                run,
                modelNodeId,
                popupNodeId
            );
        };
    flowAdapterAccess.appendStreamingDelta =
        [&flowResultController](
            const ExecutionId &runId,
            const QString &modelNodeId,
            const QString &popupNodeId,
            const QString &delta) {
            flowResultController.appendStreamingDelta(
                runId,
                modelNodeId,
                popupNodeId,
                delta
            );
        };
    flowAdapterAccess.abandonStreamingPreview =
        [&flowResultController](
            const ExecutionId &runId,
            const QString &modelNodeId,
            const QString &popupNodeId) {
            flowResultController.abandonStreamingPreview(
                runId,
                modelNodeId,
                popupNodeId
            );
        };
    flowAdapterAccess.saveHistory =
        [&savedFunctionFlowHistory, &events](
            const FunctionFlowHistoryRequest &request) {
            FunctionFlowHistorySaveResult result;
            const QString runId = request.runId.value.trimmed();
            if (runId.isEmpty()
                || request.recordDirectory.trimmed().isEmpty()) {
                result.error.code =
                    QStringLiteral("flow_history_save_failed");
                return result;
            }
            if (savedFunctionFlowHistory.contains(runId)) {
                result.ok = true;
                result.alreadyExists = true;
                result.detailPath =
                    savedFunctionFlowHistory.value(runId);
                return result;
            }

            const HistoryAppendResult saved =
                HistoryRecordService::save(
                    historyRequestForFunctionFlow(request)
                );
            if (!saved.ok
                || saved.modeDetailPath.trimmed().isEmpty()) {
                result.error.code =
                    QStringLiteral("flow_history_save_failed");
                return result;
            }
            result.ok = true;
            result.detailPath = saved.modeDetailPath;
            savedFunctionFlowHistory.insert(
                runId,
                result.detailPath
            );
            HistoryChangeSet change;
            change.recordIds << result.detailPath;
            events.publishHistoryChanged(change);
            return result;
        };
    flowAdapterAccess.updateHistoryEditedText =
        [&events](
            const FunctionFlowHistoryEditRequest &request) {
            FunctionFlowHistoryEditResult result;
            OperationError error;
            result.ok = HistoryRecordService(
                request.recordDirectory
            ).updateFlowEditedText(
                request.runId,
                request.detailPath,
                request.editedText,
                &error
            );
            result.error = error;
            if (result.ok) {
                HistoryChangeSet change;
                change.recordIds << request.detailPath;
                events.publishHistoryChanged(change);
            }
            return result;
        };
    FunctionFlowRuntimeAdapters functionFlowRuntimeAdapters(
        flowAdapterAccess
    );
    FunctionFlowExecutionController functionFlowExecution(
        functionFlowRuntimeAdapters.runtimeAccess()
    );
    flowExecutionController = &functionFlowExecution;

    const QString functionFlowLogPath =
        QDir(runtimeLogDirectory()).filePath(
            QStringLiteral("function-flow.jsonl")
        );
    QObject::connect(
        &functionFlowExecution,
        &FunctionFlowExecutionController::nodeExecutionChanged,
        hub.data(),
        [&hub, functionFlowLogPath](
            const FunctionFlowNodeExecutionEvent &event) {
            hub->applyFunctionFlowRuntimeEvent(event);
            FunctionFlowRuntimeLogEntry entry;
            entry.functionId = event.functionId;
            entry.publishedRevision = event.publishedRevision;
            entry.publishedHash = event.publishedHash;
            entry.runId = event.runId;
            entry.trigger =
                functionFlowTriggerId(event.trigger);
            entry.nodeId = event.nodeId;
            entry.nodeType =
                functionFlowNodeTypeId(event.nodeType);
            switch (event.state) {
            case FunctionFlowNodeState::Pending:
                entry.status = QStringLiteral("pending");
                break;
            case FunctionFlowNodeState::Ready:
                entry.status = QStringLiteral("ready");
                break;
            case FunctionFlowNodeState::Running:
                entry.status = QStringLiteral("running");
                break;
            case FunctionFlowNodeState::Cancelling:
                entry.status = QStringLiteral("cancelling");
                break;
            case FunctionFlowNodeState::Succeeded:
                entry.status = QStringLiteral("succeeded");
                break;
            case FunctionFlowNodeState::Skipped:
                entry.status = QStringLiteral("skipped");
                break;
            case FunctionFlowNodeState::Failed:
                entry.status = QStringLiteral("failed");
                break;
            case FunctionFlowNodeState::Blocked:
                entry.status = QStringLiteral("blocked");
                break;
            case FunctionFlowNodeState::Cancelled:
                entry.status = QStringLiteral("cancelled");
                break;
            }
            entry.elapsedMs = event.elapsedMs;
            entry.error.code = event.errorCode;
            entry.modelId = event.modelId;
            entry.promptVersion = event.promptVersion;
            appendFunctionFlowRuntimeLog(
                functionFlowLogPath,
                entry
            );
        }
    );
    QObject::connect(
        &functionFlowExecution,
        &FunctionFlowExecutionController::runExecutionChanged,
        hub.data(),
        [&hub, functionFlowLogPath](
            const FunctionFlowRunExecutionEvent &event) {
            hub->applyFunctionFlowRunEvent(event);
            FunctionFlowRuntimeLogEntry entry;
            entry.functionId = event.functionId;
            entry.publishedRevision = event.publishedRevision;
            entry.publishedHash = event.publishedHash;
            entry.runId = event.runId;
            entry.trigger =
                functionFlowTriggerId(event.trigger);
            entry.status = event.running
                ? QStringLiteral("started")
                : (event.cancelled
                    ? QStringLiteral("cancelled")
                    : (event.terminalError.isEmpty()
                        ? QStringLiteral("succeeded")
                        : QStringLiteral("failed")));
            entry.error = event.terminalError;
            appendFunctionFlowRuntimeLog(
                functionFlowLogPath,
                entry
            );
            if (!event.running
                && !event.cancelled
                && !event.terminalError.isEmpty()) {
                showAttentionWarning(
                    hub.data(),
                    tr8("功能流程运行失败"),
                    functionFlowUserMessage(
                        event.terminalError
                    )
                );
            }
        }
    );

    ScreenshotLauncher screenshotLauncher;
    std::function<void()> refreshScreenshotLauncher = [&]() {
        QVector<QPair<QString, QString>> functions;
        const AppSettingsData snapshot =
            settingsStore.snapshot();
        for (const FunctionSettings &function :
             snapshot.functions) {
            if (!functionUsesScreenshotLauncher(
                    function,
                    functionFlowPlanCache.plan(function.id))) {
                continue;
            }
            functions.append(qMakePair(
                function.id,
                functionDisplayTitle(
                    snapshot,
                    function.id,
                    tr8("自定义功能")
                )
            ));
        }
        screenshotLauncher.setFunctions(functions);
        screenshotLauncher.setSavedPosition(
            settings.screenshotLauncherPosition(),
            settings.hasScreenshotLauncherPosition()
        );
    };
    screenshotLauncher.positionChangedCallback =
        [&](const QPoint &position) {
            settings.load();
            settings.setScreenshotLauncherPosition(position);
            settings.save();
        };
    screenshotLauncher.captureTargetWindowCallback = []() {
        const FunctionCommandWindowHandle target =
            captureForegroundFunctionCommandWindow();
        return ClipboardWriter::isUsableExternalWindow(target)
            ? static_cast<
                ScreenshotLauncherTargetWindowHandle
              >(target)
            : nullptr;
    };
    screenshotLauncher.functionTriggeredCallback =
        [&](
            const QString &functionId,
            ScreenshotLauncherTargetWindowHandle targetWindow) {
            screenshotLauncher.hide();
            QTimer::singleShot(
                80,
                &voice,
                [controllerGuard, functionId, targetWindow]() {
                    if (controllerGuard) {
                        controllerGuard
                            ->handleScreenshotLauncherTrigger(
                                functionId,
                                static_cast<
                                    FunctionFlowTargetWindowHandle
                                >(targetWindow)
                            );
                    }
                }
            );
            QTimer::singleShot(
                500,
                &screenshotLauncher,
                [&refreshScreenshotLauncher]() {
                    refreshScreenshotLauncher();
                }
            );
        };
    setAttentionFaqCallback([&hub](const QString &faqId) {
        hub->openFaqById(faqId);
    });

    SelectionContextFeatureAccess selectionFeatureAccess;
    selectionFeatureAccess.settingsSnapshot = [&settingsStore]() {
        return settingsStore.snapshot();
    };
    selectionFeatureAccess.promptSnapshot = [
        &settingsStore,
        &promptLibraryStore
    ]() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings = settingsStore.snapshot();
        snapshot.libraryItems = promptLibraryStore.items();
        return snapshot;
    };
    selectionFeatureAccess.openVocabularyEditor = [&voice](
        const QString &text,
        const QString &scopeId) {
        voice.addVocabularyLocallyForFlow(
            text,
            scopeId,
            QString()
        );
    };
    selectionFeatureAccess.blockApplication = [
        &settingsStore,
        &settings,
        &settingsChanged
    ](const QString &executable) {
        const QString normalized = executable.trimmed().toLower();
        if (normalized.isEmpty()) {
            return false;
        }
        AppSettingsData next = settingsStore.snapshot();
        QStringList blocked;
        for (const QString &entry :
             next.selectionContext.blockedApplications) {
            const QString value = entry.trimmed().toLower();
            if (!value.isEmpty() && !blocked.contains(value)) {
                blocked.append(value);
            }
        }
        if (!blocked.contains(normalized)) {
            blocked.append(normalized);
        }
        next.selectionContext.blockedApplications = blocked;
        OperationError error;
        if (!settingsStore.replaceNonFlowSettingsAndSave(next, &error)) {
            return false;
        }
        settings.load();
        if (settingsChanged) {
            settingsChanged();
        }
        return true;
    };
    selectionFeatureAccess.openSettings = [&hub]() {
        hub->showSettingsPage(0);
    };
    selectionFeatureAccess.ensureNetworkConsent = [
        &hub,
        &settingsStore,
        &settings,
        &settingsChanged
    ](const QString &, const QString &) {
        if (settingsStore.snapshot()
                .selectionContext.networkConsentAcknowledged) {
            return true;
        }
        const QMessageBox::StandardButton choice = QMessageBox::question(
            hub.data(),
            tr8("允许发送选中文字吗？"),
            tr8(
                "此操作会把当前选中的文字发送到你配置的大模型服务。"
                "不会在提示框或运行日志中显示原文。是否继续？"
            ),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (choice != QMessageBox::Yes) {
            return false;
        }
        AppSettingsData next = settingsStore.snapshot();
        next.selectionContext.networkConsentAcknowledged = true;
        OperationError error;
        if (!settingsStore.replaceNonFlowSettingsAndSave(next, &error)) {
            return false;
        }
        settings.load();
        if (settingsChanged) {
            settingsChanged();
        }
        return true;
    };
    selectionFeatureAccess.logMetadata = [](
        const QString &eventId,
        const QString &actionId,
        int textLength,
        qint64 elapsedMs) {
        logRuntimeEvent(
            tr8("选中文字"),
            eventId,
            QStringLiteral("action=%1,textLength=%2,elapsedMs=%3")
                .arg(actionId)
                .arg(textLength)
                .arg(elapsedMs)
        );
    };
    SelectionContextFeature selectionContextFeature(
        selectionFeatureAccess
    );

    hotkeys.setCallback(
        [controllerGuard,
         &selectionContextFeature,
         &hotkeys,
         &hotkeyRefreshCoordinator](const QString &id) {
            if (id == QStringLiteral("selection_toolbar")) {
                selectionContextFeature.triggerFallbackShortcut();
                return;
            }
            dispatchRegisteredHotkeyPress(
                id,
                hotkeys.activeHoldFunctions(),
                [&hotkeyRefreshCoordinator](
                    const QString &functionId) {
                    hotkeyRefreshCoordinator.holdPressed(
                        functionId
                    );
                },
                [controllerGuard](const QString &functionId) {
                    if (controllerGuard) {
                        controllerGuard->handleHotkeyPressed(
                            functionId
                        );
                    }
                },
                [controllerGuard](const QString &functionId) {
                    if (controllerGuard) {
                        controllerGuard->handleHotkey(functionId);
                    }
                }
            );
        }
    );
    hotkeys.setHoldCallback(
        [controllerGuard,
         &hotkeyRefreshCoordinator,
         &hotkeys](
            const QString &id,
            HoldShortcutTransition transition
        ) {
            if (transition == HoldShortcutTransition::Pressed) {
                hotkeyRefreshCoordinator.holdPressed(id);
            } else if (
                transition == HoldShortcutTransition::Released
            ) {
                if (controllerGuard) {
                    // 先按冻结 owner 结束旧运行，再允许刷新卸载旧 hook。
                    controllerGuard->handleHotkeyReleased(id);
                }
                hotkeyRefreshCoordinator.holdReleased(
                    id,
                    hotkeys.hasPressedHold()
                );
            }
        }
    );
    qApp->installNativeEventFilter(&hotkeys);

    refreshFunctionFlowRuntime =
        [&](const QStringList &functionIds) {
        settings.load();
        const AppSettingsData snapshot =
            settingsStore.snapshot();
        if (functionIds.isEmpty()) {
            functionFlowPlanCache.rebuildAll(snapshot);
        } else {
            QStringList uniqueIds = functionIds;
            uniqueIds.removeDuplicates();
            for (const QString &functionId : uniqueIds) {
                functionFlowPlanCache.rebuildFunction(
                    snapshot,
                    functionId
                );
            }
        }
        QStringList inspectedIds = functionIds;
        if (inspectedIds.isEmpty()) {
            for (const FunctionSettings &function :
                 snapshot.functions) {
                inspectedIds.append(function.id);
            }
        }
        inspectedIds.removeDuplicates();
        for (const QString &functionId : inspectedIds) {
            const OperationError cacheError =
                functionFlowPlanCache.error(functionId);
            if (!cacheError.code.trimmed().isEmpty()) {
                logRuntimeEvent(
                    tr8("功能流程"),
                    tr8("发布缓存不可用"),
                    QStringLiteral("功能=") + functionId
                        + QStringLiteral("，错误码=")
                        + cacheError.code
                );
            }
        }
        voice.reload();
        refreshScreenshotLauncher();
    };
    refreshFunctionFlowHotkeys =
        [&](const QStringList &) {
        settings.load();
        hotkeyRefreshCoordinator.requestRefresh(
            globalHotkeySnapshotFromData(
                settings.toData(),
                [&functionFlowPlanCache](
                    const QString &functionId) {
                    return functionFlowPlanCache.plan(
                        functionId
                    );
                }
            ),
            hotkeys.hasPressedHold()
        );
    };
    settingsChanged = [&]() {
        settings.load();
        setWindowsAutoStartEnabled(settings.autoStartEnabled());
        refreshFunctionFlowRuntime(QStringList());
        refreshFunctionFlowHotkeys(QStringList());
        bar.setEnabledVisible(settings.floatingBarEnabled());
        selectionContextFeature.refresh();
        SettingsChangeSet change;
        events.publishSettingsChanged(change);
        logRuntimeEvent(
            tr8("设置"),
            tr8("已应用"),
            QStringLiteral("语音识别=") + settings.speechProvider()
                + QStringLiteral("，系统代理=") + (settings.useSystemProxy() ? QStringLiteral("开") : QStringLiteral("关"))
                + QStringLiteral("，浮动条=") + (settings.floatingBarEnabled() ? QStringLiteral("开") : QStringLiteral("关"))
        );
    };
    selectionContextFeature.start();
    settingsChanged();

    TrayController::Callbacks trayCallbacks;
    trayCallbacks.speechProvider = [&settings]() {
        return settings.speechProvider();
    };
    trayCallbacks.useSystemProxy = [&settings]() {
        return settings.useSystemProxy();
    };
    trayCallbacks.floatingBarEnabled = [&settings]() {
        return settings.floatingBarEnabled();
    };
    trayCallbacks.selectionContextEnabled = [&settingsStore]() {
        return settingsStore.snapshot().selectionContext.enabled;
    };
    trayCallbacks.selectionContextPaused = [&selectionContextFeature]() {
        return selectionContextFeature.isPaused();
    };
    trayCallbacks.setSpeechProvider = [&settings, &settingsChanged](const QString &provider) {
        settings.load();
        settings.setSpeechProvider(provider);
        settings.save();
        if (settingsChanged) {
            settingsChanged();
        }
    };
    trayCallbacks.setUseSystemProxy = [&settings, &settingsChanged](bool enabled) {
        settings.load();
        settings.setUseSystemProxy(enabled);
        settings.save();
        if (settingsChanged) {
            settingsChanged();
        }
    };
    trayCallbacks.setFloatingBarEnabled = [&settings, &settingsChanged](bool enabled) {
        settings.load();
        settings.setFloatingBarEnabled(enabled);
        settings.save();
        if (settingsChanged) {
            settingsChanged();
        }
    };
    trayCallbacks.setSelectionContextEnabled = [
        &settingsStore,
        &settings,
        &settingsChanged
    ](bool enabled) {
        AppSettingsData next = settingsStore.snapshot();
        next.selectionContext.enabled = enabled;
        OperationError error;
        if (!settingsStore.replaceNonFlowSettingsAndSave(next, &error)) {
            return;
        }
        settings.load();
        if (settingsChanged) {
            settingsChanged();
        }
    };
    trayCallbacks.pauseSelectionContextThirtyMinutes = [
        &selectionContextFeature
    ]() {
        selectionContextFeature.pauseForMinutes(30);
    };
    trayCallbacks.resumeSelectionContext = [&selectionContextFeature]() {
        selectionContextFeature.resume();
    };
    trayCallbacks.showFloatingBarTest = [&hub, &bar, &settings]() {
        if (!settings.floatingBarEnabled()) {
            showAttentionInformation(hub.data(), tr8("浮动条已关闭"), tr8("请在设置的“常用设置”页勾选“启用浮动条”。"));
            return;
        }
        bar.setStyle(settings.toData().floatingBarStyle);
        bar.setSuppressed(false);
        bar.setStatus(tr8("浮动条测试"), tr8("语音输入时显示，结束后自动关闭"));
        bar.hideLater();
    };
    trayCallbacks.openSettings = [&hub]() {
        hub->showSettingsPage();
    };
    trayCallbacks.requestApplicationQuit = [&hub]() {
        if (hub) {
            hub->requestApplicationQuit();
        }
    };
    TrayController tray(hub.data(), trayCallbacks);

    const QRect screen = primaryAvailableScreenGeometry();
    hub->move(screen.left() + 60, screen.top() + 40);
    if (!startedByAutoStart) {
        hub->show();
    }
    bar.setEnabledVisible(settings.floatingBarEnabled());

    const int exitCode = app.exec();
    selectionContextFeature.stop();
    logRuntimeEvent(tr8("程序"), tr8("退出"), QStringLiteral("exitCode=") + QString::number(exitCode));
    setAttentionFaqCallback(AttentionFaqCallback());
    hotkeys.setHoldCallback(std::function<void(const QString &, HoldShortcutTransition)>());
    qApp->removeNativeEventFilter(&hotkeys);
    hotkeys.setCallback(std::function<void(const QString &)>());
    functionFlowExecution.cancel();
    flowExecutionController = nullptr;
    controller = nullptr;
    return exitCode;
}
