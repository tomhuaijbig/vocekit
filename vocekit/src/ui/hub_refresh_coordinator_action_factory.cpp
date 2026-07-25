#include "hub_refresh_coordinator_action_factory.h"

HubRefreshCoordinatorBundleActions createHubRefreshCoordinatorActions(
    const HubRefreshDataAccess &data,
    const HubRefreshUiActions &ui
)
{
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = data.reloadSettings;
    actions.settings.refreshModeGrid = ui.refreshModeGrid;
    actions.settings.refreshStatus = ui.refreshStatus;
    actions.settings.refreshPrompts = ui.refreshPrompts;
    actions.settings.refreshFunctions = ui.refreshFunctions;
    actions.settings.refreshNavigation = ui.refreshNavigation;
    actions.settings.refreshActiveFunction = ui.refreshActiveFunction;
    actions.settings.refreshOcr = ui.refreshOcr;
    actions.settings.historyCacheValid = data.historyCacheValid;
    actions.settings.refreshRecentHistory = ui.refreshRecentHistory;

    if (data.settingsSnapshot && ui.updateLogPagination) {
        const std::function<AppSettingsData()> settingsSnapshot =
            data.settingsSnapshot;
        const std::function<void(const LogPaginationSnapshot &)> updateLogs =
            ui.updateLogPagination;
        actions.settings.refreshLogs = [settingsSnapshot, updateLogs]() {
            updateLogs(buildLogPaginationSnapshot(settingsSnapshot()));
        };
    }
    if (data.refreshHistory) {
        const std::function<void(bool)> refreshHistory = data.refreshHistory;
        actions.settings.refreshHistory = [refreshHistory]() {
            refreshHistory(false);
        };
    }

    actions.content.invalidateHistoryCache = data.invalidateHistoryCache;
    actions.content.refreshRecentHistory = ui.refreshRecentHistory;
    actions.content.historyPageCreated = data.historyPageCreated;
    actions.content.refreshHistory = data.refreshHistory;
    actions.content.refreshVocabulary = ui.refreshVocabulary;
    actions.content.refreshActiveFunction = ui.refreshActiveFunction;
    return actions;
}
