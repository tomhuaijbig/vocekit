#include "hub_utility_pages_controller.h"

#include "diagnostics_page_factory.h"
#include "faq_panel.h"
#include "hub_settings_state.h"
#include "log_pagination_snapshot.h"
#include "logs_panel.h"
#include "prompt_access_factory.h"
#include "prompts_panel.h"
#include "settings_panel.h"
#include "settings_panel_access_factory.h"

#include "../config/app_paths.h"
#include "../config/secret_config.h"
#include "../storage/vocabulary_store.h"

#include <QPointer>
#include <QWidget>

class HubUtilityPagesController::Impl
{
public:
    explicit Impl(const HubUtilityPagesControllerAccess &controllerAccess)
        : access(controllerAccess)
    {
    }

    HubUtilityPagesControllerAccess access;
    QPointer<PromptsPanel> promptsPanel;
    QPointer<DiagnosticsPanel> diagnosticsPanel;
    QPointer<LogsPanel> logsPanel;
    QPointer<SettingsPanel> settingsPanel;
    QPointer<FaqPanel> faqPanel;
};

HubUtilityPagesController::HubUtilityPagesController(
    const HubUtilityPagesControllerAccess &access
)
    : m_impl(new Impl(access))
{
}

HubUtilityPagesController::~HubUtilityPagesController() = default;

QWidget *HubUtilityPagesController::promptsPage()
{
    if (m_impl->promptsPanel) {
        return m_impl->promptsPanel.data();
    }

    PromptAccessFactoryDependencies dependencies;
    dependencies.settings = m_impl->access.settings;
    const PromptAccessAssembly prompts = createPromptAccessAssembly(dependencies);
    m_impl->promptsPanel = new PromptsPanel(
        prompts.panel,
        [this]() {
            if (m_impl->access.notifySettingsChanged) {
                m_impl->access.notifySettingsChanged();
            }
        }
    );
    return m_impl->promptsPanel.data();
}

QWidget *HubUtilityPagesController::diagnosticsPage()
{
    if (m_impl->diagnosticsPanel) {
        return m_impl->diagnosticsPanel.data();
    }

    DiagnosticsPageFactoryDependencies dependencies;
    dependencies.settings = m_impl->access.settings;
    dependencies.floatingBar = m_impl->access.floatingBar;
    dependencies.popupFallbackParent = m_impl->access.popupFallbackParent;
    dependencies.faqMatchCounter = [this](const QString &keyword) {
        return m_impl->faqPanel
            ? m_impl->faqPanel->matchCount(keyword)
            : 0;
    };
    dependencies.faqOpener = [this](const QString &keyword) {
        openFaq(keyword);
    };
    dependencies.appBasePathProvider = []() { return appBasePath(); };
    dependencies.vocabularyStorePathProvider = []() {
        return vocabularyStorePath();
    };
    dependencies.secretsProvider = []() { return loadSecrets(); };
    m_impl->diagnosticsPanel = createDiagnosticsPanel(dependencies);
    return m_impl->diagnosticsPanel.data();
}

QWidget *HubUtilityPagesController::logsPage()
{
    if (m_impl->logsPanel) {
        return m_impl->logsPanel.data();
    }

    const AppSettingsData settings = m_impl->access.settings
        ? m_impl->access.settings->toData()
        : AppSettingsData();
    m_impl->logsPanel = new LogsPanel(buildLogPaginationSnapshot(settings));
    return m_impl->logsPanel.data();
}

QWidget *HubUtilityPagesController::settingsPage()
{
    if (m_impl->settingsPanel) {
        return m_impl->settingsPanel.data();
    }

    SettingsPanelAccessFactoryDependencies dependencies;
    dependencies.settings = m_impl->access.settings;
    dependencies.notifySettingsChanged = m_impl->access.notifySettingsChanged;
    const SettingsPanelAssembly assembly =
        createSettingsPanelAssembly(dependencies);
    m_impl->settingsPanel = new SettingsPanel(
        assembly.access,
        assembly.onChanged,
        m_impl->access.popupFallbackParent
    );
    return m_impl->settingsPanel.data();
}

QWidget *HubUtilityPagesController::faqPage()
{
    if (m_impl->faqPanel) {
        return m_impl->faqPanel.data();
    }

    m_impl->faqPanel = new FaqPanel([this](const QString &keyword) {
        openDiagnostics(keyword);
    });
    return m_impl->faqPanel.data();
}

PromptSettingsAccess HubUtilityPagesController::promptSettingsAccess() const
{
    PromptAccessFactoryDependencies dependencies;
    dependencies.settings = m_impl->access.settings;
    return createPromptAccessAssembly(dependencies).settings;
}

void HubUtilityPagesController::refreshPrompts()
{
    if (m_impl->promptsPanel) {
        m_impl->promptsPanel->refresh();
    }
}

void HubUtilityPagesController::refreshDiagnostics()
{
    if (m_impl->diagnosticsPanel) {
        m_impl->diagnosticsPanel->refreshRuntimeTargets();
    }
}

void HubUtilityPagesController::refreshLogs(bool reloadFromDisk)
{
    if (!m_impl->logsPanel) {
        return;
    }

    const AppSettingsData settings = m_impl->access.settings
        ? m_impl->access.settings->toData()
        : AppSettingsData();
    m_impl->logsPanel->setPaginationSnapshot(
        buildLogPaginationSnapshot(settings)
    );
    m_impl->logsPanel->reload(reloadFromDisk);
}

void HubUtilityPagesController::refreshSettings()
{
    if (m_impl->settingsPanel) {
        m_impl->settingsPanel->refreshFromSettings();
    }
}

void HubUtilityPagesController::updateLogPagination(
    const LogPaginationSnapshot &snapshot
)
{
    if (m_impl->logsPanel) {
        m_impl->logsPanel->setPaginationSnapshot(snapshot);
    }
}

void HubUtilityPagesController::openSettings(int tabIndex)
{
    if (m_impl->access.selectPage) {
        m_impl->access.selectPage(QStringLiteral("settings"));
    }
    refreshSettings();
    if (m_impl->settingsPanel) {
        m_impl->settingsPanel->setCurrentTab(tabIndex);
    }
}

void HubUtilityPagesController::openFaq(const QString &faqId)
{
    if (m_impl->access.selectPage) {
        m_impl->access.selectPage(QStringLiteral("faq"));
    }
    if (m_impl->faqPanel) {
        m_impl->faqPanel->showFaqId(faqId);
    }
}

void HubUtilityPagesController::openDiagnostics(const QString &keyword)
{
    if (m_impl->access.selectPage) {
        m_impl->access.selectPage(QStringLiteral("diagnostics"));
    }
    if (m_impl->diagnosticsPanel) {
        m_impl->diagnosticsPanel->setSearchText(keyword);
    }
}
