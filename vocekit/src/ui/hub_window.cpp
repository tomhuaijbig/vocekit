#include "hub_window.h"
#include "../app/application_events.h"
#include "../api/api_client_utils.h"
#include "../capture/screenshot_launcher.h"
#include "../capture/screenshot_types.h"
#include "../config/app_settings_json.h"
#include "../config/app_settings_store.h"
#include "../config/baidu_sample_parser.h"
#include "../controllers/tray_controller.h"
#include "../controllers/voice_controller.h"
#include "../controllers/voice_controller_host.h"
#include "../domain/app_legacy_types.h"
#include "../domain/function_catalog.h"
#include "../domain/history_filter.h"
#include "../domain/history_modes.h"
#include "../domain/history_record_builder.h"
#include "../domain/history_selection.h"
#include "../domain/history_text.h"
#include "../domain/prompt_runtime_library.h"
#include "../file_utils.h"
#include "../input/global_hotkeys.h"
#include "../input/hotkey_definitions.h"
#include "../input/hotkey_settings_snapshot.h"
#include "../input/hold_to_talk.h"
#include "../platform/windows_autostart.h"
#include "../runtime_crash_handler.h"
#include "../result_flow_config.h"
#include "../runtime_log.h"
#include "../storage/history_record_service.h"
#include "../storage/history_archive.h"
#include "../storage/history_export.h"
#include "../storage/history_favorites.h"
#include "../storage/history_paths.h"
#include "../storage/history_store.h"
#include "../storage/prompt_library_store.h"
#include "../tasks/history_segment_retry_task.h"
#include "../ui/attention_message.h"
#include "../ui/app_dialogs.h"
#include "../ui/chinese_text_context_menu.h"
#include "../ui/command_center_shell_access_factory.h"
#include "../ui/hub_function_workspace_controller.h"
#include "../ui/home_page_access_factory.h"
#include "../ui/hub_content_pages_controller.h"
#include "../ui/hub_navigation_controller.h"
#include "../ui/hub_home_page_controller.h"
#include "../ui/hub_page_host_controller.h"
#include "../ui/hub_page_router.h"
#include "../ui/hub_refresh_coordinator_action_factory.h"
#include "../ui/hub_refresh_coordinator_bundle.h"
#include "../ui/hub_settings_state.h"
#include "../ui/hub_utility_pages_controller.h"
#include "../ui/tab_bar_wheel_filter.h"
#include "../ui/ui_style.h"

#include <QtWidgets>
#include <QtConcurrent>
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

#include "../ui/floating_bar.h"


#include "../ui/command_center_shell.h"

// 主界面窗口：承载首页、历史、提示词、自定义功能、测试工具、设置和常见问题页面。
class HubWindowImpl : public HubWindow
{
public:
    explicit HubWindowImpl(
        const HubWindowAccess &settingsAccess,
        FloatingBar *floatingBar = nullptr,
        const std::function<void()> &onSettingsChanged = std::function<void()>(),
        const VocabularyAiCallback &onVocabularyAi = VocabularyAiCallback(),
        QWidget *parent = nullptr
    )
        : HubWindow(parent), m_settings(new HubSettingsState(settingsAccess)), m_floatingBar(floatingBar), m_onSettingsChanged(onSettingsChanged), m_onVocabularyAi(onVocabularyAi)
    {
        setWindowTitle(tr8("vocekit"));
        resize(1280, 820);
        setMinimumSize(1100, 720);
        setFont(appFont());

        HubPageRouter *pageRouter = pageHostController()->router();
        auto *commandShell = new CommandCenterShell(
            commandCenterShellAccess(),
            pageRouter
        );
        m_navigationController.reset(
            new HubNavigationController(
                pageRouter,
                commandShell,
                navigationControllerAccess()
            )
        );
        setCentralWidget(commandShell);
        initializeRefreshCoordinators();
        navigationController()->selectPage(QStringLiteral("home"));
    }

    void showSettingsPage(int initialTab = 0)
    {
        openSettingsDialog(initialTab);
    }

    QWidget *voiceControllerHostWidget() override
    {
        return this;
    }

    void showVoiceAssistantHub() override
    {
        showNormal();
        raise();
        activateWindow();
    }

    void notifyVocabularyChangedForVoiceController() override
    {
        notifyVocabularyChanged(QStringList(), true);
    }

    void notifySettingsChangedForVoiceController() override
    {
        notifySettingsChanged();
    }

    void openVocabularyEntryEditorForVoiceController(
        const VocabularyEntry &entry
    ) override
    {
        openVocabularyEntryEditor(entry);
    }

    void notifyHistoryRecordSavedForVoiceController(
        const QString &filePath
    ) override
    {
        notifyHistoryRecordSaved(filePath);
    }

    void initializeRefreshCoordinators()
    {
        HubRefreshUiActions uiActions;
        uiActions.refreshModeGrid = [this]() { refreshModeGrid(); };
        uiActions.refreshStatus = [this]() { refreshStatusLabels(); };
        uiActions.refreshPrompts = [this]() {
            utilityPagesController()->refreshPrompts();
        };
        uiActions.refreshFunctions = [this]() {
            refreshCustomFunctionsPage();
        };
        uiActions.refreshNavigation = [this]() {
            navigationController()->refreshFunctions();
        };
        uiActions.refreshActiveFunction = [this]() {
            refreshActiveFunctionPage();
        };
        uiActions.refreshOcr = [this]() {
            contentPagesController()->refreshOcrConfiguration();
        };
        uiActions.refreshRecentHistory = [this]() {
            refreshRecentHistoryPanel();
        };
        uiActions.refreshVocabulary = [this]() {
            refreshVocabularyPage();
        };
        uiActions.updateLogPagination = [this](
            const LogPaginationSnapshot &snapshot
        ) {
            utilityPagesController()->updateLogPagination(snapshot);
        };
        m_refreshCoordinators.reset(
            new HubRefreshCoordinatorBundle(
                createHubRefreshCoordinatorActions(
                    createHubRefreshDataAccess(
                        m_settings.data(),
                        contentPagesController()->historyRefreshDataAccess()
                    ),
                    uiActions
                )
            )
        );
    }

    void notifySettingsChanged(
        const QStringList &keys = QStringList(),
        const QStringList &functionIds = QStringList()
    )
    {
        if (m_onSettingsChanged) {
            m_onSettingsChanged();
            return;
        }
        m_refreshCoordinators->dispatchSettingsChanged(keys, functionIds);
    }

    void setApplicationEvents(ApplicationEvents *events)
    {
        m_refreshCoordinators->setApplicationEvents(events);
    }

    void openVocabularyEntryEditor(const VocabularyEntry &entry = VocabularyEntry())
    {
        navigationController()->selectPage(QStringLiteral("vocabulary"));
        contentPagesController()->editVocabularyEntry(entry);
    }

    void refreshVocabularyPage()
    {
        contentPagesController()->refreshVocabulary();
    }

    void notifyVocabularyChanged(
        const QStringList &entryIds = QStringList(),
        bool resetRequired = false
    )
    {
        m_refreshCoordinators->dispatchVocabularyChanged(
            entryIds,
            resetRequired
        );
    }

    void notifyHistoryRecordSaved(const QString &filePath)
    {
        m_refreshCoordinators->dispatchHistoryChanged(
            QStringList() << filePath,
            false
        );
    }

    void openFaqById(const QString &faqId)
    {
        utilityPagesController()->openFaq(faqId);
        showNormal();
        raise();
        activateWindow();
    }

private:
    QScopedPointer<HubSettingsState> m_settings;
    QScopedPointer<HubRefreshCoordinatorBundle> m_refreshCoordinators;
    FloatingBar *m_floatingBar = nullptr;
    std::function<void()> m_onSettingsChanged;
    VocabularyAiCallback m_onVocabularyAi;
    QScopedPointer<HubHomePageController> m_homePageController;
    QScopedPointer<HubNavigationController> m_navigationController;
    QScopedPointer<HubPageHostController> m_pageHostController;
    QScopedPointer<HubUtilityPagesController> m_utilityPagesController;
    QScopedPointer<HubContentPagesController> m_contentPagesController;
    QScopedPointer<HubFunctionWorkspaceController> m_functionWorkspaceController;
protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (m_settings && m_settings->trayResident()) {
            hide();
            event->ignore();
        } else {
            event->accept();
            qApp->quit();
        }
    }

private:

    HubNavigationControllerAccess navigationControllerAccess()
    {
        HubNavigationControllerAccess access;
        access.currentFunctionId = [this]() {
            return functionWorkspaceController()->currentFunctionId();
        };
        access.setCurrentFunctionId = [this](const QString &id) {
            return functionWorkspaceController()->selectFunction(id);
        };
        access.clearCurrentFunction = [this]() {
            functionWorkspaceController()->clearFunction();
        };
        access.addFunction = [this]() {
            return functionWorkspaceController()->addFunction();
        };
        return access;
    }

    HubNavigationController *navigationController() const
    {
        return m_navigationController.data();
    }

    CommandCenterShellAccess commandCenterShellAccess()
    {
        CommandCenterShellAccessFactoryDependencies dependencies;
        dependencies.settings = m_settings.data();
        dependencies.openFunction = [this](const QString &id) {
            if (m_navigationController) {
                navigationController()->openFunction(id);
            }
        };
        dependencies.openTool = [this](const QString &id) {
            if (m_navigationController) {
                navigationController()->openTool(id);
            }
        };
        dependencies.addFunction = [this]() {
            if (m_navigationController) {
                navigationController()->addFunction();
            }
        };
        dependencies.searchMissed = [this](const QString &keyword) {
            showAttentionInformation(
                this,
                tr8("没有找到"),
                tr8("没有找到与“") + keyword + tr8("”匹配的功能或页面。")
            );
        };
        return createCommandCenterShellAccess(dependencies);
    }

    HubUtilityPagesController *utilityPagesController()
    {
        if (!m_utilityPagesController) {
            HubUtilityPagesControllerAccess access;
            access.settings = m_settings.data();
            access.floatingBar = m_floatingBar;
            access.popupFallbackParent = this;
            access.notifySettingsChanged = [this]() {
                notifySettingsChanged();
            };
            access.selectPage = [this](const QString &id) {
                if (m_navigationController) {
                    navigationController()->selectPage(id);
                }
            };
            m_utilityPagesController.reset(
                new HubUtilityPagesController(access)
            );
        }
        return m_utilityPagesController.data();
    }

    HubContentPagesController *contentPagesController()
    {
        if (!m_contentPagesController) {
            HubContentPagesControllerAccess access;
            access.settings = m_settings.data();
            access.pageParent = this;
            access.dialogParent = this;
            access.vocabularyAi = m_onVocabularyAi;
            access.historyChanged = [this](
                const QStringList &recordIds,
                bool resetRequired
            ) {
                m_refreshCoordinators->dispatchHistoryChanged(
                    recordIds,
                    resetRequired
                );
            };
            access.vocabularyChanged = [this](
                const QStringList &entryIds,
                bool resetRequired
            ) {
                notifyVocabularyChanged(entryIds, resetRequired);
            };
            access.historyRecordSaved = [this](const QString &filePath) {
                notifyHistoryRecordSaved(filePath);
            };
            m_contentPagesController.reset(
                new HubContentPagesController(access)
            );
        }
        return m_contentPagesController.data();
    }

    HubPageHostControllerAccess pageHostControllerAccess()
    {
        HubPageHostControllerAccess dependencies;
        dependencies.homePage = [this]() {
            return homePageController()->page();
        };
        dependencies.functionPage = [this]() { return commandFunctionPage(); };
        dependencies.historyPage = [this]() {
            return contentPagesController()->historyPage();
        };
        dependencies.vocabularyPage = [this]() {
            return contentPagesController()->vocabularyPage();
        };
        dependencies.ocrPage = [this]() {
            return contentPagesController()->ocrPage();
        };
        dependencies.promptsPage = [this]() {
            return utilityPagesController()->promptsPage();
        };
        dependencies.diagnosticsPage = [this]() {
            return utilityPagesController()->diagnosticsPage();
        };
        dependencies.logsPage = [this]() {
            return utilityPagesController()->logsPage();
        };
        dependencies.settingsPage = [this]() {
            return utilityPagesController()->settingsPage();
        };
        dependencies.faqPage = [this]() {
            return utilityPagesController()->faqPage();
        };

        dependencies.deferredContext = this;
        dependencies.refreshHistory = [this]() {
            contentPagesController()->refreshHistory();
        };
        dependencies.refreshVocabulary = [this]() {
            contentPagesController()->refreshVocabulary();
        };
        dependencies.refreshOcr = [this]() {
            contentPagesController()->refreshOcrPage();
        };
        dependencies.refreshPrompts = [this]() {
            utilityPagesController()->refreshPrompts();
        };
        dependencies.refreshDiagnostics = [this]() {
            utilityPagesController()->refreshDiagnostics();
        };
        dependencies.refreshLogs = [this]() {
            utilityPagesController()->refreshLogs(true);
        };
        dependencies.refreshSettings = [this]() {
            utilityPagesController()->refreshSettings();
        };

        return dependencies;
    }

    HubPageHostController *pageHostController()
    {
        if (!m_pageHostController) {
            m_pageHostController.reset(
                new HubPageHostController(
                    pageHostControllerAccess(),
                    this
                )
            );
        }
        return m_pageHostController.data();
    }

    HubHomePageController *homePageController()
    {
        if (!m_homePageController) {
            HomePageAccessDependencies dependencies;
            dependencies.settings = m_settings.data();
            dependencies.editFunction = [this](
                const QString &id,
                const QString &title,
                bool custom,
                const CustomFunctionDef &customFunction
            ) {
                functionWorkspaceController()->editFunction(
                    id,
                    title,
                    custom,
                    customFunction
                );
            };
            dependencies.settingsChanged = [this]() {
                notifySettingsChanged();
            };
            dependencies.showWarning = [this](
                const QString &title,
                const QString &message
            ) {
                showAttentionWarning(this, title, message);
            };
            dependencies.recentEntries = [this]() {
                return contentPagesController()->historyEntries();
            };
            dependencies.historyTabs = [this]() {
                return contentPagesController()->historyTabs();
            };
            dependencies.recentListFactory = [this](
                const QString &modeId,
                const QVector<HistoryEntry> &entries,
                int maxRows
            ) {
                return contentPagesController()->historyViewForMode(
                    modeId,
                    entries,
                    maxRows
                );
            };
            m_homePageController.reset(
                new HubHomePageController(
                    createHomePageAccess(dependencies),
                    this
                )
            );
        }
        return m_homePageController.data();
    }

    QWidget *commandFunctionPage()
    {
        return functionWorkspaceController()->page();
    }

    void refreshActiveFunctionPage()
    {
        if (m_functionWorkspaceController) {
            m_functionWorkspaceController->refreshActivePage();
        }
    }



    HubFunctionWorkspaceController *functionWorkspaceController()
    {
        if (!m_functionWorkspaceController) {
            HubFunctionWorkspaceControllerAccess workspaceAccess;
            workspaceAccess.settings = m_settings.data();
            workspaceAccess.prompts =
                utilityPagesController()->promptSettingsAccess();
            workspaceAccess.pageParent = this;
            workspaceAccess.dialogParent = this;
            workspaceAccess.saveSettings = [this]() { saveHubSettings(); };
            m_functionWorkspaceController.reset(
                new HubFunctionWorkspaceController(workspaceAccess)
            );
        }
        return m_functionWorkspaceController.data();
    }

    void openSettingsDialog(int initialTab = 0)
    {
        utilityPagesController()->openSettings(initialTab);
        showNormal();
        raise();
        activateWindow();
    }

    void saveHubSettings()
    {
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
            return;
        }
        notifySettingsChanged();
    }



























    // 历史记录工具：按当前标签页和搜索条件筛选记录，并负责备份、导入和三种导出。








































    void refreshCustomFunctionsPage()
    {
        if (m_functionWorkspaceController) {
            m_functionWorkspaceController->refreshManagementPage();
        }
    }

    void refreshModeGrid()
    {
        homePageController()->refreshFunctionModes();
    }
    void refreshRecentHistoryPanel()
    {
        homePageController()->refreshRecentHistory();
    }

    void refreshStatusLabels()
    {
        homePageController()->refreshCurrentStatus();
    }
};

HubWindow::HubWindow(QWidget *parent)
    : QMainWindow(parent)
{
}

HubWindow::~HubWindow() = default;

HubWindow *createHubWindow(
    const HubWindowAccess &settingsAccess,
    FloatingBar *floatingBar,
    const std::function<void()> &onSettingsChanged,
    const VocabularyAiCallback &onVocabularyAi,
    QWidget *parent)
{
    return new HubWindowImpl(
        settingsAccess,
        floatingBar,
        onSettingsChanged,
        onVocabularyAi,
        parent
    );
}
