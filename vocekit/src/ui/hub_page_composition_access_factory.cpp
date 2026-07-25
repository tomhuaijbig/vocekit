#include "hub_page_composition_access_factory.h"

#include <QTimer>

namespace {

HubPageActivation immediateActivation(const std::function<void()> &action)
{
    return [action](bool) {
        if (action) {
            action();
        }
    };
}

} // namespace

HubPageCompositionAccess createHubPageCompositionAccess(
    const HubPageCompositionAccessFactoryDependencies &dependencies
)
{
    HubPageCompositionAccess access;
    access.homePage = dependencies.homePage;
    access.functionPage = dependencies.functionPage;
    access.historyPage = dependencies.historyPage;
    access.vocabularyPage = dependencies.vocabularyPage;
    access.ocrPage = dependencies.ocrPage;
    access.promptsPage = dependencies.promptsPage;
    access.diagnosticsPage = dependencies.diagnosticsPage;
    access.logsPage = dependencies.logsPage;
    access.settingsPage = dependencies.settingsPage;
    access.faqPage = dependencies.faqPage;

    QObject *deferredContext = dependencies.deferredContext;
    const std::function<void()> refreshHistory = dependencies.refreshHistory;
    access.historyActivated = [deferredContext, refreshHistory](bool) {
        if (!refreshHistory) {
            return;
        }
        if (deferredContext) {
            QTimer::singleShot(0, deferredContext, refreshHistory);
            return;
        }
        refreshHistory();
    };
    access.vocabularyActivated = immediateActivation(
        dependencies.refreshVocabulary
    );
    access.ocrActivated = immediateActivation(dependencies.refreshOcr);
    access.promptsActivated = immediateActivation(dependencies.refreshPrompts);
    access.diagnosticsActivated = immediateActivation(
        dependencies.refreshDiagnostics
    );
    access.logsActivated = immediateActivation(dependencies.refreshLogs);

    const std::function<void()> refreshSettings = dependencies.refreshSettings;
    access.settingsActivated = [refreshSettings](bool pageChanged) {
        if (pageChanged && refreshSettings) {
            refreshSettings();
        }
    };
    return access;
}
