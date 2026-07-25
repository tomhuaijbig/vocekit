#include "vocekit_application_runtime.h"

#include "application_events.h"
#include "../capture/screenshot_launcher.h"
#include "../capture/screenshot_types.h"
#include "../config/app_settings_store.h"
#include "../controllers/tray_controller.h"
#include "../controllers/voice_controller.h"
#include "../domain/app_legacy_types.h"
#include "../domain/prompt_runtime_library.h"
#include "../input/global_hotkeys.h"
#include "../input/hotkey_settings_snapshot.h"
#include "../input/hold_to_talk.h"
#include "../platform/windows_autostart.h"
#include "../runtime_crash_handler.h"
#include "../runtime_log.h"
#include "../storage/prompt_library_store.h"
#include "../ui/attention_message.h"
#include "../ui/app_dialogs.h"
#include "../ui/chinese_text_context_menu.h"
#include "../ui/floating_bar.h"
#include "../ui/hub_settings_state.h"
#include "../ui/hub_window.h"
#include "../ui/toggle_switch.h"
#include "../ui/ui_style.h"

#include <QtWidgets>
#include <QPointer>
#include <functional>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

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
        qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
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

    HubWindowAccess hubAccess;
    hubAccess.settingsSnapshotProvider = [&settingsStore]() {
        return settingsStore.snapshot();
    };
    hubAccess.promptLibraryProvider = [&promptLibraryStore]() {
        return promptLibraryStore.items();
    };
    hubAccess.applyAndSave = [&settingsStore](const AppSettingsData &data) {
        OperationError error;
        return settingsStore.replaceAndSave(data, &error);
    };
    hubAccess.savePromptLibrary = [&promptLibraryStore](
        const QVector<PromptLibraryItem> &items) {
        OperationError error;
        return promptLibraryStore.save(items, &error);
    };
    HubSettingsState settings(hubAccess);

    ApplicationEvents events;
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
        if (!settingsStore.replaceAndSave(updatedSettings, &error)) {
            return false;
        }
        settings.load();
        return true;
    };
    VoiceController voice(voiceAccess, &bar, hub.data());
    QPointer<VoiceController> controllerGuard(&voice);
    controller = &voice;
    ScreenshotLauncher screenshotLauncher;
    std::function<void()> refreshScreenshotLauncher = [&]() {
        QVector<QPair<QString, QString>> functions;
        const QVector<QPair<QString, QString>> builtIns = {
            qMakePair(QStringLiteral("dictate"), tr8("听写")),
            qMakePair(QStringLiteral("translate"), tr8("翻译")),
            qMakePair(QStringLiteral("ask"), tr8("问答"))
        };
        for (const auto &function : builtIns) {
            if (settings.useScreenshotFor(function.first)
                && screenshotTriggerUsesLauncher(
                    settings.screenshotTriggerModeFor(
                        function.first
                    ))) {
                functions.append(function);
            }
        }
        for (const CustomFunctionDef &function :
            settings.customFunctions()) {
            if (function.useScreenshot
                && screenshotTriggerUsesLauncher(
                    function.screenshotTriggerMode)) {
                functions.append(qMakePair(
                    function.id,
                    function.name.trimmed().isEmpty()
                        ? tr8("自定义功能")
                        : function.name.trimmed()
                ));
            }
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
    screenshotLauncher.functionTriggeredCallback =
        [&](const QString &functionId) {
            screenshotLauncher.hide();
            QTimer::singleShot(
                80,
                &voice,
                [controllerGuard, functionId]() {
                    if (controllerGuard) {
                        controllerGuard->handleScreenshotTrigger(
                            functionId
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

    hotkeys.setCallback([controllerGuard](const QString &id) {
        if (controllerGuard) {
            controllerGuard->handleHotkey(id);
        }
    });
    hotkeys.setHoldCallback(
        [controllerGuard](
            const QString &id,
            HoldShortcutTransition transition
        ) {
            if (controllerGuard
                && transition == HoldShortcutTransition::Released) {
                controllerGuard->handleHotkeyReleased(id);
            }
        }
    );
    qApp->installNativeEventFilter(&hotkeys);

    settingsChanged = [&]() {
        settings.load();
        setWindowsAutoStartEnabled(settings.autoStartEnabled());
        voice.reload();
        const QStringList hotkeyFailures =
            hotkeys.registerFromSnapshot(
                globalHotkeySnapshotFromData(settings.toData())
            );
        voice.setActiveHoldFunctions(hotkeys.activeHoldFunctions());
        for (const QString &failure : hotkeyFailures) {
            logRuntimeEvent(tr8("快捷键"), tr8("注册失败"), failure);
        }
        if (!hotkeyFailures.isEmpty() && hub->isVisible()) {
            QTimer::singleShot(0, hub.data(), [&hub, hotkeyFailures]() {
                showAttentionWarning(
                    hub.data(),
                    tr8("全局快捷键注册失败"),
                    tr8("以下快捷键没有注册成功：\n")
                        + hotkeyFailures.join(QStringLiteral("\n"))
                        + tr8("\n\n请修改冲突快捷键，或关闭占用它的其它软件。")
                );
            });
        }
        bar.setEnabledVisible(settings.floatingBarEnabled());
        SettingsChangeSet change;
        events.publishSettingsChanged(change);
        refreshScreenshotLauncher();
        logRuntimeEvent(
            tr8("设置"),
            tr8("已应用"),
            QStringLiteral("语音识别=") + settings.speechProvider()
                + QStringLiteral("，系统代理=") + (settings.useSystemProxy() ? QStringLiteral("开") : QStringLiteral("关"))
                + QStringLiteral("，浮动条=") + (settings.floatingBarEnabled() ? QStringLiteral("开") : QStringLiteral("关"))
        );
    };
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
    trayCallbacks.showFloatingBarTest = [&hub, &bar, &settings]() {
        if (!settings.floatingBarEnabled()) {
            showAttentionInformation(hub.data(), tr8("浮动条已关闭"), tr8("请在设置的“常用设置”页勾选“启用浮动条”。"));
            return;
        }
        bar.setSuppressed(false);
        bar.setStatus(tr8("浮动条测试"), tr8("语音输入时显示，结束后自动关闭"));
        bar.hideLater();
    };
    trayCallbacks.openSettings = [&hub]() {
        hub->showSettingsPage();
    };
    TrayController tray(hub.data(), trayCallbacks);

    const QRect screen = QApplication::desktop()->availableGeometry();
    hub->move(screen.left() + 60, screen.top() + 40);
    if (!startedByAutoStart) {
        hub->show();
    }
    bar.setEnabledVisible(settings.floatingBarEnabled());

    const int exitCode = app.exec();
    logRuntimeEvent(tr8("程序"), tr8("退出"), QStringLiteral("exitCode=") + QString::number(exitCode));
    setAttentionFaqCallback(AttentionFaqCallback());
    qApp->removeNativeEventFilter(&hotkeys);
    hotkeys.setCallback(std::function<void(const QString &)>());
    controller = nullptr;
    return exitCode;
}
